#include "../../stdafx.h"

#include "ItemSystem.hpp"
#include "../VIDRegistry.hpp"

#include "../../utils.h"
#include "../../config.h"
#include "../../char.h"
#include "../../char_manager.h"
#include "../../item_manager.h"
#include "../../desc.h"
#include "../../desc_client.h"
#include "../../desc_manager.h"
#include "../../packet.h"
#include "../../protocol.h"
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
#include "../../MountInventory.h"
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
#include "../../item_addon.h"
#ifdef __NEWPET_SYSTEM__
#include "../../New_PetSystem.h"
#define __NEWPET_SYSTEM_CHECK
#endif
#ifdef __PET_SYSTEM__
#include "../../PetSystem.h"
#endif
#ifdef ENABLE_NEWSTUFF
#include "../../pvp.h"
#endif
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
#include "../../RuneDungeon.h"
#include "../../Halloween2022Dungeon.h"
#include "../../VikingDungeon.h"
#endif
#ifdef ENABLE_STOLE_COSTUME
#include "../../../common/stole_length.h"
#endif
#include "../../../common/CommonDefines.h"
#ifdef ENABLE_RUNE_SYSTEM
#include "../../../common/rune_length.h"
#endif

#include "../Registry.hpp"
#include "../AIHelpers.hpp"
#include "../SpatialHelpers.hpp"
#include "../components/identity_components.hpp"
#include "../components/inventory_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"
#include "../components/item_components.hpp"
#include "../ItemRegistry.hpp"

bool IS_SUMMONABLE_ZONE(int map_index);
bool IS_BOTARYABLE_ZONE(int nMapIndex);
extern int stone_chance;

namespace {

const int ITEM_BROKEN_METIN_VNUM = 28960;

using LegacyCharHandle = decltype(std::declval<ecs::LegacyCharPtr>().ptr);

struct FFindStone
{
	std::map<uint32_t, LegacyCharHandle> m_mapStone;

	void operator()(LPENTITY pEnt)
	{
		if (pEnt->IsType(ENTITY_CHARACTER) == true)
		{
			auto* pChar = static_cast<LegacyCharHandle>(pEnt);

			if (pChar->IsStone() == true)
			{
				m_mapStone[pChar->GetPacketVID()] = pChar;
			}
		}
	}
};

LegacyCharHandle LegacyCharOf(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e);
    return legacy ? legacy->ptr : nullptr;
}

#ifdef ENABLE_EXTRA_INVENTORY
static ecs::ExtraInventoryRuntimeComponent* EnsureExtraInventoryRuntimeComponent(LPCHARACTER ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return &g_registry.emplace_or_replace<ecs::ExtraInventoryRuntimeComponent>(e);
}
#endif

static entt::entity ItemEntityOf(LPITEM item)
{
    if (!item || item->GetID() == 0)
        return entt::null;

    return CItemRegistry::Instance().Find(item->GetID());
}

static uint32_t ItemVnumOrLegacy(LPITEM item)
{
    if (!item)
        return 0;

    entt::entity e = ItemEntityOf(item);
    if (e != entt::null)
    {
        if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(e))
            return identity->vnum;
    }

    return item->GetVnum();
}

static void SyncItemCountComponent(LPITEM item, int count)
{
    entt::entity e = ItemEntityOf(item);
    if (e == entt::null)
        return;

    g_registry.emplace_or_replace<ecs::ItemCount>(e, ecs::ItemCount{count});
}

static void SyncItemFlagsComponent(LPITEM item)
{
    entt::entity e = ItemEntityOf(item);
    if (e == entt::null)
        return;

    ecs::ItemFlags flags{};
    flags.flags = item->GetFlag();
    flags.exchanging = item->IsExchanging();
    flags.skipSave = item->GetSkipSave();
    flags.isLocked = item->isLocked();
    g_registry.emplace_or_replace<ecs::ItemFlags>(e, flags);
}

#ifndef ENABLE_SWITCHBOT
const int MAX_NORM_ATTR_NUM = ITEM_MANAGER::MAX_NORM_ATTR_NUM;
const int MAX_RARE_ATTR_NUM = ITEM_MANAGER::MAX_RARE_ATTR_NUM;
#endif

static void SyncItemAttributesComponent(LPITEM item)
{
    entt::entity e = ItemEntityOf(item);
    if (e == entt::null)
        return;

    ecs::ItemAttributes attrs{};
    const TPlayerItemAttribute* values = item->GetAttributes();
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        attrs.attrs[i] = values[i];

    g_registry.emplace_or_replace<ecs::ItemAttributes>(e, attrs);
}

static void SyncItemSocketsComponent(LPITEM item)
{
    entt::entity e = ItemEntityOf(item);
    if (e == entt::null)
        return;

    ecs::ItemSockets sockets{};
    const int32_t* values = item->GetSockets();
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        sockets.sockets[i] = values[i];

    g_registry.emplace_or_replace<ecs::ItemSockets>(e, sockets);
}

namespace
{
	bool IsZodiacAttributeItemVnum(uint32_t dwVnum)
	{
#ifdef DISABLE_ZODIAC_ATT
		return (dwVnum == 12314141);
#else
		return
			((dwVnum >= 19290) && (dwVnum <= 19312)) ||
			((dwVnum >= 19490) && (dwVnum <= 19512)) ||
			((dwVnum >= 19690) && (dwVnum <= 19712)) ||
			((dwVnum >= 19890) && (dwVnum <= 19912)) ||
			((dwVnum >= 300) && (dwVnum <= 319)) ||
			(dwVnum == 329) ||
			(dwVnum == 339) ||
			(dwVnum == 349) ||
			(dwVnum == 359) ||
			(dwVnum == 369) ||
			(dwVnum == 379) ||
			(dwVnum == 389) ||
			(dwVnum == 399) ||
			((dwVnum >= 1180) && (dwVnum <= 1189)) ||
			(dwVnum == 1199) ||
			(dwVnum == 1209) ||
			(dwVnum == 1219) ||
			(dwVnum == 1229) ||
			((dwVnum >= 2200) && (dwVnum <= 2209)) ||
			(dwVnum == 2219) ||
			(dwVnum == 2229) ||
			(dwVnum == 2239) ||
			(dwVnum == 2249) ||
			((dwVnum >= 3220) && (dwVnum <= 3229)) ||
			(dwVnum == 3239) ||
			(dwVnum == 3249) ||
			(dwVnum == 3259) ||
			(dwVnum == 3269) ||
			((dwVnum >= 5160) && (dwVnum <= 5169)) ||
			(dwVnum == 5179) ||
			(dwVnum == 5189) ||
			(dwVnum == 5199) ||
			(dwVnum == 5209) ||
			((dwVnum >= 7300) && (dwVnum <= 7309)) ||
			(dwVnum == 7319) ||
			(dwVnum == 7329) ||
			(dwVnum == 7339) ||
			(dwVnum == 7349) ||
			((dwVnum >= 1700) && (dwVnum <= 1713)) ||
			((dwVnum >= 1720) && (dwVnum <= 1733)) ||
			((dwVnum >= 1740) && (dwVnum <= 1753)) ||
			((dwVnum >= 1760) && (dwVnum <= 1773)) ||
			((dwVnum >= 1780) && (dwVnum <= 1793)) ||
			((dwVnum >= 1800) && (dwVnum <= 1813)) ||
			((dwVnum >= 8500) && (dwVnum <= 8839));
#endif
	}
}

bool IsExtraEnchantUseSubtype(uint8_t subtype)
{
	switch (subtype)
	{
	case USE_CHANGE_ATTRIBUTE:
	case USE_ADD_ATTRIBUTE:
	case USE_ADD_ATTRIBUTE2:
	case USE_CHANGE_ATTRIBUTE2:
	case USE_CHANGE_COSTUME_ATTR:
	case USE_RESET_COSTUME_ATTR:
	case USE_CHANGE_ATTRIBUTE_PLUS:
#ifdef ATTR_LOCK
	case USE_ADD_ATTRIBUTE_LOCK:
	case USE_CHANGE_ATTRIBUTE_LOCK:
	case USE_DELETE_ATTRIBUTE_LOCK:
#endif
#ifdef ENABLE_ATTR_COSTUMES
	case USE_CHANGE_ATTR_COSTUME:
	case USE_ADD_ATTR_COSTUME1:
	case USE_ADD_ATTR_COSTUME2:
	case USE_REMOVE_ATTR_COSTUME:
#endif
#ifdef ENABLE_DS_ENCHANT
	case USE_DS_ENCHANT:
	case USE_ENCHANT_STOLE:
#endif
		return true;
	}

	return false;
}

bool IsExtraPotionUseSubtype(uint8_t subtype)
{
	switch (subtype)
	{
	case USE_POTION:
	case USE_POTION_NODELAY:
	case USE_POTION_CONTINUE:
	case USE_ABILITY_UP:
	case USE_AFFECT:
#ifdef ENABLE_NEW_USE_POTION
	case USE_NEW_POTIION:
#endif
		return true;
	}

	return false;
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
	case 22020:
	case ITEM_MARRIAGE_RING:
		return true;
	}

	return false;
}


static bool FN_check_item_socket(LPITEM item)
{
#ifdef ENABLE_NEW_USE_POTION
	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_NEW_POTIION)
	{
		// inactive new potionok stackelhetnek
		// active példány (socket1 != 0) ne stackeljen vissza
		return item->GetSocket(1) == 0;
	}
#endif

	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		if (item->GetSocket(i) != item->GetProto()->alSockets[i])
			return false;
	}

	return true;
}

// item socket º¹»ç -- by mhh

static bool FN_check_item_sex(LegacyCharHandle ch, LPITEM item)
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

EVENTFUNC(item_destroy_event);
EVENTFUNC(unique_expire_event);
EVENTFUNC(timer_based_on_wear_expire_event);
EVENTFUNC(real_time_expire_event);
EVENTFUNC(accessory_socket_expire_event);
EVENTFUNC(soul_item_event);

namespace ItemSystem {

LPITEM GetItem(entt::entity e, TItemPos cell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->GetItem(cell) : nullptr;
}

LPITEM GetInventoryItem(entt::entity e, uint16_t cell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->GetInventoryItem(cell) : nullptr;
}

#ifdef ENABLE_EXTRA_INVENTORY
LPITEM GetExtraInventoryItem(entt::entity e, uint16_t cell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->GetExtraInventoryItem(cell) : nullptr;
}

void SyncExtraInventoryAll(LPCHARACTER ch)
{
    auto* comp = EnsureExtraInventoryRuntimeComponent(ch);
    if (!comp)
        return;

    std::memcpy(comp->pItems, ch->m_pointsInstant.pExtraItems, sizeof(comp->pItems));
    std::memcpy(comp->wItemGrid, ch->m_pointsInstant.wExtraItemGrid, sizeof(comp->wItemGrid));
}
#endif

LPITEM FindSpecifyItem(entt::entity e, uint32_t vnum
#ifdef ENABLE_EXTRA_INVENTORY
                       , bool reinforce
#endif
)
{
    auto* ch = LegacyCharOf(e);
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
    auto* ch = LegacyCharOf(e);
    return ch ? ch->FindItemByID(id) : nullptr;
}

int CountItemRenewal(entt::entity e, uint32_t vnum)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->CountSpecifyItemRenewal(vnum) : 0;
}

int CountItem(entt::entity e, uint32_t vnum)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->CountSpecifyItem(vnum) : 0;
}

int CountTypeItem(entt::entity e, uint8_t type)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->CountSpecifyTypeItem(type) : 0;
}

bool HasItem(entt::entity e, uint32_t vnum, uint32_t count)
{
    return CountItem(e, vnum) >= static_cast<int>(count);
}

LPITEM GetWearItem(entt::entity e, uint8_t wearPos)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->GetWear(wearPos) : nullptr;
}

void SetWearItem(entt::entity e, uint8_t wearPos, LPITEM item)
{
    auto* ch = LegacyCharOf(e);
    if (ch)
        ch->SetWear(wearPos, item);
}

bool UnequipItem(entt::entity e, LPITEM item)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->UnequipItem(item) : false;
}

bool EquipItem(entt::entity e, LPITEM item, int candidateCell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->EquipItem(item, candidateCell) : false;
}

bool IsEquipUniqueItem(entt::entity e, uint32_t itemVnum)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->IsEquipUniqueItem(itemVnum) : false;
}

bool IsEquipUniqueGroup(entt::entity e, uint32_t groupVnum)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->IsEquipUniqueGroup(groupVnum) : false;
}

bool UnEquipSpecialRideUniqueItem(entt::entity e)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->UnEquipSpecialRideUniqueItem() : false;
}

bool CanEquipNow(entt::entity e, const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->CanEquipNow(item, srcCell, destCell) : false;
}

bool CanUnequipNow(entt::entity e, const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell)
{
    auto* ch = LegacyCharOf(e);
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
    auto* ch = LegacyCharOf(e);
    return ch ? ch->DropItem(cell, count) : false;
}

bool DropGold(entt::entity e, int64_t gold)
{
    auto* ch = LegacyCharOf(e);
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
    auto* ch = LegacyCharOf(e);
    return ch ? ch->MoveItem(fromCell, toCell, count) : false;
}

bool PickupItem(entt::entity e, uint32_t vid)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->PickupItem(vid) : false;
}

bool UseItem(entt::entity e, TItemPos cell, TItemPos destCell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->UseItem(cell, destCell) : false;
}

bool UseItemEx(entt::entity e, LPITEM item, TItemPos destCell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->UseItemEx(item, destCell) : false;
}


void RemoveTypeItem(entt::entity e, uint8_t type, int count)
{
    if (auto* ch = LegacyCharOf(e))
        ch->RemoveSpecifyTypeItem(type, count);
}

void AutoGiveItem(entt::entity e, LPITEM item, bool longOwnerShip
#ifdef __HIGHLIGHT_SYSTEM__
                  , bool isHighLight
#endif
)
{
    if (auto* ch = LegacyCharOf(e))
        ch->AutoGiveItem(item, longOwnerShip
#ifdef __HIGHLIGHT_SYSTEM__
                         , isHighLight
#endif
        );
}

#ifdef ENABLE_DS_REFINE_ALL
bool AutoGiveDS(entt::entity e, LPITEM item, bool longOwnerShip)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->AutoGiveDS(item, longOwnerShip) : false;
}
#endif

LPITEM AutoGiveItem(entt::entity e, uint32_t itemVnum,
#ifdef ENABLE_NEW_STACK_LIMIT
                    int
#else
                    uint8_t
#endif
                        count, int rarePct, bool sendMessage
#ifdef __HIGHLIGHT_SYSTEM__
                    , bool isHighLight
#endif
)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->AutoGiveItem(itemVnum, count, rarePct, sendMessage
#ifdef __HIGHLIGHT_SYSTEM__
                                 , isHighLight
#endif
    ) : nullptr;
}

bool GiveItem(entt::entity from, entt::entity victim, TItemPos cell)
{
    auto* fromCh = LegacyCharOf(from);
    auto* victimCh = LegacyCharOf(victim);
    return (fromCh && victimCh) ? fromCh->GiveItem(victimCh, cell) : false;
}

bool CanReceiveItem(entt::entity receiver, entt::entity from, LPITEM item)
{
    auto* receiverCh = LegacyCharOf(receiver);
    auto* fromCh = LegacyCharOf(from);
    return (receiverCh && fromCh) ? receiverCh->CanReceiveItem(fromCh, item) : false;
}

void ReceiveItem(entt::entity receiver, entt::entity from, LPITEM item)
{
    auto* receiverCh = LegacyCharOf(receiver);
    auto* fromCh = LegacyCharOf(from);
    if (receiverCh && fromCh)
        receiverCh->ReceiveItem(fromCh, item);
}

bool GiveItemFromSpecialItemGroup(entt::entity e, uint32_t groupNum,
                                  std::vector<uint32_t>& itemVnums,
                                  std::vector<uint32_t>& itemCounts,
                                  std::vector<LPITEM>& itemGets,
                                  int& count)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->GiveItemFromSpecialItemGroup(groupNum, itemVnums, itemCounts, itemGets, count) : false;
}

bool DestroyItem(entt::entity e, TItemPos cell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->DestroyItem(cell) : false;
}

void ItemDivision(entt::entity e, TItemPos cell)
{
    (void)e;
    (void)cell;
}

void SetRefineNPC(entt::entity e, entt::entity npc)
{
    if (auto* ch = LegacyCharOf(e))
        ch->SetRefineNPC(LegacyCharOf(npc));
}

bool DoRefine(entt::entity e, LPITEM item, bool moneyOnly)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->DoRefine(item, moneyOnly) : false;
}

bool DoRefineWithScroll(entt::entity e, LPITEM item)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->DoRefineWithScroll(item) : false;
}

bool DoRefineItemSoul(entt::entity e, LPITEM item)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->DoRefineItemSoul(item) : false;
}

bool RefineInformation(entt::entity e, uint8_t cell, uint8_t type, int additionalCell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->RefineInformation(cell, type, additionalCell) : false;
}

bool RefineItem(entt::entity e, LPITEM item, LPITEM target)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->RefineItem(item, target) : false;
}

void UseSilkBotary(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e))
        ch->UseSilkBotary();
}

void SetRefineMode(entt::entity e, int additionalCell)
{
    if (auto* ch = LegacyCharOf(e))
        ch->SetRefineMode(additionalCell);
}

void ClearRefineMode(entt::entity e)
{
    if (auto* ch = LegacyCharOf(e))
        ch->ClearRefineMode();
}

} // namespace ItemSystem

bool CItem::IsNewMountItem()
{
	switch (GetVnum())
	{
	case 76000: case 76001: case 76002: case 76003:
	case 76004: case 76005: case 76006: case 76007:
	case 76008: case 76009: case 76010: case 76011:
	case 76012: case 76013: case 76014:
		return true;
	}
	return false;
}

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
bool CItem::IsMountItem()
{
	if (GetType() == ITEM_COSTUME && GetSubType() == COSTUME_MOUNT)
		return true;

	return false;
}
#endif

#ifdef ENABLE_RUNE_SYSTEM
bool CItem::IsRune() {
	if ((GetType() == ITEM_COSTUME) && (GetSubType() >= RUNE_SLOT1) && (GetSubType() <= RUNE_SLOT7))
		return true;

	return false;
}
#endif

#ifdef ENABLE_MULTI_NAMES
const char* CItem::GetName(uint8_t Lang)
{
	if (!m_pProto)
		return "";

	const size_t localeCount = sizeof(m_pProto->szLocaleName) / sizeof(m_pProto->szLocaleName[0]);
	const uint8_t fallbackIndex = (localeCount > 1) ? 1 : 0;

	uint8_t idx = Lang;
	if (idx == 0)
	{
		idx = fallbackIndex;

		if (m_pOwner)
		{
			if (LPDESC d = m_pOwner->GetDesc())
			{
				const uint8_t dlang = d->GetLanguage();
				if (dlang != 0)
					idx = dlang;
			}
		}
	}

	if (localeCount == 0 || idx >= localeCount)
		idx = fallbackIndex;

	const char* name = m_pProto->szLocaleName[idx];
	if (!name || !*name)
		return m_pProto->szName;

	return name;
}
#endif

// Phase 11: migrated from item.cpp slice S2

void CItem::RemoveFlag(int32_t bit)
{
	REMOVE_BIT(m_lFlag, bit);
	SyncItemFlagsComponent(this);
}

void CItem::AddFlag(int32_t bit)
{
	SET_BIT(m_lFlag, bit);
	SyncItemFlagsComponent(this);
}

int CItem::GetCount()
{
	const TItemTable* proto = GetProto();
	const TItemTable* expected = ITEM_MANAGER::instance().GetTable(GetOriginalVnum());

	if (proto != expected)
	{
		proto = expected;
	}

	int count = m_dwCount;
	entt::entity e = ItemEntityOf(this);
	if (e != entt::null)
	{
		if (const auto* itemCount = g_registry.try_get<ecs::ItemCount>(e))
			count = itemCount->count;
	}

	const uint8_t itemType = proto ? proto->bType : 0;
	if (itemType == ITEM_ELK)
	{
		return MIN(count, INT_MAX);
	}

	return MIN(count, g_bItemCountLimit);
}

bool CItem::SetCount(int count)
{
#ifdef ENABLE_MINUS_COUNT_FIX_RAZOR93
	if (count < 0) {
		sys_err("SetCount attempted negative value (count=%d) vnum=%u", count, GetVnum());
		count = 0;
	}

	const int limit = (GetType() == ITEM_ELK) ? INT_MAX : g_bItemCountLimit;
	if (count > limit)
		count = limit;

	m_dwCount = count;
#else

	if (GetType() == ITEM_ELK)
	{
		m_dwCount = MIN(count, INT_MAX);
	}
	else
	{
		m_dwCount = MIN(count, g_bItemCountLimit);
	}
#endif
	SyncItemCountComponent(this, m_dwCount);
	if (count == 0 && m_pOwner)
	{
		if (GetSubType() == USE_ABILITY_UP || GetSubType() == USE_POTION || GetVnum() == 70020)
		{
			auto* pOwner = GetOwner();
			uint16_t wCell = GetCell();

			RemoveFromCharacter();

			if (!IsDragonSoul())
			{
				LPITEM pItem = pOwner->FindSpecifyItem(GetVnum());
				if (nullptr != pItem)
				{
					pItem->SetCount(pItem->GetCount() + count);
					M2_DESTROY_ITEM(this);
					return false;
				}
			}

			M2_DESTROY_ITEM(RemoveFromCharacter());

			const uint8_t bType = pOwner->GetQuestFlag("main_quest_flame_lv7.reward")*1 + pOwner->GetQuestFlag("main_quest_flame_lv7.reward")*2;
			if (IsDragonSoul())
			{
				if (bType == 0)
					pOwner->DragonSoul_RefineWindow_Close();
				else if (bType == 1)
					pOwner->DragonSoul_RefineWindow_Close();
			}

			LogManager::instance().ItemLog(pOwner, this, "REMOVE", "DELETED (set count to 0)");

			return false;
		}

		M2_DESTROY_ITEM(RemoveFromCharacter());
		return false;
	}

	UpdatePacket();
	Save();

	return true;
}

int32_t CItem::GetValue(uint32_t idx)
{
	assert(idx < ITEM_VALUES_MAX_NUM);
	return GetProto()->alValues[idx];
}

void CItem::SetSockets(const int32_t* c_al)
{
	memcpy(m_alSockets, c_al, sizeof(m_alSockets));
	SyncItemSocketsComponent(this);
	Save();
}

void CItem::SetSocket(int i, int32_t v, bool bLog)
{
	assert(i < ITEM_SOCKET_MAX_NUM);
	m_alSockets[i] = v;
	SyncItemSocketsComponent(this);
	UpdatePacket();
	Save();
	if (bLog)
	{
#ifdef ENABLE_NEWSTUFF
		if (g_iDbLogLevel >= LOG_LEVEL_MAX)
#endif
			LogManager::instance().ItemLog(i, v, 0, GetID(), "SET_SOCKET", "", "", GetOriginalVnum());
	}
}

int64_t CItem::GetGold()
{
	if (IS_SET(GetFlag(), ITEM_FLAG_COUNT_PER_1GOLD))
	{
		if (GetProto()->dwGold == 0)
			return GetCount();
		else
			return GetCount() / GetProto()->dwGold;
	}
	else
		return GetProto()->dwGold;
}

int64_t CItem::GetShopBuyPrice()
{
	return GetProto()->dwShopBuyPrice;
}

int CItem::GetSocketCount()
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
	{
		if (GetSocket(i) == 0)
			return i;
	}
	return ITEM_SOCKET_MAX_NUM;
}

bool CItem::AddSocket()
{
	int count = GetSocketCount();
	if (count == ITEM_SOCKET_MAX_NUM)
		return false;
	m_alSockets[count] = 1;
	SyncItemSocketsComponent(this);
	return true;
}

// Phase 11: migrated from item_attribute.cpp batch A

int CItem::GetAttributeSetIndex()

{

	if (GetType() == ITEM_WEAPON)

	{

		if (GetSubType() == WEAPON_ARROW)

			return -1;



		return ATTRIBUTE_SET_WEAPON;

	}



	if (GetType() == ITEM_ARMOR)

	{

		switch (GetSubType())

		{

			case ARMOR_BODY:

				return ATTRIBUTE_SET_BODY;



			case ARMOR_WRIST:

				return ATTRIBUTE_SET_WRIST;



			case ARMOR_FOOTS:

				return ATTRIBUTE_SET_FOOTS;



			case ARMOR_NECK:

				return ATTRIBUTE_SET_NECK;



			case ARMOR_HEAD:

				return ATTRIBUTE_SET_HEAD;



			case ARMOR_SHIELD:

				return ATTRIBUTE_SET_SHIELD;



			case ARMOR_EAR:

				return ATTRIBUTE_SET_EAR;

		

#if defined(ENABLE_PENDANT) && defined(ENABLE_NEW_BONUS_TALISMAN)

			case ARMOR_PENDANT:

				return ATTRIBUTE_SET_PENDANT;

#endif

		}

	}

#ifdef ENABLE_ATTR_COSTUMES

	else if (GetType() == ITEM_COSTUME)

	{

		switch (GetSubType())

		{

			case COSTUME_BODY:

				return ATTRIBUTE_SET_COSTUME_BODY;

			case COSTUME_HAIR:

				return ATTRIBUTE_SET_COSTUME_HAIR;

			case COSTUME_WEAPON:

				return ATTRIBUTE_SET_COSTUME_WEAPON;

#ifdef ENABLE_STOLE_COSTUME

			case COSTUME_STOLE:

				return ATTRIBUTE_SET_COSTUME_STOLE;

#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM

			case COSTUME_MOUNT:

				break;

#endif

		}

	}

#endif



	return -1;

}

bool CItem::HasAttr(uint8_t bApply)

{

	bool ignoreBaseApplies = false;



#ifdef ENABLE_PENDANT

	// Talizm�n / pendant: lehessen ugyanaz a b�nusz az alap b�nusz mellett is

	if ((GetType() == ITEM_ARMOR && GetSubType() == ARMOR_NUM_TYPES) || (GetWearFlag() & WEARABLE_PENDANT))

		ignoreBaseApplies = true;

#endif



	if (!ignoreBaseApplies)

	{

		for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)

			if (m_pProto->aApplies[i].bType == bApply)

				return true;



#ifdef ENABLE_ITEM_EXTRA_PROTO

		if (HasExtraProto())

		{

#ifdef ENABLE_NEW_EXTRA_BONUS

			for (int i = 0; i < NEW_EXTRA_BONUS_COUNT; ++i)

				if (m_ExtraProto->ExtraBonus[i].bType == bApply)

					return true;

#endif

		}

#endif

	}



 

	for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)

		if (GetAttributeType(i) == bApply)

			return true;



	return false;

}

bool CItem::HasRareAttr(uint8_t bApply)

{

	for (int i = 0; i < MAX_RARE_ATTR_NUM; ++i)

		if (GetAttributeType(i + 5) == bApply)

			return true;



	return false;

}

int CItem::GetAttributeCount()

{

	int i;



	for (i = 0; i < MAX_NORM_ATTR_NUM; ++i)

	{

		if (GetAttributeType(i) == 0)

			break;

	}



	return i;

}

int CItem::FindAttribute(uint8_t bType)

{

	for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)

	{

		if (GetAttributeType(i) == bType)

			return i;

	}



	return -1;

}

bool CItem::RemoveAttributeAt(int index)

{

	if (GetAttributeCount() <= index)

		return false;



	for (int i = index; i < MAX_NORM_ATTR_NUM - 1; ++i)

	{

		SetAttribute(i, GetAttributeType(i + 1), GetAttributeValue(i + 1));

	}



	SetAttribute(MAX_NORM_ATTR_NUM - 1, APPLY_NONE, 0);

	return true;

}

bool CItem::RemoveAttributeType(uint8_t bType)

{

	int index = FindAttribute(bType);

	return index != -1 && RemoveAttributeType(index);

}

void CItem::SetAttributes(const TPlayerItemAttribute* c_pAttribute)

{

	memcpy(m_aAttr, c_pAttribute, sizeof(m_aAttr));

	Save();

}

void CItem::SetAttribute(int i, uint8_t bType, short sValue)

{

	assert(i < MAX_NORM_ATTR_NUM);

	m_aAttr[i].bType = bType;

	m_aAttr[i].sValue = sValue;

	SyncItemAttributesComponent(this);
	UpdatePacket();

	Save();

	if (bType)

	{

		const char * pszIP = nullptr;

		if (GetOwner() && GetOwner()->GetDesc())

			pszIP = GetOwner()->GetDesc()->GetHostName();

		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(i, bType, sValue, GetID(), "SET_ATTR", "", pszIP ? pszIP : "", GetOriginalVnum()));

	}

}

void CItem::SetAttribute2(int i, uint8_t bType, short sValue)

{

	assert(i < MAX_NORM_ATTR_NUM+2);



	m_aAttr[i].bType = bType;

	m_aAttr[i].sValue = sValue;

	UpdatePacket();

	Save();



	if (bType)

	{

		const char* pszIP = nullptr;



		if (GetOwner() && GetOwner()->GetDesc())

			pszIP = GetOwner()->GetDesc()->GetHostName();



		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(i, bType, sValue, GetID(), "SET_ATTR", "", pszIP ? pszIP : "", GetOriginalVnum()));

	}

}

void CItem::SetForceAttribute(int i, uint8_t bType, short sValue)

{

	assert(i < ITEM_ATTRIBUTE_MAX_NUM);



	m_aAttr[i].bType = bType;

	m_aAttr[i].sValue = sValue;

	UpdatePacket();

	Save();



	if (bType)

	{

		const char * pszIP = nullptr;



		if (GetOwner() && GetOwner()->GetDesc())

			pszIP = GetOwner()->GetDesc()->GetHostName();



		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(i, bType, sValue, GetID(), "SET_FORCE_ATTR", "", pszIP ? pszIP : "", GetOriginalVnum()));

	}

}

void CItem::CopyAttributeTo(LPITEM pItem)

{

	pItem->SetAttributes(m_aAttr);

}

// Phase 11: migrated from item_attribute.cpp batch B

void CItem::AddAttribute(uint8_t bApply, short sValue)
{
	int iSameAttrCount = 0;
	for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)
	{
		if (GetAttributeType(i) == bApply)
			++iSameAttrCount;
	}

	if (IsZodiacAttributeItemVnum(GetVnum()))
	{
		if (iSameAttrCount >= 1)
			return;
	}
	else if (HasAttr(bApply))
		return;

	int i = GetAttributeCount();

	if (i >= MAX_NORM_ATTR_NUM)
		sys_err("item attribute overflow!");
	else
	{
		if (sValue)
			SetAttribute(i, bApply, sValue);
	}
}

void CItem::AddAttribute2(uint8_t bApply, short sValue)
{
	if (HasAttr(bApply))
		return;

	int i = GetAttributeCount();

	if (i >= MAX_NORM_ATTR_NUM+2)
		sys_err("item attribute overflow!");
	else
	{
		if (sValue)
			SetAttribute2(i, bApply, sValue);
	}
}

bool CItem::ChangeKKAK(int iAddonType)
{
	(void)iAddonType; // 

	// random  
	int iSkillBonus = MINMAX(-30, int(gauss_random(0, 5) + 0.5f), 30);
	int iNormalHitBonus = 0;
	if (abs(iSkillBonus) <= 20)
		iNormalHitBonus = -2 * iSkillBonus + abs(number(-8, 8) + number(-8, 8)) + number(1, 4);
	else
		iNormalHitBonus = -2 * iSkillBonus + number(1, 5);

	// 71/72  
	//RemoveAttributeType(APPLY_SKILL_DAMAGE_BONUS);
	//RemoveAttributeType(APPLY_NORMAL_HIT_DAMAGE_BONUS);
	AddAttr4(APPLY_NORMAL_HIT_DAMAGE_BONUS, iNormalHitBonus);
	AddAttr4(APPLY_SKILL_DAMAGE_BONUS, iSkillBonus);

	return true;
}

bool CItem::AddRareAttribute3(uint8_t bApply, short sValue)
{
	int count = GetRareAttrCount();

	if (count >= ITEM_ATTRIBUTE_RARE_NUM+2)
		return false;

	int pos = count + ITEM_ATTRIBUTE_RARE_START;
	TPlayerItemAttribute& attr = m_aAttr[pos];

	int nAttrSet = GetAttributeSetIndex();
	std::vector<int> avail;

	for (int i = 0; i < MAX_APPLY_NUM; ++i)
	{
		const TItemAttrTable& r = g_map_itemRare[i];

		if (r.dwApplyIndex != 0 && r.bMaxLevelBySet[nAttrSet] > 0 && HasRareAttr(i) != true)
		{
			avail.push_back(i);
		}
	}
}

void CItem::AddAttr4(uint8_t bApply, uint8_t bLevel)
{
	if (HasAttr(bApply))
		return;

	if (bLevel <= 0)
		return;

	int i = GetAttributeCount();

	if (i < 5)
		return;
	else
	{
		const TItemAttrTable& r = g_map_itemAttr[bApply];
		int32_t lVal = r.lValues[MIN(4, bLevel - 1)];
#ifdef ENABLE_ATTR_COSTUMES
		if (GetType() == ITEM_COSTUME)
			lVal = r.lValues[MIN(9, bLevel + 5 - 1)];
#endif

		if (lVal)
			SetAttribute(i, bApply, lVal);
	}
}

void CItem::AddAttr(uint8_t bApply, uint8_t bLevel)
{
	int iSameAttrCount = 0;
	for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)
	{
		if (GetAttributeType(i) == bApply)
			++iSameAttrCount;
	}

	if (IsZodiacAttributeItemVnum(GetVnum()))
	{
		if (iSameAttrCount >= 1)
			return;
	}
	else if (HasAttr(bApply))
		return;

	if (bLevel <= 0)
		return;

	int i = GetAttributeCount();

	if (i == MAX_NORM_ATTR_NUM)
		sys_err("item attribute overflow!");
	else
	{
		const TItemAttrTable & r = g_map_itemAttr[bApply];
		int32_t lVal = r.lValues[MIN(4, bLevel - 1)];
#ifdef ENABLE_ATTR_COSTUMES
		if (GetType() == ITEM_COSTUME)
			lVal = r.lValues[MIN(9, bLevel + 5 - 1)];
#endif
		
		if (lVal)
			SetAttribute(i, bApply, lVal);
	}
}

void CItem::PutAttributeWithLevel(uint8_t bLevel)
{
	int iAttributeSet = GetAttributeSetIndex();
	if (iAttributeSet < 0)
		return;

	if (bLevel > ITEM_ATTRIBUTE_MAX_LEVEL)
		return;

	std::vector<int> avail;

	int total = 0;

	// ???? ?� ?ִ� ?�?? ???�?� ��??
	for (int i = 0; i < MAX_APPLY_NUM; ++i)
	{
		const TItemAttrTable & r = g_map_itemAttr[i];

		if (!r.bMaxLevelBySet[iAttributeSet])
			continue;

		if (IsZodiacAttributeItemVnum(GetVnum()))
		{
			int iSameAttrCount = 0;
			for (int j = 0; j < MAX_NORM_ATTR_NUM; ++j)
			{
				if (GetAttributeType(j) == i)
					++iSameAttrCount;
			}

			if (iSameAttrCount >= 1)
				continue;
		}
		else if (HasAttr(i))
		{
			continue;
		}

		avail.push_back(i);
		total += r.dwProb;
	}

	if (avail.empty())
	{
		return;
	}

	// ��??�? ???��� ?��� �?�??� ?��? ???? ?�?? ?���
	unsigned int prob = number(1, total);
	int attr_idx = APPLY_NONE;

	for (uint32_t i = 0; i < avail.size(); ++i)
	{
		const TItemAttrTable & r = g_map_itemAttr[avail[i]];

		if (prob <= r.dwProb)
		{
			attr_idx = avail[i];
			break;
		}

		prob -= r.dwProb;
	}

	if (!attr_idx)
	{
		sys_err("Cannot put item attribute %d %d", iAttributeSet, bLevel);
		return;
	}

	const TItemAttrTable & r = g_map_itemAttr[attr_idx];

	// �?�??� ?�?? �??� ?ִ�? ���?
	if (bLevel > r.bMaxLevelBySet[iAttributeSet])
		bLevel = r.bMaxLevelBySet[iAttributeSet];

	AddAttr(attr_idx, bLevel);
}

void CItem::PutAttribute(const int * aiAttrPercentTable)
{
	int iAttrLevelPercent = number(1, 100);
	int i;

	for (i = 0; i < ITEM_ATTRIBUTE_MAX_LEVEL; ++i)
	{
		if (iAttrLevelPercent <= aiAttrPercentTable[i])
			break;

		iAttrLevelPercent -= aiAttrPercentTable[i];
	}

	PutAttributeWithLevel(i + 1);
}

void CItem::ChangeAttribute(const int* aiChangeProb)
{
	int iAttributeCount = GetAttributeCount();

	ClearAttribute();

	if (iAttributeCount == 0)
		return;

	TItemTable const * pProto = GetProto();

	if (pProto && pProto->sAddonType)
	{
		ApplyAddon(pProto->sAddonType);
	}

	static const int tmpChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
	{
		0, 10, 40, 35, 15,
	};

	for (int i = GetAttributeCount(); i < iAttributeCount; ++i)
	{
#ifdef ATTR_LOCK		
		if (GetLockedAttr() == i)
		{
			continue;
		}
#endif
		if (aiChangeProb == nullptr)
		{
			PutAttribute(tmpChangeProb);
		}
		else
		{
			PutAttribute(aiChangeProb);
		}
	}
}

void CItem::AddAttribute()
{
	static const int aiItemAddAttributePercent[ITEM_ATTRIBUTE_MAX_LEVEL] =
	{
		40, 50, 10, 0, 0
	};

	if (GetAttributeCount() < MAX_NORM_ATTR_NUM)
		PutAttribute(aiItemAddAttributePercent);
}

void CItem::ClearAttribute()
{
	for (int i = 0; i < MAX_NORM_ATTR_NUM; ++i)
	{
#ifdef ATTR_LOCK		
		if (GetLockedAttr() == i)
		{
			continue;
		}
#endif
		m_aAttr[i].bType = 0;
		m_aAttr[i].sValue = 0;
	}
	SyncItemAttributesComponent(this);
}

// Phase 11: migrated from item_attribute.cpp batch C

int CItem::GetRareAttrCount()
{
	int ret = 0;

	for (uint32_t dwIdx = ITEM_ATTRIBUTE_RARE_START; dwIdx < ITEM_ATTRIBUTE_RARE_END; dwIdx++)
	{
		if (m_aAttr[dwIdx].bType != 0)
			ret++;
	}

	return ret;
}

bool CItem::ChangeRareAttribute()
{
	if (GetRareAttrCount() == 0)
		return false;

	int cnt = GetRareAttrCount();

	for (int i = 0; i < cnt; ++i)
	{
		m_aAttr[i + ITEM_ATTRIBUTE_RARE_START].bType = 0;
		m_aAttr[i + ITEM_ATTRIBUTE_RARE_START].sValue = 0;
	}

	SyncItemAttributesComponent(this);

	if (GetOwner() && GetOwner()->GetDesc())
		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(GetOwner(), this, "SET_RARE_CHANGE", ""))
	else
		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(0, 0, 0, GetID(), "SET_RARE_CHANGE", "", "", GetOriginalVnum()))

	for (int i = 0; i < cnt; ++i)
	{
		AddRareAttribute();
	}

	return true;
}

bool CItem::AddRareAttribute()
{
	int count = GetRareAttrCount();

	if (count >= ITEM_ATTRIBUTE_RARE_NUM)
		return false;

	int pos = count + ITEM_ATTRIBUTE_RARE_START;
	TPlayerItemAttribute & attr = m_aAttr[pos];

	int nAttrSet = GetAttributeSetIndex();
	std::vector<int> avail;

	for (int i = 0; i < MAX_APPLY_NUM; ++i)
	{
		const TItemAttrTable & r = g_map_itemRare[i];

		if (r.dwApplyIndex != 0 && r.bMaxLevelBySet[nAttrSet] > 0 && HasRareAttr(i) != true)
		{
			avail.push_back(i);
		}
	}

	if (avail.empty())
	{
		sys_err("Couldn't add a rare bonus - item_attr_rare has incorrect values!");
		return false;
	}

	const TItemAttrTable& r = g_map_itemRare[avail[number(0, avail.size() - 1)]];
	int nAttrLevel = 5;

	if (nAttrLevel > r.bMaxLevelBySet[nAttrSet])
		nAttrLevel = r.bMaxLevelBySet[nAttrSet];

	attr.bType = r.dwApplyIndex;
	attr.sValue = r.lValues[nAttrLevel - 1];

	SyncItemAttributesComponent(this);
	UpdatePacket();

	Save();

	const char * pszIP = nullptr;

	if (GetOwner() && GetOwner()->GetDesc())
		pszIP = GetOwner()->GetDesc()->GetHostName();

	LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(pos, attr.bType, attr.sValue, GetID(), "SET_RARE", "", pszIP ? pszIP : "", GetOriginalVnum()));
	return true;
}

void CItem::AddRareAttribute2(const int * aiAttrPercentTable)
{
	static const int aiItemAddAttributePercent[ITEM_ATTRIBUTE_MAX_LEVEL] =
	{
		40, 50, 10, 0, 0
	};
	if (aiAttrPercentTable == nullptr)
		aiAttrPercentTable = aiItemAddAttributePercent;

	if (GetRareAttrCount() < MAX_RARE_ATTR_NUM)
		PutRareAttribute(aiAttrPercentTable);
}

void CItem::PutRareAttribute(const int * aiAttrPercentTable)
{
	int iAttrLevelPercent = number(1, 100);
	int i;

	for (i = 0; i < ITEM_ATTRIBUTE_MAX_LEVEL; ++i)
	{
		if (iAttrLevelPercent <= aiAttrPercentTable[i])
			break;

		iAttrLevelPercent -= aiAttrPercentTable[i];
	}

	PutRareAttributeWithLevel(i + 1);
}

void CItem::PutRareAttributeWithLevel(uint8_t bLevel)
{
	int iAttributeSet = GetAttributeSetIndex();
	if (iAttributeSet < 0)
		return;

	if (bLevel > ITEM_ATTRIBUTE_MAX_LEVEL)
		return;

	std::vector<int> avail;

	int total = 0;

	// ???? ?� ?ִ� ?�?? ???�?� ��??
	for (int i = 0; i < MAX_APPLY_NUM; ++i)
	{
		const TItemAttrTable & r = g_map_itemRare[i];

		if (r.bMaxLevelBySet[iAttributeSet] && !HasRareAttr(i))
		{
			avail.push_back(i);
			total += r.dwProb;
		}
	}

	if (avail.empty())
	{
		return;
	}

	// ��??�? ???��� ?��� �?�??� ?��? ???? ?�?? ?���
	unsigned int prob = number(1, total);
	int attr_idx = APPLY_NONE;

	for (uint32_t i = 0; i < avail.size(); ++i)
	{
		const TItemAttrTable & r = g_map_itemRare[avail[i]];

		if (prob <= r.dwProb)
		{
			attr_idx = avail[i];
			break;
		}

		prob -= r.dwProb;
	}

	if (!attr_idx)
	{
		sys_err("Cannot put item rare attribute %d %d", iAttributeSet, bLevel);
		return;
	}

	const TItemAttrTable & r = g_map_itemRare[attr_idx];

	// �?�??� ?�?? �??� ?ִ�? ���?
	if (bLevel > r.bMaxLevelBySet[iAttributeSet])
		bLevel = r.bMaxLevelBySet[iAttributeSet];

	AddRareAttr(attr_idx, bLevel);
}

void CItem::AddRareAttr(uint8_t bApply, uint8_t bLevel)
{
	if (HasRareAttr(bApply))
		return;

	if (bLevel <= 0)
		return;

	int i = ITEM_ATTRIBUTE_RARE_START + GetRareAttrCount();

	if (i == ITEM_ATTRIBUTE_RARE_END)
		sys_err("item rare attribute overflow!");
	else
	{
		const TItemAttrTable & r = g_map_itemRare[bApply];
		int32_t lVal = r.lValues[MIN(4, bLevel - 1)];

		if (lVal)
			SetForceAttribute(i, bApply, lVal);
	}
}

// char_item.cpp slice A moved into ItemSystem.cpp

// Phase 11: migrated from item.cpp slice S1

int CItem::GetSpecialGroup() const
{
	return ITEM_MANAGER::instance().GetSpecialGroupFromItem(ItemVnumOrLegacy(const_cast<LPITEM>(this)));
}

bool CItem::IsRideItem()
{
	if (ITEM_UNIQUE == GetType() && UNIQUE_SPECIAL_RIDE == GetSubType())
		return true;
	if (ITEM_UNIQUE == GetType() && UNIQUE_SPECIAL_MOUNT_RIDE == GetSubType())
		return true;
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (ITEM_COSTUME == GetType() && COSTUME_MOUNT == GetSubType())
		return true;
#endif
	return false;
}

bool CItem::IsPCBangItem()
{
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		if (m_pProto->aLimits[i].bType == LIMIT_PCBANG)
			return true;
	}
	return false;
}

bool CItem::CheckItemUseLevel(int nLevel)
{
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		if (this->m_pProto->aLimits[i].bType == LIMIT_LEVEL)
		{
			if (this->m_pProto->aLimits[i].lValue > nLevel) return false;
			else return true;
		}
	}
	return true;
}

int CItem::GetLevelLimit()
{
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		if (this->m_pProto->aLimits[i].bType == LIMIT_LEVEL)
		{
			return this->m_pProto->aLimits[i].lValue;
		}
	}
	return 0;
}

bool CItem::OnAfterCreatedItem()
{
	if (-1 != this->GetProto()->cLimitRealTimeFirstUseIndex)
	{
		if (0 != GetSocket(1))
		{
			StartRealTimeExpireEvent();
		}
	}

#ifdef ENABLE_SOUL_SYSTEM
	if (GetType() == ITEM_SOUL)
	{
		StartSoulItemEvent();
	}
#endif

	return true;
}

bool CItem::IsDragonSoul()
{
	return GetType() == ITEM_DS;
}

bool CItem::IsSameSpecialGroup(const LPITEM item) const
{
	if (ItemVnumOrLegacy(const_cast<LPITEM>(this)) == ItemVnumOrLegacy(item))
		return true;

	if (GetSpecialGroup() && (item->GetSpecialGroup() == GetSpecialGroup()))
		return true;

	return false;
}

bool CItem::IsExtraItem()
{
	switch (GetVnum()) {
	case 70612:
	case 70613:
	case 70614:
	case 88968:
	case 30002:
	case 30003:
	case 30004:
	case 30005:
	case 30006:
	case 30015:
	case 30047:
	case 30050:
	case 30165:
	case 30166:
	case 30167:
	case 30168:
	case 30251:
	case 30252:
	case 2870:
	case 2871:
	case 2872:
	case 2873:
	case 2874:
	case 2875:
	case 2876:
	case 2877:
	case 2878:
		return false;
	case 30277:
	case 30279:
	case 30284:
	case 86053:
	case 86054:
	case 86055:
	case 70102:
	case 39008:
	case 71001:
	case 72310:
	case 39030:
	case 71094:
#ifdef __NEWPET_SYSTEM__
	case 86077:
	case 86076:
	case 55010:
	case 55011:
	case 55012:
	case 55013:
	case 55014:
	case 55015:
	case 55016:
	case 55017:
	case 55018:
	case 55019:
	case 55020:
	case 55021:
#endif
	case 50513:
	case 50525:
	case 50526:
	case 50527:
	case 71095:
		return true;
	default:
		break;
	}

	switch (GetType()) {
	case ITEM_MATERIAL:
	case ITEM_METIN:
	case ITEM_SKILLBOOK:
	case ITEM_SKILLFORGET:
	case ITEM_GIFTBOX:
	case ITEM_TREASURE_BOX:
	case ITEM_TREASURE_KEY:
	{
		return true;
	}
	case ITEM_USE:
	{
		uint8_t subtype = GetSubType();
		return (subtype == USE_CHANGE_ATTRIBUTE ||
			subtype == USE_ADD_ATTRIBUTE ||
			subtype == USE_ADD_ATTRIBUTE2 ||
			subtype == USE_CHANGE_ATTRIBUTE2 ||
			subtype == USE_CHANGE_COSTUME_ATTR ||
			subtype == USE_RESET_COSTUME_ATTR ||
			subtype == USE_CHANGE_ATTRIBUTE_PLUS ||
#ifdef ATTR_LOCK
			subtype == USE_ADD_ATTRIBUTE_LOCK ||
			subtype == USE_CHANGE_ATTRIBUTE_LOCK ||
			subtype == USE_DELETE_ATTRIBUTE_LOCK ||
#endif
#ifdef ENABLE_ATTR_COSTUMES
			subtype == USE_CHANGE_ATTR_COSTUME ||
			subtype == USE_ADD_ATTR_COSTUME1 ||
			subtype == USE_ADD_ATTR_COSTUME2 ||
			subtype == USE_REMOVE_ATTR_COSTUME ||
#endif
#ifdef ENABLE_DS_ENCHANT
			subtype == USE_DS_ENCHANT ||
#endif
#ifdef ENABLE_DS_ENCHANT
			subtype == USE_ENCHANT_STOLE ||
#endif
			subtype == USE_POTION ||
			subtype == USE_POTION_NODELAY ||
			subtype == USE_POTION_CONTINUE ||
			subtype == USE_ABILITY_UP ||
			subtype == USE_AFFECT
#ifdef ENABLE_NEW_USE_POTION
			|| subtype == USE_NEW_POTIION
#endif
			);
	}
	default:
	{
		break;
	}
	}

	return false;
}

uint8_t CItem::GetExtraCategory()
{
	switch (GetType())
	{
	case ITEM_SKILLBOOK:
	case ITEM_SKILLFORGET:
	{
		return 0;
	}
	case ITEM_MATERIAL:
	{
		return 1;
	}
	case ITEM_METIN:
	{
		return 2;
	}
	case ITEM_GIFTBOX:
	case ITEM_TREASURE_BOX:
	case ITEM_TREASURE_KEY:
	{
		return 3;
	}
	case ITEM_USE:
	{
		uint8_t subtype = GetSubType();

		if (IsExtraEnchantUseSubtype(subtype))
			return 4;

		if (IsExtraPotionUseSubtype(subtype))
			return 5;

		break;
	}
	default:
	{
		break;
	}
	}

	switch (GetVnum()) {
	case 30277:
	case 30279:
	case 30284:
	case 86053:
	case 86054:
	case 86055:
		return 1;
	case 70102:
	case 39008:
	case 71001:
	case 72310:
	case 39030:
	case 71094:
#ifdef __NEWPET_SYSTEM__
	case 86077:
	case 86076:
	case 55010:
	case 55011:
	case 55012:
	case 55013:
	case 55014:
	case 55015:
	case 55016:
	case 55017:
	case 55018:
	case 55019:
	case 55020:
	case 55021:
#endif
	case 50513:
	case 50525:
	case 50526:
	case 50527:
		return 0;
	}

	return 0;
}

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
				// �3A� ���!?! ��I�E 1���AI�� 3N3�L�U.
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
				// �3A� ���!?! ��I�E 1���AI�� 3N3�L�U.
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

	// ?1?�A3���! 3a�I�U.
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

	// > WEAR_MAX_NUM : ?�EY1� 11�Ե�.
	if (bCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
	{
		sys_err("CHARACTER::GetWear: invalid wear cell %d", bCell);
		return nullptr;
	}

	return m_pointsInstant.pItems[INVENTORY_MAX_NUM + bCell];
}


void CHARACTER::SetWear(uint8_t bCell, LPITEM item)
{
	// > WEAR_MAX_NUM : ?�EY1� 11�Ե�.
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

	// 1�3?�!�� Ao ��A�?!1� A�1A�� AԱ� ����
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

	//1A�� A��� ��?�1A ���� �� ��?�?�o� A1A�
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

	// ?�EY1� A�1� A3��
	if (item->IsDragonSoul())
	{
		// ��Ao A�A�A� ?�EY1�AI AI1I ��3�! AִU�� �o?��O 1� 3o�U.
		// ?�EY1�Ao swapA� ��?o�I�� 3E�E.
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
	// ?�EY1�AI 3A��.
	else
	{
		// �o?��O ��?! 3AAIAUAI AִU��,
		if (GetWear(iWearCell) && !IS_SET(GetWear(iWearCell)->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		{
			// AI 3AAIAUAo �N1o 1�E��� o��a oO�!. swap ?a1A ?IA� oO�!
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
		// 3AAIAU A�AE ��?� AIE�o�Aʹ� ��?��I�� 3E3A�� 1A�LAI �����Ǵ� 1a1� A3��.
		if (-1 != item->GetProto()->cLimitRealTimeFirstUseIndex)
		{
			// �N 1oAI�� ��?��N 3AAIAUA��� ?�oδ� Socket1A� o��� AǴ��N�U. (Socket1?! ��?�E11� ��I)
			if (0 == item->GetSocket(1))
			{
				// ��?�!��1A�LAo Default �aA��� Limit Value �aA� ��?��I��, Socket0?! �aAI A�A��� �� �aA� ��?��I���I �N�U. (��A��� AE)
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

		// �󸶴� AIoYA� AE1´?A� 1���(71135) �o?�1A AIAaA� 1ߵ?
		if (true == CItemVnumHelper::IsRamadanMoonRing(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_RAMADAN_RING);
		}
		// �O��A� ��A�(71136) �o?�1A AIAaA� 1ߵ?
		else if (true == CItemVnumHelper::IsHalloweenCandy(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_HALLOWEEN_CANDY);
		}
		// �ao1A� 1���(71143) �o?�1A AIAaA� 1ߵ?
		else if (true == CItemVnumHelper::IsHappinessRing(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_HAPPINESS_RING);
		}
		// ��uA� AO�oA�(71145) �o?�1A AIAaA� 1ߵ?
		else if (true == CItemVnumHelper::IsLovePendant(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_LOVE_PENDANT);
		}
		// ITEM_UNIQUEA� �a?i, SpecialItemGroup?! ��Aǵ�3� Aְ�, (item->GetSIGVnum() != NULL)
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

	// 3?3�1���A� �a?i 3?3�1���(��o�) A����� A1A��N�U.
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
		break; // TODO: 1�A��� 3AAIAU �o?�!��?�o� A3��
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

	// ?�?oE� �O���O 1� 3o�� 3AAIAU
	if (IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		return false;

	// 3AAIAU unequip1A A�oYA丮�� ?A�a �� o� Aڸ��! Aִ� �� E�A�
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
		LegacyCharHandle owner;

		FFindOwnership(LPITEM item)
			: item(item), owner(nullptr)
		{
		}

		void operator () (LegacyCharHandle ch)
		{
			if (item->IsOwnership(ch))
				owner = ch;
		}
	};

	struct FCountNearMember
	{
		int		total;
		int		x, y;

		FCountNearMember(LegacyCharHandle center)
			: total(0), x(center->GetX()), y(center->GetY())
		{
		}

		void operator () (LegacyCharHandle ch)
		{
			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
				total += 1;
		}
	};

	struct FMoneyDistributor
	{
		int		total;
		LegacyCharHandle	c;
		int		x, y;
		int64_t		iMoney;

		FMoneyDistributor(LegacyCharHandle center, int64_t iMoney)
			: total(0), c(center), x(center->GetX()), y(center->GetY()), iMoney(iMoney)
		{
		}

		void operator ()(LegacyCharHandle ch)
		{
			if (ch != c)
				if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
				{
					ch->PointChange(POINT_GOLD, iMoney, true);

					if (iMoney > 1000) // Ãµ¿ø ÀÌ»ó¸¸ ±â·ÏÇÑ´Ù.
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

			if (gold > 1000) // Ãµ¿ø ÀÌ»ó¸¸ ±â·ÏÇÑ´Ù.
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
		//	ChatPacket(CHAT_TYPE_INFO, "Belt inventory is disabled. You can only take items out.");//ezt majd be kell kapcsolni ha bekerül élesre
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
			// Határozd meg a típus azonosítót (pl. 18000–18009 = 1800, 18010–18019 = 1801 stb.)
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

	// ±âÈ¹ÀÚÀÇ ¿äÃ»À¸·Î º§Æ® ÀÎº¥Åä¸®¿¡´Â Æ¯Á¤ Å¸ÀÔÀÇ ¾ÆÀÌÅÛ¸¸ ³ÖÀ» ¼ö ÀÖ´Ù.
	if (DestCell.IsBeltInventoryPosition() && false == CBeltInventoryHelper::CanMoveIntoBeltInventory(item))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacket(CHAT_TYPE_INFO, "Belt Only // Csak öveket tehetsz ide.");
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
	// ÀÌ¹Ì Âø¿ëÁßÀÎ ¾ÆÀÌÅÛÀ» ´Ù¸¥ °÷À¸·Î ¿Å±â´Â °æ¿ì, 'ÀåÃ¥ ÇØÁ¦' °¡´ÉÇÑ Áö È®ÀÎÇÏ°í ¿Å±è

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
		if (GetItem(DestCell))	// ÀåºñÀÏ °æ¿ì ÇÑ °÷¸¸ °Ë»çÇØµµ µÈ´Ù.
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 538, "");
#endif
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:	if (GetItem(DestCell))	// ÀåºñÀÏ °æ¿ì ÇÑ °÷¸¸ °Ë»çÇØµµ µÈ´Ù.");//INGAME_DEBUG_RAZOR93

			sys_log(0, "Razor93 LOG:: Called: Char_item.cpp 	if (GetItem(DestCell))	// ÀåºñÀÏ °æ¿ì ÇÑ °÷¸¸ °Ë»çÇØµµ µÈ´Ù. ");

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

		// ¿ëÈ¥¼®ÀÌ ¾Æ´Ñ ¾ÆÀÌÅÛÀº ¿ëÈ¥¼® ÀÎº¥¿¡ µé¾î°¥ ¼ö ¾ø´Ù.
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
			item2->GetVnum() == item->GetVnum()) // ÇÕÄ¥ ¼ö ÀÖ´Â ¾ÆÀÌÅÛÀÇ °æ¿ì
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
			// Övek: 18000 - 18119
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
			//kártyák: 18140 - 18149
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
			//			ChatPacket(CHAT_TYPE_INFO, "item bekerült!");
			//#endif
			//		}
					// Ha belt inventory-ba mozgattak, vagy onnan el, akkor ujraszamolas
			if (Cell.IsBeltInventoryPosition() || DestCell.IsBeltInventoryPosition())
			{
				//sys_log(0, "DEBUG: Belt inventory valtozas - ComputePoints ujrahivas");
				ComputePoints();
				//UpdatePacket();
#ifdef ENABLE_FAKE_SHOP_HEADER
		// Frissítés saját magunknak
				UpdateMountInventoryCountOverhead(this);
				//SendLeaderboardData();
				SendLeaderboardDataSkillMob(this);

				// Frissítés a körülöttünk lévő játékosoknak
				for (const auto& it : m_map_view)
				{
					LPENTITY ent = it.first;
					if (!ent || !ent->IsType(ENTITY_CHARACTER))
						continue;

					auto* viewer = static_cast<LegacyCharHandle>(ent);
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
			// ¸¸¾à ÁÖÀ¸·Á ÇÏ´Â ¾ÆÀÌÅÛÀÌ ¿¤Å©¶ó¸é
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
			// Æò¹üÇÑ ¾ÆÀÌÅÛÀÌ¶ó¸é
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
								//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 01 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

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
								//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 02 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

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
					, 102, "%d#%s", item->GetCount(), item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));//földröl
				//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 03 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
			}

			//Motion(MOTION_PICKUP);
			return true;
		}
		else if (!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP) && GetParty())
		{
			// ´Ù¸¥ ÆÄÆ¼¿ø ¼ÒÀ¯±Ç ¾ÆÀÌÅÛÀ» ÁÖÀ¸·Á°í ÇÑ´Ù¸é
			NPartyPickupDistribute::FFindOwnership funcFindOwnership(item);

			GetParty()->ForEachOnlineMember(funcFindOwnership);

			auto* owner = funcFindOwnership.owner;
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
							//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 04 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

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
							//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 05 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

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
				//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 06 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

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
				//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 07 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

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

		//Ã¢°í ¿¬ÈÄ Ã¼Å©
		if (iPulse - GetSafeboxLoadTime() < PASSES_PER_SEC(g_nPortalLimitTime))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
			return false;
		}

		//°Å·¡°ü·Ã Ã¢ Ã¼Å©
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
		//°³·®ÈÄ ½Ã°£Ã¼Å©
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


		//±ÍÈ¯ºÎ °Å¸®Ã¼Å©
		if (item->GetVnum() != 70302)
		{
			PIXEL_POSITION posWarp;

			int x = 0;
			int y = 0;

			double nDist = 0;
			const double nDistant = 5000.0;
			//±ÍÈ¯±â¾ïºÎ
			if (item->GetVnum() == 22010)
			{
				x = item->GetSocket(0) - GetX();
				y = item->GetSocket(1) - GetY();
			}
			//±ÍÈ¯ºÎ
			else if (item->GetVnum() == 22000)
			{
				ecs::GetRecallPosition(GetMapIndex(), GetEmpire(), posWarp);

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
		//±³È¯ ÈÄ ½Ã°£Ã¼Å©
		if (iPulse - GetExchangeTime() < PASSES_PER_SEC(g_nPortalLimitTime))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
			return false;
		}
		//END_PREVENT_PORTAL_AFTER_EXCHANGE

	}

	//º¸µû¸® ºñ´Ü »ç¿ë½Ã °Å·¡Ã¢ Á¦ÇÑ Ã¼Å©
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

	if (IS_SET(item->GetFlag(), ITEM_FLAG_LOG)) // »ç¿ë ·Î±×¸¦ ³²±â´Â ¾ÆÀÌÅÛ Ã³¸®
	{
		uint32_t vid = item->GetVID();
		int oldCount = item->GetCount();
		uint32_t vnum = item->GetVnum();

		char hint[ITEM_NAME_MAX_LEN + 48 + 1];
		int len = snprintf(hint, sizeof(hint) - 48, "%s", item->GetName());

		if (len < 0 || len >= (int)sizeof(hint) - 48)
			len = (sizeof(hint) - 48) - 1;

		bool ret = UseItemEx(item, DestCell);

		if (nullptr == ITEM_MANAGER::instance().FindByVID(vid)) // UseItemEx¿¡¼­ ¾ÆÀÌÅÛÀÌ »èÁ¦ µÇ¾ú´Ù. »èÁ¦ ·Î±×¸¦ ³²±è
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

// char_item.cpp slice C2b moved into ItemSystem.cpp

EVENTFUNC(kill_campfire_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("kill_campfire_event> <Factor> Null pointer");
		return 0;
	}

	auto*	ch = info->ch.Get();

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	// Phase 10: WRITES_STATE - deferred until ECS component covers m_pkMiningEvent
	ch->m_pkMiningEvent = nullptr;
	M2_DESTROY_CHARACTER(ch);
	return 0;
}

int CalculateConsume(LegacyCharHandle ch)
{
	static const int WARP_NEED_LIFE_PERCENT = 30;
	static const int WARP_MIN_LIFE_PERCENT = 10;
	// CONSUME_LIFE_WHEN_USE_WARP_ITEM
	int consumeLife = 0;
	{
		// CheckNeedLifeForWarp
		const int curLife = ch->GetHP();
		const int needPercent = WARP_NEED_LIFE_PERCENT;
		const int needLife = ch->GetMaxHP() * needPercent / 100;
		if (curLife < needLife)
		{
#ifdef TEXTS_IMPROVEMENT
			if (ch) {
				ch->ChatPacketNew(CHAT_TYPE_INFO, 284, "");
			}
#endif
			return -1;
		}

		consumeLife = needLife;


		// CheckMinLifeForWarp: µ¶¿¡ ÀÇÇØ¼­ Á×À¸¸é ¾ÈµÇ¹Ç·Î »ý¸í·Â ÃÖ¼Ò·®´Â ³²°ÜÁØ´Ù
		const int minPercent = WARP_MIN_LIFE_PERCENT;
		const int minLife = ch->GetMaxHP() * minPercent / 100;
		if (curLife - needLife < minLife)
			consumeLife = curLife - minLife;

		if (consumeLife < 0)
			consumeLife = 0;
	}
	// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM
	return consumeLife;
}

int CalculateConsumeSP(LegacyCharHandle lpChar)
{
	static const int NEED_WARP_SP_PERCENT = 30;

	const int curSP = lpChar->GetSP();
	const int needSP = lpChar->GetMaxSP() * NEED_WARP_SP_PERCENT / 100;

	if (curSP < needSP)
	{
#ifdef TEXTS_IMPROVEMENT
		if (lpChar) {
			lpChar->ChatPacketNew(CHAT_TYPE_INFO, 287, "");
		}
#endif
		return -1;
	}

	return needSP;
}

// #define ENABLE_FIREWORK_STUN
#define ENABLE_ADDSTONE_FAILURE
bool CHARACTER::UseItemEx(LPITEM item, TItemPos DestCell)
{
	int iLimitRealtimeStartFirstUseFlagIndex = -1;
	//int iLimitTimerBasedOnWearFlagIndex = -1;

	uint16_t wDestCell = DestCell.cell;
	uint8_t bDestInven = DestCell.window_type;
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		int32_t limitValue = item->GetProto()->aLimits[i].lValue;

		switch (item->GetProto()->aLimits[i].bType)
		{
		case LIMIT_LEVEL:
			if (GetLevel() < limitValue)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 325, "%d", limitValue);
#endif
				return false;
			}
			break;

		case LIMIT_REAL_TIME_START_FIRST_USE:
			iLimitRealtimeStartFirstUseFlagIndex = i;
			break;

		case LIMIT_TIMER_BASED_ON_WEAR:
			//iLimitTimerBasedOnWearFlagIndex = i;
			break;
		}
	}

	if (test_server)
	{
		sys_log(0, "USE_ITEM %s, Inven %d, Cell %d, ItemType %d, SubType %d", item->GetName(), bDestInven, wDestCell, item->GetType(), item->GetSubType());
	}

	if (CArenaManager::instance().IsLimitedItem(GetMapIndex(), item->GetVnum()) == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
		return false;
	}
#ifdef ENABLE_NEWSTUFF
	else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && IsLimitedPotionOnPVP(item->GetVnum()))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
		return false;
	}
#endif

	// @fixme402 (IsLoadedAffect to block affect hacking)
	if (!IsLoadedAffect()) {
		return false;
	}

	// @fixme141 BEGIN
/* 	if (TItemPos(item->GetWindow(), item->GetCell()).IsBeltInventoryPosition())// @Razor93 GetWear(WEAR_BELT); ne legyen szukseges a wear_mount_costume hez
	{
		LPITEM beltItem = GetWear(WEAR_BELT);

		if (NULL == beltItem)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 785, "");
#endif
			return false;
		}

		if (false == CBeltInventoryHelper::IsAvailableCell(item->GetCell() - BELT_INVENTORY_SLOT_START, beltItem->GetValue(0)))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 786, "");
#endif
			return false;
		}
	} */
	// @fixme141 END

	// ¾ÆÀÌÅÛ ÃÖÃÊ »ç¿ë ÀÌÈÄºÎÅÍ´Â »ç¿ëÇÏÁö ¾Ê¾Æµµ ½Ã°£ÀÌ Â÷°¨µÇ´Â ¹æ½Ä Ã³¸®.
	if (-1 != iLimitRealtimeStartFirstUseFlagIndex)
	{
		// ÇÑ ¹øÀÌ¶óµµ »ç¿ëÇÑ ¾ÆÀÌÅÛÀÎÁö ¿©ºÎ´Â Socket1À» º¸°í ÆÇ´ÜÇÑ´Ù. (Socket1¿¡ »ç¿ëÈ½¼ö ±â·Ï)
		if (0 == item->GetSocket(1))
		{
			// »ç¿ë°¡´É½Ã°£Àº Default °ªÀ¸·Î Limit Value °ªÀ» »ç¿ëÇÏµÇ, Socket0¿¡ °ªÀÌ ÀÖÀ¸¸é ±× °ªÀ» »ç¿ëÇÏµµ·Ï ÇÑ´Ù. (´ÜÀ§´Â ÃÊ)
			int32_t duration = (0 != item->GetSocket(0)) ? item->GetSocket(0) : item->GetProto()->aLimits[iLimitRealtimeStartFirstUseFlagIndex].lValue;

			if (0 == duration)
				duration = 60 * 60 * 24 * 7;

			item->SetSocket(0, time(nullptr) + duration);
			item->StartRealTimeExpireEvent();
		}

		if (false == item->IsEquipped())
			item->SetSocket(1, item->GetSocket(1) + 1);
	}

#ifdef __NEWPET_SYSTEM__
	if (item->GetVnum() == 55001)
	{

		LPITEM item2;

		if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
			return false;

		if (item2->IsExchanging() || item2->IsEquipped()) // ENABLE_BUG_FIXES
			return false;

		if (item2->GetVnum() > 55711 || item2->GetVnum() < 55701)
			return false;


		char szQuery1[1024];
		snprintf(szQuery1, sizeof(szQuery1), "SELECT duration FROM new_petsystem WHERE id = %d LIMIT 1", item2->GetID());
		std::unique_ptr<SQLMsg> pmsg2(DBManager::instance().DirectQuery(szQuery1));
		if (pmsg2->Get()->uiNumRows > 0) {
			MYSQL_ROW row = mysql_fetch_row(pmsg2->Get()->pSQLResult);
			if (atoi(row[0]) > 0) {
				if (GetNewPetSystem()->IsActivePet()) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 787, "");
#endif
					return false;
				}

				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE new_petsystem SET duration =(tduration) WHERE id = %d", item2->GetID()));
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 788, "");
#endif
			}
			else {
				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE new_petsystem SET duration =(tduration/2) WHERE id = %d", item2->GetID()));
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 788, "");
#endif
			}
			item->SetCount(item->GetCount() - 1);
			return true;
		}
		else
			return false;
	}

	if (item->GetVnum() >= 55701 && item->GetVnum() <= 55711) {
		LPITEM box = GetItem(DestCell);
		if (box) {
			if (item->GetSocket(1) == 0) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 858, "");
#endif
				return false;
			}

			if (box->GetSocket(0) != 0) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 853, "%s", box->GetName());
#endif
				return false;
			}
			else {
				if (item->GetSocket(0) == true) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 854, "");
#endif
					return false;
				}
				else {
					char query[1024];
					snprintf(query, sizeof(query), "SELECT level"
#ifdef ENABLE_NEW_PET_EDITS
						", minAge "
#endif
						", evolution, bonus0, bonus1, bonus2, skill0, skill0lv, skill1, skill1lv, skill2, skill2lv, skill3, skill3lv FROM player.new_petsystem WHERE id = %d", item->GetID());
					std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(query));
					if (pmsg->Get()->uiNumRows > 0)
					{
						MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);
						uint32_t evolution = atoi(row[2]);
						uint32_t petVnum = 0;
						switch (item->GetVnum()) {
						case 55701:
							petVnum = evolution == 3 ? 34042 : 34041;
							break;
						case 55702:
							petVnum = evolution == 3 ? 34046 : 34045;
							break;
						case 55703:
							petVnum = evolution == 3 ? 34050 : 34049;
							break;
						case 55704:
							petVnum = evolution == 3 ? 34054 : 34053;
							break;
						case 55705:
							petVnum = evolution == 3 ? 34037 : 34036;
							break;
						case 55706:
							petVnum = evolution == 3 ? 34065 : 34064;
							break;
						case 55707:
							petVnum = evolution == 3 ? 34074 : 34073;
							break;
						case 55708:
							petVnum = evolution == 3 ? 34076 : 34075;
							break;
						case 55709:
							petVnum = evolution == 3 ? 34081 : 34080;
							break;
						case 55710:
							petVnum = evolution == 3 ? 34083 : 34082;
							break;
						case 55711:
							petVnum = evolution == 3 ? 34096 : 34095;
							break;
						default:
							break;
						}

						if (petVnum == 0) {
							return false;
						}

						box->SetSocket(1, item->GetID());
						box->SetSocket(0, petVnum);
						ITEM_MANAGER::instance().RemoveItem(item);
#ifdef ENABLE_NEW_PET_EDITS
						box->SetSocket(2, atoi(row[1]));
#endif
						uint8_t res1 = atoi(row[0]);
						uint8_t res2 = atoi(row[2]);
						uint8_t res3 = atoi(row[3]);
						uint8_t res4 = atoi(row[4]);
						box->SetForceAttribute(0, res1, res2);
						box->SetForceAttribute(1, res3, res4);
						uint8_t dwskill1 = atoi(row[6]) == -1 ? 255 : atoi(row[6]), dwskilllv1 = atoi(row[7]);
						box->SetForceAttribute(2, atoi(row[5]), dwskill1);
						uint8_t dwskill2 = atoi(row[8]) == -1 ? 255 : atoi(row[8]), dwskilllv2 = atoi(row[9]);
						box->SetForceAttribute(3, dwskilllv1, dwskill2);
						uint8_t dwskill3 = atoi(row[10]) == -1 ? 255 : atoi(row[10]), dwskilllv3 = atoi(row[11]);
						box->SetForceAttribute(4, dwskilllv2, dwskill3);
						uint8_t dwskill4 = atoi(row[12]) == -1 ? 255 : atoi(row[12]), dwskilllv4 = atoi(row[13]);
						box->SetForceAttribute(5, dwskilllv3, dwskill4);
						box->SetForceAttribute(6, dwskilllv4, 1);
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 855, "%s", box->GetName());
#endif
						return true;
					}
					else {
						return false;
					}
				}
			}
		}
	}
	else if (item->GetVnum() == 55002) {
		if (item->GetSocket(0) != 0) {
			uint32_t itemVnum = 0;
			switch (item->GetSocket(0)) {
			case 34041:
			case 34042:
				itemVnum = 55701;
				break;
			case 34045:
			case 34046:
				itemVnum = 55702;
				break;
			case 34049:
			case 34050:
				itemVnum = 55703;
				break;
			case 34053:
			case 34054:
				itemVnum = 55704;
				break;
			case 34036:
			case 34037:
				itemVnum = 55705;
				break;
			case 34064:
			case 34065:
				itemVnum = 55706;
				break;
			case 34073:
			case 34074:
				itemVnum = 55707;
				break;
			case 34075:
			case 34076:
				itemVnum = 55708;
				break;
			case 34080:
			case 34081:
				itemVnum = 55709;
				break;
			case 34082:
			case 34083:
				itemVnum = 55710;
				break;
			case 34095:
			case 34096:
				itemVnum = 55711;
				break;
			default:
				break;
			}

			if (itemVnum == 0) {
				return false;
			}

			LPITEM petItem = AutoGiveItem(itemVnum, 1);
			if (!petItem) {
				return false;
			}

			petItem->SetSocket(0, 0);
			petItem->SetForceAttribute(0, 1, item->GetAttributeType(1));
			petItem->SetForceAttribute(1, 1, item->GetAttributeValue(1));
			petItem->SetForceAttribute(2, 1, item->GetAttributeType(2));

			char query[256];
			snprintf(query, sizeof(query), "SELECT tduration FROM player.new_petsystem WHERE id = %ld", item->GetSocket(1));
			std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(query));
			if (pmsg->Get()->uiNumRows > 0) {
				MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);
#ifdef ENABLE_NEW_PET_EDITS
				petItem->SetSocket(1, atoi(row[0]));
				petItem->SetSocket(2, atoi(row[0]));
#else
				petItem->SetForceAttribute(3, 1, atoi(row[0]));
				petItem->SetForceAttribute(4, 1, atoi(row[0]));
#endif
			}
#ifdef ENABLE_NEW_PET_EDITS
			petItem->SetForceAttribute(3, 1, item->GetAttributeType(0));
#else
			petItem->SetSocket(1, item->GetAttributeType(0));
#endif
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE player.new_petsystem SET id = %d WHERE id = %ld", petItem->GetID(), item->GetSocket(1)));
			ITEM_MANAGER::instance().RemoveItem(item);
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 857, "%s", item->GetName());
			return true;
#endif
		}
		else {
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 856, "%s", item->GetName());
#endif
			return false;
		}
	}
#endif

	// 30001: Teszt klonok torlese (dungeon instance-ben blokkolva)
	//switch (item->GetVnum())
	//{
	//	case 30001: return CLostCastleDungeon::instance().OnUseItem30001(this);
	//	default: break;
	//}

#ifdef ENABLE_CPP_DUNGEON_RAZOR93
	switch (item->GetVnum())
	{
	case 89103: return CRuneDungeon::instance().OnUseItem89103(this);
	case 89102: return CRuneDungeon::instance().OnUseItem89102(this);
	case 89100: return CRuneDungeon::instance().OnUseItem89100(this);
	default: break;
	}
#endif

	switch (item->GetType())
	{
#ifdef ENABLE_ITEMSHOP_ITEM
	case ITEM_TYPE_ISHOP:
	{
		uint32_t vnum = item->GetSocket(0);
		if (vnum == 0) {
			return false;
		}

		LPITEM reward = AutoGiveItem(vnum, 1);
		if (!reward) {
			return false;
		}

		item->SetCount(item->GetCount() - 1);
		return true;
	}
	break;
#endif
	case ITEM_HAIR:
		return ItemProcess_Hair(item, wDestCell);

	case ITEM_POLYMORPH:
		return ItemProcess_Polymorph(item);

	case ITEM_QUEST:
		if (GetArena() != nullptr || IsObserverMode() == true)
		{
			if (item->GetVnum() == 50051 || item->GetVnum() == 50052 || item->GetVnum() == 50053)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
		}

		if (!IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE))
		{
			if (item->GetSIGVnum() == 0)
			{
				quest::CQuestManager::instance().UseItem(GetPlayerID(), item, false);
			}
			else
			{
				quest::CQuestManager::instance().SIGUse(GetPlayerID(), item->GetSIGVnum(), item, false);
			}
		}

#ifdef __AUTO_QUQUE_ATTACK__
		if (item->GetVnum() >= 61400 && item->GetVnum() <= 61405)
		{
			if (item->isLocked() || item->IsExchanging())
				return false;

			if (FindAffect(AFFECT_AUTO_METIN_FARM)) {
				ChatPacket(CHAT_TYPE_INFO, "You has already affect.");
				return false;
			}
			ChatPacket(CHAT_TYPE_INFO, "Affect successfully added.");
			AddAffect(AFFECT_AUTO_METIN_FARM, 0, 0, AFF_NONE, item->GetValue(0) == 999 ? INFINITE_AFFECT_DURATION : 60 * 60 * 24 * item->GetValue(0), 0, false);
			item->SetCount(item->GetCount() - 1);
			return true;
		}
#endif
		break;

	case ITEM_CAMPFIRE:
	{
		float fx, fy;
		GetDeltaByDegree(GetRotation(), 100.0f, &fx, &fy);

		LPSECTREE tree = ecs::SectorAt(GetMapIndex(), (int32_t)(GetX() + fx), (int32_t)(GetY() + fy));

		if (!tree)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 344, "");
#endif
			return false;
		}

		if (tree->IsAttr((int32_t)(GetX() + fx), (int32_t)(GetY() + fy), ATTR_WATER))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 346, "");
#endif
			return false;
		}

#ifdef ENABLE_BUG_FIXES
		if (get_global_time() - GetQuestFlag("kamp.spawned") < 60) {
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 1246, "");
#endif
			return false;
		}
		else {
			SetQuestFlag("kamp.spawned", get_global_time());
		}
#endif

		auto* campfire = CHARACTER_MANAGER::instance().SpawnMob(fishing::CAMPFIRE_MOB, GetMapIndex(), (int32_t)(GetX() + fx), (int32_t)(GetY() + fy), 0, false, number(0, 359));

		char_event_info* info = AllocEventInfo<char_event_info>();

		info->ch = campfire;

		campfire->m_pkMiningEvent = event_create(kill_campfire_event, info, PASSES_PER_SEC(40));

		item->SetCount(item->GetCount() - 1);
	}
	break;

	case ITEM_UNIQUE:
	{
		switch (item->GetSubType())
		{
		case USE_ABILITY_UP:
		{
			switch (item->GetValue(0))
			{
			case APPLY_MOV_SPEED:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_MOV_SPEED, item->GetValue(2), AFF_MOV_SPEED_POTION, item->GetValue(1), 0, true, true);
				break;

			case APPLY_ATT_SPEED:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_ATT_SPEED, item->GetValue(2), AFF_ATT_SPEED_POTION, item->GetValue(1), 0, true, true);
				break;

			case APPLY_STR:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_ST, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_DEX:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_DX, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_CON:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_HT, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_INT:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_IQ, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_CAST_SPEED:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_CASTING_SPEED, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_RESIST_MAGIC:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_RESIST_MAGIC, item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_ATT_GRADE_BONUS:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_ATT_GRADE_BONUS,
					item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;

			case APPLY_DEF_GRADE_BONUS:
				AddAffect(AFFECT_UNIQUE_ABILITY, POINT_DEF_GRADE_BONUS,
					item->GetValue(2), 0, item->GetValue(1), 0, true, true);
				break;
			}
		}

		if (GetWarMap())
			GetWarMap()->UsePotion(this, item);

		item->SetCount(item->GetCount() - 1);
		break;

		default:
		{
			if (item->GetSubType() == USE_SPECIAL)
			{
				sys_log(0, "ITEM_UNIQUE: USE_SPECIAL %u", item->GetVnum());

				switch (item->GetVnum())
				{
				case 71049: // ºñ´Üº¸µû¸®
#ifdef KASMIR_PAKET_SYSTEM
				case 88901:
#endif
					if (g_bEnableBootaryCheck)
					{
						if (IS_BOTARYABLE_ZONE(GetMapIndex()) == true)
						{
#ifdef KASMIR_PAKET_SYSTEM
							m_bKasmirPaketDurum = item->GetVnum() == 88901 ? true : false;
#endif

							UseSilkBotary();
						}
#ifdef TEXTS_IMPROVEMENT
						else {
							ChatPacketNew(CHAT_TYPE_INFO, 668, "");
						}
#endif
					}
					else
					{
#ifdef KASMIR_PAKET_SYSTEM
						m_bKasmirPaketDurum = item->GetVnum() == 88901 ? true : false;
#endif

						UseSilkBotary();
					}
					break;
				}
			}
			else
			{
				if (!item->IsEquipped())
					EquipItem(item);
				else
					UnequipItem(item);
			}
		}
		break;
		}
	}
	break;

	case ITEM_COSTUME:
	case ITEM_WEAPON:
	case ITEM_ARMOR:
	case ITEM_ROD:
	case ITEM_RING:		// ½Å±Ô ¹ÝÁö ¾ÆÀÌÅÛ
	case ITEM_BELT:		// ½Å±Ô º§Æ® ¾ÆÀÌÅÛ
		//ChatPacket(CHAT_TYPE_INFO, "You can put in your Mount inventory");
		// MINING
	case ITEM_PICK:
		// END_OF_MINING
		if (!item->IsEquipped())
			EquipItem(item);
		else
			UnequipItem(item);
		break;
		// Âø¿ëÇÏÁö ¾ÊÀº ¿ëÈ¥¼®Àº »ç¿ëÇÒ ¼ö ¾ø´Ù.
		// Á¤»óÀûÀÎ Å¬¶ó¶ó¸é, ¿ëÈ¥¼®¿¡ °üÇÏ¿© item use ÆÐÅ¶À» º¸³¾ ¼ö ¾ø´Ù.
		// ¿ëÈ¥¼® Âø¿ëÀº item move ÆÐÅ¶À¸·Î ÇÑ´Ù.
		// Âø¿ëÇÑ ¿ëÈ¥¼®Àº ÃßÃâÇÑ´Ù.
	case ITEM_DS:
	{
		if (!item->IsEquipped())
			return false;
		return DSManager::instance().PullOut(this, NPOS, item);
		break;
	}
	case ITEM_SPECIAL_DS:
		if (!item->IsEquipped())
			EquipItem(item);
		else
			UnequipItem(item);
		break;

	case ITEM_FISH:
	{
		if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
			return false;
		}
#ifdef ENABLE_NEWSTUFF
		else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
			return false;
		}
#endif

		if (item->GetSubType() == FISH_ALIVE)
			fishing::UseFish(this, item);
	}
	break;

	case ITEM_TREASURE_BOX:
	{
		return false;
	}
	break;

	case ITEM_TREASURE_KEY:
	{
		LPITEM item2;

		if (!GetItem(DestCell) || !(item2 = GetItem(DestCell)))
			return false;

		if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
			return false;

		if (item2->GetType() != ITEM_TREASURE_BOX)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 408, "");
#endif
			return false;
		}

		if (item->GetValue(0) == item2->GetValue(0))
		{
			uint32_t dwBoxVnum = item2->GetVnum();
			std::vector <uint32_t> dwVnums;
			std::vector <uint32_t> dwCounts;
			std::vector <LPITEM> item_gets(0);
			int count = 0;

			if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
			{
				ITEM_MANAGER::instance().RemoveItem(item);
				ITEM_MANAGER::instance().RemoveItem(item2);

				for (int i = 0; i < count; i++) {
					switch (dwVnums[i])
					{
					case CSpecialItemGroup::GOLD:
						break;
					case CSpecialItemGroup::EXP:
						break;
					case CSpecialItemGroup::MOB:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 378, "");
#endif
						break;
					case CSpecialItemGroup::SLOW:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 377, "");
#endif
						break;
					case CSpecialItemGroup::DRAIN_HP:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 373, "");
#endif
						break;
					case CSpecialItemGroup::POISON:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 376, "");
#endif
						break;
#ifdef ENABLE_WOLFMAN_CHARACTER
					case CSpecialItemGroup::BLEEDING:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 379, "");
#endif
						break;
#endif
					case CSpecialItemGroup::MOB_GROUP:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 380, "");
#endif
						break;
					default:
						//#ifdef TEXTS_IMPROVEMENT
						//									if (item_gets[i]) {
						//										if (dwCounts[i] > 1) {
						//											ChatPacketNew(CHAT_TYPE_INFO, 374, "%d#%s", dwCounts[i], item_gets[i]->GetName());
						//										} else {
						//											ChatPacketNew(CHAT_TYPE_INFO, 375, "%s", item_gets[i]->GetName());
						//										}
						//									}
						//#endif
						break;
					}
				}
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 408, "");
#endif
				return false;
			}
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 408, "");
#endif
			return false;
		}
	}
	break;

	case ITEM_GIFTBOX:
	{
#ifdef ENABLE_NEWSTUFF
		if (0 != g_BoxUseTimeLimitValue)
		{
			if (get_dword_time() < m_dwLastBoxUseTime + g_BoxUseTimeLimitValue)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 510, "");
#endif
				return false;
			}
		}

		m_dwLastBoxUseTime = get_dword_time();
#endif
		uint32_t dwBoxVnum = item->GetVnum();

		std::vector <uint32_t> dwVnums;
		std::vector <uint32_t> dwCounts;
		std::vector <LPITEM> item_gets(0);
		int count = 0;

		if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
		{
			item->SetCount(item->GetCount() - 1);
#ifdef ENABLE_RANKING
			SetRankPoints(17, GetRankPoints(17) + 1);
#endif

			for (int i = 0; i < count; i++) {
				switch (dwVnums[i])
				{
				case CSpecialItemGroup::GOLD:
					break;
				case CSpecialItemGroup::EXP:
					break;
				case CSpecialItemGroup::MOB:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 378, "");
#endif
					break;
				case CSpecialItemGroup::SLOW:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 377, "");
#endif
					break;
				case CSpecialItemGroup::DRAIN_HP:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 373, "");
#endif
					break;
				case CSpecialItemGroup::POISON:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 376, "");
#endif
					break;
#ifdef ENABLE_WOLFMAN_CHARACTER
				case CSpecialItemGroup::BLEEDING:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 379, "");
#endif
					break;
#endif
				case CSpecialItemGroup::MOB_GROUP:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 380, "");
#endif
					break;
				default:
					//#ifdef TEXTS_IMPROVEMENT
					//							if (item_gets[i]) {
					//								if (dwCounts[i] > 1) {
					//									ChatPacketNew(CHAT_TYPE_INFO, 374, "%d#%s", dwCounts[i], item_gets[i]->GetName());
					//								} else {
					//									ChatPacketNew(CHAT_TYPE_INFO, 375, "%s", item_gets[i]->GetName());
					//								}
					//							}
					//#endif
					break;
				}
			}
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 395, "");
#endif
			return false;
		}
	}
	break;

	case ITEM_SKILLFORGET:
	{
		if (!item->GetSocket(0))
		{
			ITEM_MANAGER::instance().RemoveItem(item);
			return false;
		}

		uint32_t dwVnum = item->GetSocket(0);

		if (SkillLevelDown(dwVnum)) {
			ITEM_MANAGER::instance().RemoveItem(item);
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 399, "");
#endif
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ChatPacketNew(CHAT_TYPE_INFO, 400, "");
		}
#endif
	}
	break;

	case ITEM_SKILLBOOK:
	{
		if (item->GetVnum() == 55003 || item->GetVnum() == 55004 || item->GetVnum() == 55005) {
			return false;
		}

		if (IsPolymorphed())
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
			return false;
		}

		uint32_t dwVnum = 0;
		if (item->GetVnum() == 50300)
		{
			dwVnum = item->GetSocket(0);
		}
		else
		{
			dwVnum = item->GetValue(0);
		}

		dwVnum = item->GetVnum() == 50301 || item->GetVnum() == 50302 || item->GetVnum() == 50303 ? SKILL_LEADERSHIP : dwVnum;

		if (0 == dwVnum)
		{
			ITEM_MANAGER::instance().RemoveItem(item);

			return false;
		}

		if (dwVnum == SKILL_LEADERSHIP) {
			int lv = GetSkillLevel(SKILL_LEADERSHIP);
			if (lv < item->GetValue(0)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 429, "");
#endif
				return false;
			}

			if (lv >= item->GetValue(1)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 430, "");
#endif
				return false;
			}
		}

		if (true == LearnSkillByBook(dwVnum))
		{
#ifdef ENABLE_BOOKS_STACKFIX
			item->SetCount(item->GetCount() - 1);
#else
			ITEM_MANAGER::instance().RemoveItem(item);
#endif
			int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
			SetSkillNextReadTime(dwVnum, dwVnum == SKILL_LEADERSHIP ? get_global_time() + 18000 : get_global_time() + iReadDelay);
		}
	}
	break;
#ifdef ENABLE_NEW_PET_EDITS
	case ITEM_TYPE_PET:
	{
		if (!GetNewPetSystem())
			return false;

		if (GetNewPetSystem()->IsActivePet()) {
			GetNewPetSystem()->IncreasePetSkillByBook(item);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ChatPacketNew(CHAT_TYPE_INFO, 53, "");
		}
#endif
	}
	break;
#endif
	case ITEM_USE:
	{
		if (item->GetVnum() > 50800 && item->GetVnum() <= 50820)
		{
			if (test_server)
				sys_log(0, "ADD addtional effect : vnum(%d) subtype(%d)", item->GetOriginalVnum(), item->GetSubType());

			int affect_type = AFFECT_EXP_BONUS_EURO_FREE;
			int apply_type = aApplyInfo[item->GetValue(0)].bPointType;
			int apply_value = item->GetValue(2);
			int apply_duration = item->GetValue(1);

			switch (item->GetSubType())
			{
			case USE_ABILITY_UP:
				if (FindAffect(affect_type, apply_type))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
					return false;
				}

				{
					switch (item->GetValue(0))
					{
					case APPLY_MOV_SPEED:
						AddAffect(affect_type, apply_type, apply_value, AFF_MOV_SPEED_POTION, apply_duration, 0, true, true);
						break;

					case APPLY_ATT_SPEED:
						AddAffect(affect_type, apply_type, apply_value, AFF_ATT_SPEED_POTION, apply_duration, 0, true, true);
						break;

					case APPLY_STR:
					case APPLY_DEX:
					case APPLY_CON:
					case APPLY_INT:
					case APPLY_CAST_SPEED:
					case APPLY_RESIST_MAGIC:
					case APPLY_ATT_GRADE_BONUS:
					case APPLY_DEF_GRADE_BONUS:
						AddAffect(affect_type, apply_type, apply_value, 0, apply_duration, 0, true, true);
						break;
					}
				}

				if (GetWarMap())
					GetWarMap()->UsePotion(this, item);

				item->SetCount(item->GetCount() - 1);
				break;

			case USE_AFFECT:
			{
				if (FindAffect(AFFECT_EXP_BONUS_EURO_FREE, aApplyInfo[item->GetValue(1)].bPointType))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
				}
				else
				{
					// PC_BANG_ITEM_ADD
					if (item->IsPCBangItem() == true)
					{
						// PC¹æÀÎÁö Ã¼Å©ÇØ¼­ Ã³¸®
						if (CPCBangManager::instance().IsPCBangIP(GetDesc()->GetHostName()) == false)
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 426, "");
#endif
							return false;
						}
					}
					// END_PC_BANG_ITEM_ADD

					AddAffect(AFFECT_EXP_BONUS_EURO_FREE, aApplyInfo[item->GetValue(1)].bPointType, item->GetValue(2), 0, item->GetValue(3), 0, false, true);
					item->SetCount(item->GetCount() - 1);
				}
			}
			break;
			case USE_POTION_NODELAY:
			{
				if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
				{
					if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 303, "");
#endif
						return false;
					}

					switch (item->GetVnum())
					{
					case 70020:
					case 71018:
					case 71019:
					case 71020:
						if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
						{
							if (m_nPotionLimit <= 0)
							{
#ifdef TEXTS_IMPROVEMENT
								ChatPacketNew(CHAT_TYPE_INFO, 362, "");
#endif
								return false;
							}
						}
						break;

					default:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 303, "");
#endif
						return false;
						break;
					}
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				bool used = false;

				if (item->GetValue(0) != 0) // HP Àý´ë°ª È¸º¹
				{
					if (GetHP() < GetMaxHP())
					{
						PointChange(POINT_HP, item->GetValue(0) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
						EffectPacket(SE_HPUP_RED);
						used = true;
					}
				}

				if (item->GetValue(1) != 0)	// SP Àý´ë°ª È¸º¹
				{
					if (GetSP() < GetMaxSP())
					{
						PointChange(POINT_SP, item->GetValue(1) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
						EffectPacket(SE_SPUP_BLUE);
						used = true;
					}
				}

				if (item->GetValue(3) != 0) // HP % È¸º¹
				{
					if (GetHP() < GetMaxHP())
					{
						PointChange(POINT_HP, item->GetValue(3) * GetMaxHP() / 100);
						EffectPacket(SE_HPUP_RED);
						used = true;
					}
				}

				if (item->GetValue(4) != 0) // SP % È¸º¹
				{
					if (GetSP() < GetMaxSP())
					{
						PointChange(POINT_SP, item->GetValue(4) * GetMaxSP() / 100);
						EffectPacket(SE_SPUP_BLUE);
						used = true;
					}
				}

				if (used)
				{
					if (item->GetVnum() == 50085 || item->GetVnum() == 50086) {
						SetUseSeedOrMoonBottleTime();
					}

					if (GetWarMap())
						GetWarMap()->UsePotion(this, item);

					m_nPotionLimit--;

					//RESTRICT_USE_SEED_OR_MOONBOTTLE
					item->SetCount(item->GetCount() - 1);
					//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
				}
			}
			break;
			}

			return true;
		}


		if (item->GetVnum() >= 27863 && item->GetVnum() <= 27883)
		{
			if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif
		}

		if (test_server)
		{
			sys_log(0, "USE_ITEM %s Type %d SubType %d vnum %d", item->GetName(), item->GetType(), item->GetSubType(), item->GetOriginalVnum());
		}

		switch (item->GetSubType())
		{
		case USE_FISH:
		{
			CAffect* pAffect = nullptr;
			int type = 0, duration = item->GetValue(0);
			for (int i = 0; i < ITEM_APPLY_MAX_NUM; i++) {
				type = aApplyInfo[item->GetApplyType(i)].bPointType;
				if (type != 0) {
					pAffect = FindAffect(AFFECT_FISH_BONUS, type);
				}
			}

			if (pAffect != nullptr) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 893, "");
#endif
				return false;
			}
			else {
				item->SetCount(item->GetCount() - 1);

				for (int i = 0; i < ITEM_APPLY_MAX_NUM; i++) {
					type = item->GetApplyType(i);
					if (type != 0) {
						AddAffect(AFFECT_FISH_BONUS, aApplyInfo[type].bPointType, item->GetApplyValue(i), item->GetID(), duration, 0, false, false);
					}
				}
			}
			break;
		}
		case USE_TIME_CHARGE_PER:
		{
			LPITEM pDestItem = GetItem(DestCell);
			if (nullptr == pDestItem)
			{
				return false;
			}
			// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
			if (pDestItem->IsDragonSoul())
			{
#ifdef ENABLE_DS_POTION_DIFFRENT
				if (item->GetCount() > 1) {
					int pos = GetEmptyInventory(item->GetSize());
					if (pos == -1) {
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
						return false;
					}

					item->SetCount(item->GetCount() - 1);
					LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), 1);
					if (!item2)
						return false;

					item2->AddToCharacter(this, TItemPos(INVENTORY, pos), false);
					item = item2;
				}

				if (item->GetSocket(0) <= 0) {
					item->RemoveFromCharacter();
					return false;
				}
				else {
					uint32_t duration = DSManager::instance().GetDuration(pDestItem);
					uint32_t remain_sec = pDestItem->GetSocket(ITEM_SOCKET_REMAIN_SEC);
					if (remain_sec == duration)
						return false;

					uint32_t dwBottlePercent = item->GetSocket(0);
					uint32_t dwOnePercent = duration / 100;
					uint32_t dwRemainPercent = remain_sec / dwOnePercent;
					uint32_t dif = 100 - dwRemainPercent;
					dif = dif > dwBottlePercent ? dwBottlePercent : dif;
					uint32_t add = dwOnePercent * dif;
					if (remain_sec + add >= 86400) {
						pDestItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, duration);
					}
					else {
						pDestItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, remain_sec + add);
					}

					item->SetSocket(0, dwBottlePercent - dif);
					if (item->GetSocket(0) < 1)
						item->RemoveFromCharacter();

					return true;
				}
#else
				int ret;
				char buf[128];
				if (item->GetVnum() == DRAGON_HEART_VNUM)
				{
					ret = pDestItem->GiveMoreTime_Per((float)item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
				}
				else
				{
					ret = pDestItem->GiveMoreTime_Per((float)item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
				}
				if (ret > 0)
				{
					if (item->GetVnum() == DRAGON_HEART_VNUM)
					{
						sprintf(buf, "Inc %ds by item{VN:%d SOC%d:%ld}", ret, item->GetVnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
					}
					else
					{
						sprintf(buf, "Inc %ds by item{VN:%d VAL%d:%ld}", ret, item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
					}

#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 670, "%s#%d", pDestItem->GetName(), ret);
#endif
					item->SetCount(item->GetCount() - 1);
					LogManager::instance().ItemLog(this, item, "DS_CHARGING_SUCCESS", buf);
					return true;
				}
				else
				{
					if (item->GetVnum() == DRAGON_HEART_VNUM)
					{
						sprintf(buf, "No change by item{VN:%d SOC%d:%ld}", item->GetVnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
					}
					else
					{
						sprintf(buf, "No change by item{VN:%d VAL%d:%ld}", item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
					}

#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 671, "%s", pDestItem->GetName());
#endif
					LogManager::instance().ItemLog(this, item, "DS_CHARGING_FAILED", buf);
					return false;
				}
#endif
			}
			else
				return false;
		}
		break;
		case USE_TIME_CHARGE_FIX:
		{
			LPITEM pDestItem = GetItem(DestCell);
			if (nullptr == pDestItem)
			{
				return false;
			}
			// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
			if (pDestItem->IsDragonSoul())
			{
				int ret = pDestItem->GiveMoreTime_Fix(item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
				char buf[128];
				if (ret)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 670, "%s#%d", pDestItem->GetName(), ret);
#endif
					sprintf(buf, "Increase %ds by item{VN:%d VAL%d:%ld}", ret, item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
					LogManager::instance().ItemLog(this, item, "DS_CHARGING_SUCCESS", buf);
					item->SetCount(item->GetCount() - 1);
					return true;
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 671, "%s", pDestItem->GetName());
#endif
					sprintf(buf, "No change by item{VN:%d VAL%d:%ld}", item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
					LogManager::instance().ItemLog(this, item, "DS_CHARGING_FAILED", buf);
					return false;
				}
			}
			else
				return false;
		}
		break;
#ifdef ENABLE_NEW_USE_POTION
		case USE_NEW_POTIION: {
			uint32_t dwType = item->GetValue(0);
			if (dwType >= AFFECT_NEW_POTION24 && dwType <= AFFECT_NEW_POTION29 && !marriage::CManager::instance().IsMarried(GetPlayerID())) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 891, "");
#endif
				return false;
			}

			if (dwType == AFFECT_NEW_POTION31) {
				LPPARTY party = GetParty();
				if ((!party) || (party && GetPlayerID() != party->GetLeaderPID())) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 902, "");
#endif
					return false;
				}
			}

			CAffect* pAffect = FindAffect(dwType);
			if (pAffect && item->GetID() != pAffect->dwFlag)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#else
				ChatPacket(CHAT_TYPE_INFO, "Már aktiv egy ilyen harmat.");
#endif
				return false;
			}

			if (item->GetCount() > 1)
			{
#ifdef ENABLE_EXTRA_INVENTORY
				const bool bFromExtraInventory = (item->GetWindow() == EXTRA_INVENTORY);
#else
				const bool bFromExtraInventory = false;
#endif
				int pos = -1;

#ifdef ENABLE_EXTRA_INVENTORY
				if (bFromExtraInventory)
					pos = GetEmptyExtraInventory(item);
				else
#endif
					pos = GetEmptyInventory(item->GetSize());

				if (pos == -1) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
					break;
				}

				item->SetCount(item->GetCount() - 1);
				LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), 1);
				if (!item2)
					return true;

#ifdef ENABLE_EXTRA_INVENTORY
				if (bFromExtraInventory)
					item2->AddToCharacter(this, TItemPos(EXTRA_INVENTORY, pos), false);
				else
#endif
					item2->AddToCharacter(this, TItemPos(INVENTORY, pos), false);

				item = item2;
			}

			uint8_t bApplyOn = item->GetApplyType(0);
			int32_t lApplyValue = item->GetApplyValue(0);

			pAffect = FindAffect(dwType);
			if (pAffect) {
				uint32_t dwItemID = pAffect->dwFlag;
				if (item->GetID() == dwItemID) {
					item->Lock(false);
					item->SetSocket(1, 0);
					RemoveAffect(dwType);
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 28, "%s", item->GetName());
#endif
				}
				else {
					LPITEM pkItem = FindItemByID(dwItemID);
					if (pkItem) {
						pkItem->Lock(false);
						pkItem->SetSocket(1, 0);
					}

					RemoveAffect(dwType);
					item->Lock(true);
					item->SetSocket(1, 1);
					AddAffect(dwType, aApplyInfo[bApplyOn].bPointType, lApplyValue, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 29, "%s", item->GetName());
#endif
				}
			}
			else {
				if (dwType == AFFECT_NEW_POTION19) {
					pAffect = FindAffect(AFFECT_NEW_POTION20);
					if (pAffect) {
						LPITEM pkItem = FindItemByID(pAffect->dwFlag);
						if (pkItem) {
							pkItem->Lock(false);
							pkItem->SetSocket(1, 0);
						}

						RemoveAffect(AFFECT_NEW_POTION20);
					}
				}
				else if (dwType == AFFECT_NEW_POTION20) {
					pAffect = FindAffect(AFFECT_NEW_POTION19);
					if (pAffect) {
						LPITEM pkItem = FindItemByID(pAffect->dwFlag);
						if (pkItem) {
							pkItem->Lock(false);
							pkItem->SetSocket(1, 0);
						}

						RemoveAffect(AFFECT_NEW_POTION19);
					}
				}
				else if (dwType == AFFECT_NEW_POTION21) {
					pAffect = FindAffect(AFFECT_NEW_POTION22);
					if (pAffect) {
						LPITEM pkItem = FindItemByID(pAffect->dwFlag);
						if (pkItem) {
							pkItem->Lock(false);
							pkItem->SetSocket(1, 0);
						}

						RemoveAffect(AFFECT_NEW_POTION22);
					}
				}
				else if (dwType == AFFECT_NEW_POTION22) {
					pAffect = FindAffect(AFFECT_NEW_POTION21);
					if (pAffect) {
						LPITEM pkItem = FindItemByID(pAffect->dwFlag);
						if (pkItem) {
							pkItem->Lock(false);
							pkItem->SetSocket(1, 0);
						}

						RemoveAffect(AFFECT_NEW_POTION21);
					}
				}

				item->Lock(true);
				item->SetSocket(1, 1);
				AddAffect(dwType, aApplyInfo[bApplyOn].bPointType, lApplyValue, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 29, "%s", item->GetName());
#endif
			}
		}
							break;
#endif
		case USE_SPECIAL:

			switch (item->GetVnum())
			{
				//Å©¸®½º¸¶½º ¶õÁÖ
			case ITEM_NOG_POCKET:
			{
				/*
				¶õÁÖ´É·ÂÄ¡ : item_proto value ÀÇ¹Ì
					ÀÌµ¿¼Óµµ  value 1
					°ø°Ý·Â	  value 2
					°æÇèÄ¡    value 3
					Áö¼Ó½Ã°£  value 0 (´ÜÀ§ ÃÊ)

				*/
				if (FindAffect(AFFECT_NOG_ABILITY))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
					return false;
				}
				int32_t time = item->GetValue(0);
				int32_t moveSpeedPer = item->GetValue(1);
				int32_t attPer = item->GetValue(2);
				int32_t expPer = item->GetValue(3);
				AddAffect(AFFECT_NOG_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
				AddAffect(AFFECT_NOG_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
				AddAffect(AFFECT_NOG_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
				item->SetCount(item->GetCount() - 1);
			}
			break;

			//¶ó¸¶´Ü¿ë »çÅÁ
			case ITEM_RAMADAN_CANDY:
			{
				/*
				»çÅÁ´É·ÂÄ¡ : item_proto value ÀÇ¹Ì
					ÀÌµ¿¼Óµµ  value 1
					°ø°Ý·Â	  value 2
					°æÇèÄ¡    value 3
					Áö¼Ó½Ã°£  value 0 (´ÜÀ§ ÃÊ)

				*/
				// @fixme147 BEGIN
				if (FindAffect(AFFECT_RAMADAN_ABILITY))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
					return false;
				}
				// @fixme147 END
				int32_t time = item->GetValue(0);
				int32_t moveSpeedPer = item->GetValue(1);
				int32_t attPer = item->GetValue(2);
				int32_t expPer = item->GetValue(3);
				AddAffect(AFFECT_RAMADAN_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
				AddAffect(AFFECT_RAMADAN_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
				AddAffect(AFFECT_RAMADAN_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
				item->SetCount(item->GetCount() - 1);
			}
			break;
			case ITEM_MARRIAGE_RING:
			{
				marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(GetPlayerID());
				if (pMarriage)
				{
					if (pMarriage->ch1 != nullptr)
					{
						if (CArenaManager::instance().IsArenaMap(pMarriage->ch1->GetMapIndex()) == true)
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 672, "");
#endif
							break;
						}
					}

					if (pMarriage->ch2 != nullptr)
					{
						if (CArenaManager::instance().IsArenaMap(pMarriage->ch2->GetMapIndex()) == true)
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 672, "");
#endif
							break;
						}
					}

					int consumeSP = CalculateConsumeSP(this);

					if (consumeSP < 0)
						return false;

					PointChange(POINT_SP, -consumeSP, false);

					WarpToPID(pMarriage->GetOther(GetPlayerID()));
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 242, "");
				}
#endif
			}
			break;

			//±âÁ¸ ¿ë±âÀÇ ¸ÁÅä
			case UNIQUE_ITEM_CAPE_OF_COURAGE:
				// {
					// if (GetMapIndex() != 1)
					// {
	// #ifdef TEXTS_IMPROVEMENT
						// ChatPacketNew(CHAT_TYPE_INFO, 489, "");
	// #endif
						// return true;
					// }


				// }
				// break;
			case 70057:
			case REWARD_BOX_UNIQUE_ITEM_CAPE_OF_COURAGE:
#ifdef __EFFETTO_MANTELLO__
				if (GetMapIndex() != 1)
				{
					this->EffectPacket(SE_MANTELLO);
					AggregateMonster();

				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 489, "");
					return false;
#endif
				}
#endif
#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
			case 70030:
#ifdef __EFFETTO_MANTELLO__
				if (GetMapIndex() != 1)
				{
					this->EffectPacket(SE_MANTELLO);
					AggregateMonsterPlus();

				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 489, "");
					return false;
#endif
				}

#endif
#endif
				item->SetCount(item->GetCount() - 1);//@Razor93 (batorsag kopi fogyjon)
				//UpdateMountCountOverhead(this);
				break;

			case UNIQUE_ITEM_WHITE_FLAG:
				ForgetMyAttacker();
				item->SetCount(item->GetCount() - 1);
				break;

			case UNIQUE_ITEM_TREASURE_BOX:
				break;
#ifdef ENABLE_BATTLE_PASS
#ifdef ENABLE_FREE_PASS_RAZOR93
#ifdef ENABLE_BATTLE_PASS
			case 70611:
			{
				const uint8_t bBattlePassId = GetBattlePassId();
				if (!bBattlePassId)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 780, "");  
#endif
					return false;
				}

				// 1x hasznalhato ugyanarra a BP ID-re
				if (HasBattlePassBoost(bBattlePassId))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 8, ""); 
#endif
					return false;
				}

				int remain = (int)(m_dwBattlePassEndTime - get_global_time());
				if (remain <= 0)
					remain = GetSecondsTillNextMonth();

				 
				AddAffect(AFFECT_BATTLE_PASS_BOOST, POINT_BATTLE_PASS_ID, bBattlePassId, 0, remain, 0, true);

				 
				ApplyBattlePassBoostRecalc(bBattlePassId);

				 
				CBattlePass::instance().BattlePassRequestOpen(this);

				item->SetCount(item->GetCount() - 1);
			}
			break;
#endif

#else

			case 70611://79900
			{
				char szQuery[1024];
				snprintf(szQuery, sizeof(szQuery), "SELECT * FROM battle_pass_ranking WHERE player_name = '%s' AND battle_pass_id = %d;", GetName(), 1);
				std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(szQuery));
				if (pmsg->Get()->uiNumRows > 0) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 6, "");
#endif
					return false;
				}

				int iSeconds = GetSecondsTillNextMonth();
				if (iSeconds < 0) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 7, "");
#endif
					return false;
				}

				if (FindAffect(AFFECT_BATTLE_PASS)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 8, "");
#endif
					return false;
				}
				else {
					m_dwBattlePassEndTime = get_global_time() + iSeconds;

					AddAffect(AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID, 1, 0, iSeconds, 0, true);
					item->SetCount(item->GetCount() - 1);
				}
			}
			break;
#endif
#endif
			case 27989: // ¿µ¼®°¨Áö±â
			case 76006: // ¼±¹°¿ë ¿µ¼®°¨Áö±â
			{
				LPSECTREE_MAP pMap = ecs::GetMap(GetMapIndex());

				if (pMap != nullptr)
				{
					item->SetSocket(0, item->GetSocket(0) + 1);

					FFindStone f;

					// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
					pMap->for_each(f);

					if (f.m_mapStone.size() > 0)
					{
						auto stone = f.m_mapStone.begin();

						uint32_t max = UINT_MAX;
						auto* pTarget = stone->second;

						while (stone != f.m_mapStone.end())
						{
							uint32_t dist = (uint32_t)DISTANCE_SQRT(GetX() - stone->second->GetX(), GetY() - stone->second->GetY());

							if (dist != 0 && max > dist)
							{
								max = dist;
								pTarget = stone->second;
							}
							stone++;
						}

						if (pTarget != nullptr)
						{
							int val = 3;

							if (max < 10000) val = 2;
							else if (max < 70000) val = 1;

							ChatPacket(CHAT_TYPE_COMMAND, "StoneDetect %u %d %d", GetPacketVID(), val,
								(int)GetDegreeFromPositionXY(GetX(), pTarget->GetY(), pTarget->GetX(), GetY()));
						}
#ifdef TEXTS_IMPROVEMENT
						else {
							ChatPacketNew(CHAT_TYPE_INFO, 673, "");
						}
#endif
					}
#ifdef TEXTS_IMPROVEMENT
					else {
						ChatPacketNew(CHAT_TYPE_INFO, 673, "");
					}
#endif

					if (item->GetSocket(0) >= 6)
					{
						ChatPacket(CHAT_TYPE_COMMAND, "StoneDetect %u 0 0", GetPacketVID());
						ITEM_MANAGER::instance().RemoveItem(item);
					}
				}
				break;
			}
			break;

			case 27996: // µ¶º´
				item->SetCount(item->GetCount() - 1);
				AttackedByPoison(nullptr); // @warme008
				break;

			case 27987: // Á¶°³
				// 50  µ¹Á¶°¢ 47990
				// 30  ²Î
				// 10  ¹éÁøÁÖ 47992
				// 7   Ã»ÁøÁÖ 47993
				// 3   ÇÇÁøÁÖ 47994
			{
				item->SetCount(item->GetCount() - 1);

				int r = number(1, 100);

				if (r <= 50)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 458, "");
#endif
					AutoGiveItem(27990);
				}
				else
				{
					const int prob_table_gb2312[] =
					{
						95, 97, 99
					};

					const int* prob_table = prob_table_gb2312;

					if (r <= prob_table[0]) {
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 457, "");
#endif
					}
					else if (r <= prob_table[1])
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 459, "");
#endif
						AutoGiveItem(27992);
					}
					else if (r <= prob_table[2])
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 460, "");
#endif
						AutoGiveItem(27993);
					}
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 461, "");
#endif
						AutoGiveItem(27994);
					}
				}
			}
			break;

			case 71013: // ÃàÁ¦¿ëÆøÁ×
				CreateFly(number(FLY_FIREWORK1, FLY_FIREWORK6), this);
				item->SetCount(item->GetCount() - 1);
				break;

			case 50100: // ÆøÁ×
			case 50101:
			case 50102:
			case 50103:
			case 50104:
			case 50105:
			case 50106:
				CreateFly(item->GetVnum() - 50100 + FLY_FIREWORK1, this);
				item->SetCount(item->GetCount() - 1);
				break;

			case 50200: // º¸µû¸®
				if (g_bEnableBootaryCheck)
				{
					if (IS_BOTARYABLE_ZONE(GetMapIndex()) == true)
					{
#ifdef KASMIR_PAKET_SYSTEM
						m_bKasmirPaketDurum = false;
#endif
						__OpenPrivateShop();
					}
#ifdef TEXTS_IMPROVEMENT
					else {
						ChatPacketNew(CHAT_TYPE_INFO, 668, "");
					}
#endif
				}
				else
				{
#ifdef KASMIR_PAKET_SYSTEM
					m_bKasmirPaketDurum = false;
#endif
					__OpenPrivateShop();
				}
				break;

			case fishing::FISH_MIND_PILL_VNUM:
			{
#ifdef ENABLE_NEW_FISHING_SYSTEM
				if (FindAffect(AFFECT_FISH_MIND_PILL)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 900, "");
#endif
					return false;
				}
#endif

				AddAffect(AFFECT_FISH_MIND_PILL, POINT_NONE, 0, AFF_FISH_MIND, 20 * 60, 0, true);
				item->SetCount(item->GetCount() - 1);
			}
			break;

			case 50304: // ¿¬°è±â ¼ö·Ã¼­
			case 50305:
			case 50306:
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				if (GetSkillLevel(SKILL_COMBO) == 0 && GetLevel() < 30)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 322, "");
#endif
					return false;
				}

				if (GetSkillLevel(SKILL_COMBO) == 1 && GetLevel() < 50)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 323, "");
#endif
					return false;
				}

				if (GetSkillLevel(SKILL_COMBO) >= 2)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 324, "");
#endif
					return false;
				}

				int iPct = item->GetValue(0);

				if (LearnSkillByBook(SKILL_COMBO, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(SKILL_COMBO, get_global_time() + iReadDelay);
				}
			}
			break;

#ifdef ENABLE_NEW_SECONDARY_SKILLS
			case 50333:
			case 50334:
			case 50335:
			case 50336: {
				if (IsPolymorphed()) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}

				uint32_t dwSkillVnum = item->GetValue(0);
				if (GetSkillLevel(dwSkillVnum) >= 10) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 439, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, 0)) {
					item->SetCount(item->GetCount() - 1);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + 10800);
				}
			}
					  break;
#endif

			case 50311: // ¾ð¾î ¼ö·Ã¼­
			case 50312:
			case 50313:
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = item->GetValue(0);
				int iPct = MINMAX(0, item->GetValue(1), 100);
				if (GetSkillLevel(dwSkillVnum) >= 20 || dwSkillVnum - SKILL_LANGUAGE1 + 1 == GetEmpire())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 439, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;

			case 50061: // ÀÏº» ¸» ¼ÒÈ¯ ½ºÅ³ ¼ö·Ã¼­
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = item->GetValue(0);
				int iPct = MINMAX(0, item->GetValue(1), 100);

				if (GetSkillLevel(dwSkillVnum) >= 10)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;

			case 50314: case 50315: case 50316: // º¯½Å ¼ö·Ã¼­
			case 50323: case 50324: // ÁõÇ÷ ¼ö·Ã¼­
			case 50325: case 50326: // Ã¶Åë ¼ö·Ã¼­
			{
				if (IsPolymorphed() == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;
				}

				int iSkillLevelLowLimit = item->GetValue(0);
				int iSkillLevelHighLimit = item->GetValue(1);
				int iPct = MINMAX(0, item->GetValue(2), 100);
				int iLevelLimit = item->GetValue(3);
				uint32_t dwSkillVnum = 0;

				switch (item->GetVnum())
				{
				case 50314: case 50315: case 50316:
					dwSkillVnum = SKILL_POLYMORPH;
					break;

				case 50323: case 50324:
					dwSkillVnum = SKILL_ADD_HP;
					break;

				case 50325: case 50326:
					dwSkillVnum = SKILL_RESIST_PENETRATE;
					break;

				default:
					return false;
				}

				if (0 == dwSkillVnum)
					return false;

				if (GetLevel() < iLevelLimit)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 431, "%d", iLevelLimit);
#endif
					return false;
				}

				if (GetSkillLevel(dwSkillVnum) >= 40)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (GetSkillLevel(dwSkillVnum) < iSkillLevelLowLimit)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 429, "");
#endif
					return false;
				}

				if (GetSkillLevel(dwSkillVnum) >= iSkillLevelHighLimit)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;

			case 50902:
			case 50903:
			case 50904:
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = SKILL_CREATE;
				int iPct = MINMAX(0, item->GetValue(1), 100);

				if (GetSkillLevel(dwSkillVnum) >= 40)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;
			// MINING
			case ITEM_MINING_SKILL_TRAIN_BOOK:
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = SKILL_MINING;
				int iPct = MINMAX(0, item->GetValue(1), 100);

				if (GetSkillLevel(dwSkillVnum) >= 40)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (LearnSkillByBook(dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;
			// END_OF_MINING

			case ITEM_HORSE_SKILL_TRAIN_BOOK:
			{
				if (IsPolymorphed())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = SKILL_HORSE;
				int iPct = MINMAX(0, item->GetValue(1), 100);

				if (GetLevel() < 50)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 404, "%d", 50);
#endif
					return false;
				}

				if (!test_server && get_global_time() < GetSkillNextReadTime(dwSkillVnum))
				{
					if (FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
					{
						// ÁÖ¾È¼ú¼­ »ç¿ëÁß¿¡´Â ½Ã°£ Á¦ÇÑ ¹«½Ã
						RemoveAffect(AFFECT_SKILL_NO_BOOK_DELAY);
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 465, "");
#endif
					}
					else
					{
						SkillLearnWaitMoreTimeMessage(GetSkillNextReadTime(dwSkillVnum) - get_global_time());
						return false;
					}
				}

				if (GetPoint(POINT_HORSE_SKILL) >= 20 ||
					GetSkillLevel(SKILL_HORSE_WILDATTACK) + GetSkillLevel(SKILL_HORSE_CHARGE) + GetSkillLevel(SKILL_HORSE_ESCAPE) >= 60 ||
					GetSkillLevel(SKILL_HORSE_WILDATTACK_RANGE) + GetSkillLevel(SKILL_HORSE_CHARGE) + GetSkillLevel(SKILL_HORSE_ESCAPE) >= 60)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 307, "");
#endif
					return false;
				}

				if (number(1, 100) <= iPct)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 394, "");
#endif
					PointChange(POINT_HORSE_SKILL, 1);

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					if (!test_server)
						SetSkillNextReadTime(dwSkillVnum, get_global_time() + iReadDelay);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 393, "");
				}
#endif
#ifdef ENABLE_BOOKS_STACKFIX
				item->SetCount(item->GetCount() - 1);
#else
				ITEM_MANAGER::instance().RemoveItem(item);
#endif
			}
			break;



			case 70102: // Zenbab
			{

				uint32_t max_limit = 2500000;
				uint32_t current = GetAlignment();

				if (current >= max_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Max  250.000 with this item!");
					return false;
				}

				uint32_t add_value = item->GetValue(0); 
				uint32_t remaining = max_limit - current;

				uint32_t real_add = std::min(add_value, remaining);

				UpdateAlignment(real_add);
				item->SetCount(item->GetCount() - 1);

				ChatPacket(CHAT_TYPE_INFO, " +500 point.");
			}
			break;


			case 70100:
			{
				const uint32_t max_limit = 25000000;
				const uint32_t current = GetAlignment();

				if (current >= max_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Max 2.500.000 with this item!");
					return false;
				}

				const uint32_t add_value = item->GetValue(0);
				const uint32_t real_add = std::min(add_value, max_limit - current);

				UpdateAlignment(real_add);
				item->SetCount(item->GetCount() - 1);

				ChatPacket(CHAT_TYPE_INFO, "+5.000 point.");
			}
			break;

			// --------------------------------------------------------------
			// 80008: ShopBuyPrice alapjan Dragon Coin (account.coins) jovairas
			// --------------------------------------------------------------
			case 39065://sé 1
			case 89003://sé 3
			case 89004://sé 50
			case 89005://sé 100
			case 89006://sé 500
			case 89007://sé 1000

			{
#ifdef ENABLE_ITEMSHOP
				const uint32_t count = item->GetCount();
				if (count == 0)
					break;

				const uint64_t unitPrice = (uint64_t)item->GetShopBuyPrice();
				if (unitPrice == 0)
					break;

				const uint64_t total = unitPrice * (uint64_t)count;

				if (GetDesc() == nullptr)
					break;

				const uint32_t curCoins = GetDragonCoin();
				const uint64_t maxCoins = 0xFFFFFFFFULL; // uint32 max

				if ((uint64_t)curCoins >= maxCoins)
					break;

				const uint64_t canAdd = ((uint64_t)curCoins + total > maxCoins) ? (maxCoins - (uint64_t)curCoins) : total;
				if (canAdd == 0)
					break;

				SetDragonCoin(curCoins + (uint32_t)canAdd);

				item->SetCount(0); // teljes stack felhasznalasa
				ChatPacket(CHAT_TYPE_INFO, "Kaptal %u Sarkanyermet.", (uint32_t)canAdd);
#else
				ChatPacket(CHAT_TYPE_INFO, "ItemShop ki van kapcsolva.");
#endif
			}
			break;

			// --------------------------------------------------------------
			// Gyümölcs – +2000 RP (1 óránként használható)
			// --------------------------------------------------------------
			case 71107:
			case 39032:
			{
				const uint32_t max_limit = 25000000;
				const uint32_t current = GetAlignment();

				if (current >= max_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Max 2.500.000 with this item!");
					return false;
				}

				const uint32_t add_value = item->GetValue(0);
				const uint32_t real_add = std::min(add_value, max_limit - current);

				UpdateAlignment(real_add);
				item->SetCount(item->GetCount() - 1);

				ChatPacket(CHAT_TYPE_INFO, "+2000 point");
			}
			break;

			case 72101:
			{
				const uint32_t min_limit = 25000000; // 2.500.000 lathato rang
				const uint32_t max_limit = 50000000; // 5.000.000 lathato rang
				const uint32_t current = GetAlignment();

				if (current < min_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Min point: 2.500.000 ");
					return false;
				}

				if (current >= max_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Max 5.000.000!");
					return false;
				}

				// 10.000  rang = 100.000 belso alignment
				const uint32_t add_value = 300000;
				const uint32_t real_add = std::min(add_value, max_limit - current);

				UpdateAlignment(real_add);
				item->SetCount(item->GetCount() - 1);

				ChatPacket(CHAT_TYPE_INFO, "+30.000 point addaed.");
			}
			break;
			// --------------------------------------------------------------
			// Arany Gyümölcs – +10000 RP (1 óránként használható)
			// --------------------------------------------------------------
			case 72100:
			{
				const uint32_t max_limit = 25000000;
				const uint32_t current = GetAlignment();

				if (current >= max_limit)
				{
					ChatPacket(CHAT_TYPE_INFO, "Max 2.500.000!");
					return false;
				}

				const uint32_t add_value = item->GetValue(0);
				const uint32_t real_add = std::min(add_value, max_limit - current);

				UpdateAlignment(real_add);
				item->SetCount(item->GetCount() - 1);

				ChatPacket(CHAT_TYPE_INFO, "+10.000.");
			}
			break;
			break;
			case 39069:
			case 80003:
			case 80004:
			case 80005:
			case 80006:
			case 80007:
			case 80008:
			{
				const uint32_t count = item->GetCount();
				if (count == 0)
					break;

				const uint64_t unitPrice = (uint64_t)item->GetShopBuyPrice();
				if (unitPrice == 0)
					break;

				const uint64_t curGold = (uint64_t)GetGold();
				const uint64_t maxGold = (uint64_t)GOLD_MAX;

				if (curGold >= maxGold)
					break;

				const uint64_t freeSpace = maxGold - curGold;
				if (freeSpace < unitPrice)
					break;

				 
				uint32_t canUse = (uint32_t)(freeSpace / unitPrice);
				if (canUse > count)
					canUse = count;

				if (canUse == 0)
					break;

				const uint64_t canAdd = unitPrice * (uint64_t)canUse;

				 
				GiveGold((long long)canAdd);

				 
				item->SetCount(count - canUse);
			}
			break;


			//case 71107: // Ãµµµº¹¼þ¾Æ
//			{
//				uint32_t val = item->GetValue(0);
//				int interval = item->GetValue(1);
//				quest::PC* pPC = quest::CQuestManager::instance().GetPC(GetPlayerID());
//				int last_use_time = pPC->GetFlag("mythical_peach.last_use_time");
//
//				if (get_global_time() - last_use_time < interval * 60 * 60)
//				{
//#ifdef TEXTS_IMPROVEMENT
//					ChatPacketNew(CHAT_TYPE_INFO, 508, "");
//#endif
//					return false;
//				}
//
//				if (GetAlignment() == 25000000)
//				{
//#ifdef TEXTS_IMPROVEMENT
//					ChatPacketNew(CHAT_TYPE_INFO, 674, "%d", 25000000);
//#endif
//					return false;
//				}
//
//				if (25000000 - GetAlignment() < val * 10)
//				{
//					val = (25000000 - GetAlignment()) / 10;
//				}
//
//				uint32_t old_alignment = GetAlignment() / 10;
//
//				UpdateAlignment(val * 10);
//
//				item->SetCount(item->GetCount() - 1);
//				pPC->SetFlag("mythical_peach.last_use_time", get_global_time());
//
//#ifdef TEXTS_IMPROVEMENT
//				ChatPacketNew(CHAT_TYPE_INFO, 327, "%d", val);
//#endif
//
//				char buf[256 + 1];
//				snprintf(buf, sizeof(buf), "%u %u", old_alignment, GetAlignment() / 10);
//				LogManager::instance().CharLog(this, val, "MYTHICAL_PEACH", buf);
//			}
//			break;

			case 71109: // Å»¼®¼­
			case 72719:
			{
				LPITEM item2;

				if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				if (item2->GetSocketCount() == 0)
					return false;

#ifdef ENABLE_BUG_FIXES
				if (item2->IsEquipped())
					return false;
#endif

				switch (item2->GetType())
				{
				case ITEM_WEAPON:
					break;
				case ITEM_ARMOR:
					switch (item2->GetSubType())
					{
					case ARMOR_EAR:
					case ARMOR_WRIST:
					case ARMOR_NECK:
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 675, "%s", item->GetName());
#endif
						return false;
					}
					break;

				default:
					return false;
				}

				std::stack<int32_t> socket;

				for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
					socket.push(item2->GetSocket(i));

				int idx = ITEM_SOCKET_MAX_NUM - 1;

				while (socket.size() > 0)
				{
					if (socket.top() > 2 && socket.top() != ITEM_BROKEN_METIN_VNUM)
						break;

					idx--;
					socket.pop();
				}

				if (socket.size() == 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 675, "%s", item2->GetName());
#endif
					return false;
				}

				LPITEM pItemReward = AutoGiveItem(socket.top());

				if (pItemReward != nullptr)
				{
					item2->SetSocket(idx, 1);

					char buf[256 + 1];
					snprintf(buf, sizeof(buf), "%s(%u) %s(%u)",
						item2->GetName(), item2->GetID(), pItemReward->GetName(), pItemReward->GetID());
					LogManager::instance().ItemLog(this, item, "USE_DETACHMENT_ONE", buf);

					item->SetCount(item->GetCount() - 1);
				}
			}
			break;

			case 70201:   // Å»»öÁ¦
			case 70202:   // ¿°»ö¾à(Èò»ö)
			case 70203:   // ¿°»ö¾à(±Ý»ö)
			case 70204:   // ¿°»ö¾à(»¡°£»ö)
			case 70205:   // ¿°»ö¾à(°¥»ö)
			case 70206:   // ¿°»ö¾à(°ËÀº»ö)
			{
				if (GetPart(PART_HAIR) < 1001)
				{
					quest::CQuestManager& q = quest::CQuestManager::instance();
					quest::PC* pPC = q.GetPC(GetPlayerID());

					if (pPC)
					{
						int last_dye_level = pPC->GetFlag("dyeing_hair.last_dye_level");

						if (last_dye_level == 0 ||
							last_dye_level + 3 <= GetLevel() ||
							item->GetVnum() == 70201)
						{
							SetPart(PART_HAIR, item->GetVnum() - 70201);

							if (item->GetVnum() == 70201)
								pPC->SetFlag("dyeing_hair.last_dye_level", 0);
							else
								pPC->SetFlag("dyeing_hair.last_dye_level", GetLevel());

							item->SetCount(item->GetCount() - 1);
							UpdatePacket();
						}
#ifdef TEXTS_IMPROVEMENT
						else {
							ChatPacketNew(CHAT_TYPE_INFO, 97, "%d", last_dye_level + 3);
						}
#endif
					}
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 491, "");
				}
#endif
			}
			break;

			case ITEM_NEW_YEAR_GREETING_VNUM:
			{
				uint32_t dwBoxVnum = ITEM_NEW_YEAR_GREETING_VNUM;
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector <LPITEM> item_gets;
				int count = 0;

				if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
				{
#ifdef TEXTS_IMPROVEMENT
					for (int i = 0; i < count; i++) {
						if (dwVnums[i] == CSpecialItemGroup::GOLD) {
							ChatPacketNew(CHAT_TYPE_INFO, 102, "%d", dwCounts[i]);
						}
					}
#endif
					item->SetCount(item->GetCount() - 1);
				}
			}
			break;

			case ITEM_VALENTINE_ROSE:
			case ITEM_VALENTINE_CHOCOLATE:
			{
				uint32_t dwBoxVnum = item->GetVnum();
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector <LPITEM> item_gets(0);
				int count = 0;

				if (item->GetVnum() == ITEM_VALENTINE_ROSE && SEX_MALE == GET_SEX(this)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 383, "");
#endif
					return false;
				}
				else if (item->GetVnum() == ITEM_VALENTINE_CHOCOLATE && SEX_FEMALE == GET_SEX(this)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 382, "");
#endif
					return false;
				}

				if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
					item->SetCount(item->GetCount() - 1);
			}
			break;

			case ITEM_WHITEDAY_CANDY:
			case ITEM_WHITEDAY_ROSE:
			{
				uint32_t dwBoxVnum = item->GetVnum();
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector <LPITEM> item_gets(0);
				int count = 0;

				if (item->GetVnum() == ITEM_WHITEDAY_ROSE && SEX_MALE == GET_SEX(this)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 383, "");
#endif
					return false;
				}
				else if (item->GetVnum() == ITEM_WHITEDAY_CANDY && SEX_FEMALE == GET_SEX(this)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 382, "");
#endif
					return false;
				}

				if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
					item->SetCount(item->GetCount() - 1);
			}
			break;

			case 50011: // ¿ù±¤º¸ÇÕ
			{
				uint32_t dwBoxVnum = 50011;
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector <LPITEM> item_gets(0);
				int count = 0;

				if (GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
				{
					for (int i = 0; i < count; i++)
					{
						char buf[50 + 1];
						snprintf(buf, sizeof(buf), "%u %u", dwVnums[i], dwCounts[i]);
						LogManager::instance().ItemLog(this, item, "MOONLIGHT_GET", buf);

						//ITEM_MANAGER::instance().RemoveItem(item);
						item->SetCount(item->GetCount() - 1);

						switch (dwVnums[i])
						{
						case CSpecialItemGroup::GOLD:
							break;
						case CSpecialItemGroup::EXP:
							break;

						case CSpecialItemGroup::MOB:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 378, "");
#endif
							break;

						case CSpecialItemGroup::SLOW:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 377, "");
#endif
							break;

						case CSpecialItemGroup::DRAIN_HP:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 373, "");
#endif
							break;

						case CSpecialItemGroup::POISON:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 376, "");
#endif
							break;
#ifdef ENABLE_WOLFMAN_CHARACTER
						case CSpecialItemGroup::BLEEDING:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 379, "");
#endif
							break;
#endif
						case CSpecialItemGroup::MOB_GROUP:
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 380, "");
#endif
							break;

						default:
							//#ifdef TEXTS_IMPROVEMENT
							//												if (item_gets[i]) {
							//													if (dwCounts[i] > 1) {
							//														ChatPacketNew(CHAT_TYPE_INFO, 374, "%d#%s", dwCounts[i], item_gets[i]->GetName());
							//													} else {
							//														ChatPacketNew(CHAT_TYPE_INFO, 375, "%s", item_gets[i]->GetName());
							//													}
							//												}
							//#endif
							break;
						}
					}
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 395, "");
#endif
					return false;
				}
			}
			break;

			case ITEM_GIVE_STAT_RESET_COUNT_VNUM:
			{
				//PointChange(POINT_GOLD, -iCost);
				PointChange(POINT_STAT_RESET_COUNT, 1);
				item->SetCount(item->GetCount() - 1);
			}
			break;

			case 50107:
			{
				if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				EffectPacket(SE_CHINA_FIREWORK);
#ifdef ENABLE_FIREWORK_STUN
				// ½ºÅÏ °ø°ÝÀ» ¿Ã·ÁÁØ´Ù
				AddAffect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5 * 60, 0, true);
#endif
				item->SetCount(item->GetCount() - 1);
			}
			break;

			case 50108:
			{
				if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				EffectPacket(SE_SPIN_TOP);
#ifdef ENABLE_FIREWORK_STUN
				// ½ºÅÏ °ø°ÝÀ» ¿Ã·ÁÁØ´Ù
				AddAffect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5 * 60, 0, true);
#endif
				item->SetCount(item->GetCount() - 1);
			}
			break;

			case ITEM_WONSO_BEAN_VNUM:
				PointChange(POINT_HP, GetMaxHP() - GetHP());
				item->SetCount(item->GetCount() - 1);
				break;

			case ITEM_WONSO_SUGAR_VNUM:
				PointChange(POINT_SP, GetMaxSP() - GetSP());
				item->SetCount(item->GetCount() - 1);
				break;

			case ITEM_WONSO_FRUIT_VNUM:
				PointChange(POINT_STAMINA, GetMaxStamina() - GetStamina());
				item->SetCount(item->GetCount() - 1);
				break;

			case 90008: // VCARD
			case 90009: // VCARD
				VCardUse(this, this, item);
				break;

			case ITEM_ELK_VNUM: // µ·²Ù·¯¹Ì
			{
				int iGold = item->GetSocket(0);
				ITEM_MANAGER::instance().RemoveItem(item);
				PointChange(POINT_GOLD, iGold);
			}
			break;
			case 27995:
			{
			}
			break;

			case 71092: // º¯½Å ÇØÃ¼ºÎ ÀÓ½Ã
			{
				if (m_pkChrTarget != nullptr)
				{
					if (m_pkChrTarget->IsPolymorphed())
					{
						m_pkChrTarget->SetPolymorph(0);
						m_pkChrTarget->RemoveAffect(AFFECT_POLYMORPH);
					}
				}
				else
				{
					if (IsPolymorphed())
					{
						SetPolymorph(0);
						RemoveAffect(AFFECT_POLYMORPH);
					}
				}
			}
			break;

			case 30617: // ÁøÀç°¡
			{
				// À¯·´, ½Ì°¡Æú, º£Æ®³² ÁøÀç°¡ »ç¿ë±ÝÁö
				LPITEM item2;

				if (!IsValidItemPosition(DestCell) || !(item2 = GetInventoryItem(wDestCell)))
					return false;

				if (ITEM_COSTUME == item2->GetType())
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->AddRareAttribute() == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 389, "");
#endif
					int iAddedIdx = item2->GetRareAttrCount() + 4;
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());

					LogManager::instance().ItemLog(
						GetPlayerID(),
						item2->GetAttributeType(iAddedIdx),
						item2->GetAttributeValue(iAddedIdx),
						item->GetID(),
						"ADD_RARE_ATTR",
						buf,
						GetDesc()->GetHostName(),
						item->GetOriginalVnum());

					item->SetCount(item->GetCount() - 1);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 308, "");
				}
#endif
			}
			break;

			case 30618: // ÁøÀç°æ
			{
				// À¯·´, ½Ì°¡Æú, º£Æ®³² ÁøÀç°¡ »ç¿ë±ÝÁö
				LPITEM item2;

				if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (ITEM_COSTUME == item2->GetType()) // @fixme124
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->ChangeRareAttribute() == true)
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "CHANGE_RARE_ATTR", buf);

					item->SetCount(item->GetCount() - 1);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 354, "");
				}
#endif
			}
			break;
#ifdef ENABLE_CHANGE_NORMAL_HIT_RAZOR93
			case 70251: // ÁøÀç°æ
			{
				// À¯·´, ½Ì°¡Æú, º£Æ®³² ÁøÀç°¡ »ç¿ë±ÝÁö
				LPITEM item2;

				if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (ITEM_COSTUME == item2->GetType()) // @fixme124
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->ChangeKKAK() == true)
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "CHANGE_RARE_ATTR21", buf);

					item->SetCount(item->GetCount() - 1);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 354, "");
				}
#endif
			}
			break;

#endif

			case ITEM_AUTO_HP_RECOVERY_S:
			case ITEM_AUTO_HP_RECOVERY_M:
			case ITEM_AUTO_HP_RECOVERY_L:
			case ITEM_AUTO_HP_RECOVERY_X:
			case ITEM_AUTO_SP_RECOVERY_S:
			case ITEM_AUTO_SP_RECOVERY_M:
			case ITEM_AUTO_SP_RECOVERY_L:
			case ITEM_AUTO_SP_RECOVERY_X:
				// ¹«½Ã¹«½ÃÇÏÁö¸¸ ÀÌÀü¿¡ ÇÏ´ø °É °íÄ¡±â´Â ¹«¼·°í...
				// ±×·¡¼­ ±×³É ÇÏµå ÄÚµù. ¼±¹° »óÀÚ¿ë ÀÚµ¿¹°¾à ¾ÆÀÌÅÛµé.
			case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
			case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
			case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
			case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
			{
				if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				EAffectTypes type = AFFECT_NONE;
				bool isSpecialPotion = false;

				switch (item->GetVnum())
				{
				case ITEM_AUTO_HP_RECOVERY_X:
					isSpecialPotion = true;

				case ITEM_AUTO_HP_RECOVERY_S:
				case ITEM_AUTO_HP_RECOVERY_M:
				case ITEM_AUTO_HP_RECOVERY_L:
				case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
				case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
					type = AFFECT_AUTO_HP_RECOVERY;
					break;

				case ITEM_AUTO_SP_RECOVERY_X:
					isSpecialPotion = true;

				case ITEM_AUTO_SP_RECOVERY_S:
				case ITEM_AUTO_SP_RECOVERY_M:
				case ITEM_AUTO_SP_RECOVERY_L:
				case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
				case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
					type = AFFECT_AUTO_SP_RECOVERY;
					break;
				}

				if (AFFECT_NONE == type)
					break;

				if (item->GetCount() > 1)
				{
#ifdef ENABLE_EXTRA_INVENTORY
					const bool bFromExtraInventory = (item->GetWindow() == EXTRA_INVENTORY);
#else
					const bool bFromExtraInventory = false;
#endif
					int pos = -1;

#ifdef ENABLE_EXTRA_INVENTORY
					if (bFromExtraInventory)
						pos = GetEmptyExtraInventory(item);
					else
#endif
						pos = GetEmptyInventory(item->GetSize());

					if (-1 == pos)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
						break;
					}

					item->SetCount(item->GetCount() - 1);

					LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), 1);

#ifdef ENABLE_EXTRA_INVENTORY
					if (bFromExtraInventory)
						item2->AddToCharacter(this, TItemPos(EXTRA_INVENTORY, pos));
					else
#endif
						item2->AddToCharacter(this, TItemPos(INVENTORY, pos));

					if (item->GetSocket(1) != 0)
					{
						item2->SetSocket(1, item->GetSocket(1));
					}

					if (FindAffect(type))
						return true;
					else if (isSpecialPotion) {
						EAffectTypes eType = type == AFFECT_AUTO_HP_RECOVERY ? AFFECT_AUTO_HP_RECOVERY2 : AFFECT_AUTO_SP_RECOVERY2;
						if (FindAffect(eType))
							return true;
					}

					item = item2;
				}

#ifdef ENABLE_NEW_USE_POTION
				EAffectTypes type2 = AFFECT_NONE;
				CAffect* pAffect2 = nullptr;
#endif
				CAffect* pAffect = FindAffect(type);

				if (nullptr == pAffect)
				{
					EPointTypes bonus = POINT_NONE;
					if (true == isSpecialPotion)
					{
						if (type == AFFECT_AUTO_HP_RECOVERY)
						{
#ifdef ENABLE_NEW_USE_POTION
							type2 = type;
							type = AFFECT_AUTO_HP_RECOVERY2;
#endif
							bonus = POINT_MAX_HP_PCT;
						}
						else if (type == AFFECT_AUTO_SP_RECOVERY)
						{
#ifdef ENABLE_NEW_USE_POTION
							type2 = type;
							type = AFFECT_AUTO_SP_RECOVERY2;
#endif
							bonus = POINT_MAX_SP_PCT;
						}
					}
#ifdef ENABLE_NEW_USE_POTION
					else {
						if (type == AFFECT_AUTO_HP_RECOVERY)
							type2 = AFFECT_AUTO_HP_RECOVERY2;
						else if (type == AFFECT_AUTO_SP_RECOVERY)
							type2 = AFFECT_AUTO_SP_RECOVERY2;
					}

					pAffect2 = FindAffect(type2);
					if (pAffect2) {
						if (item->GetID() == pAffect2->dwFlag)
						{
							RemoveAffect(pAffect2);
							item->Lock(false);
							item->SetSocket(0, false);
						}
						else
						{
							LPITEM old = FindItemByID(pAffect2->dwFlag);
							if (nullptr != old)
							{
								old->Lock(false);
								old->SetSocket(0, false);
							}

							RemoveAffect(pAffect2);
						}
					}
					else if (isSpecialPotion == true) {
						pAffect2 = FindAffect(type);
						if (pAffect2) {
							if (item->GetID() == pAffect2->dwFlag)
							{
								RemoveAffect(pAffect2);
								item->Lock(false);
								item->SetSocket(0, false);
								return true;
							}
							else {
								LPITEM old = FindItemByID(pAffect2->dwFlag);
								if (old)
								{
									old->Lock(false);
									old->SetSocket(0, false);
								}
							}
						}
					}
#endif

					AddAffect(type, bonus, 4, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);
					item->Lock(true);
					item->SetSocket(0, true);
					AutoRecoveryItemProcess(type);
				}
				else
				{
					if (item->GetID() == pAffect->dwFlag)
					{
						RemoveAffect(pAffect);

						item->Lock(false);
						item->SetSocket(0, false);
					}
					else
					{
						LPITEM old = FindItemByID(pAffect->dwFlag);

						if (nullptr != old)
						{
							old->Lock(false);
							old->SetSocket(0, false);
						}

						RemoveAffect(pAffect);

						EPointTypes bonus = POINT_NONE;

						if (true == isSpecialPotion)
						{
							if (type == AFFECT_AUTO_HP_RECOVERY)
							{
#ifdef ENABLE_NEW_USE_POTION
								type2 = type;
								type = AFFECT_AUTO_HP_RECOVERY2;
#endif
								bonus = POINT_MAX_HP_PCT;
							}
							else if (type == AFFECT_AUTO_SP_RECOVERY)
							{
#ifdef ENABLE_NEW_USE_POTION
								type2 = type;
								type = AFFECT_AUTO_SP_RECOVERY2;
#endif
								bonus = POINT_MAX_SP_PCT;
							}
						}
#ifdef ENABLE_NEW_USE_POTION
						else {
							if (type == AFFECT_AUTO_HP_RECOVERY)
								type2 = AFFECT_AUTO_HP_RECOVERY2;
							else if (type == AFFECT_AUTO_SP_RECOVERY)
								type2 = AFFECT_AUTO_SP_RECOVERY2;
						}

						pAffect2 = FindAffect(type2);
						if (pAffect2) {
							if (item->GetID() == pAffect2->dwFlag)
							{
								RemoveAffect(pAffect2);
								item->Lock(false);
								item->SetSocket(0, false);
							}
							else
							{
								LPITEM old = FindItemByID(pAffect2->dwFlag);
								if (nullptr != old)
								{
									old->Lock(false);
									old->SetSocket(0, false);
								}

								RemoveAffect(pAffect2);
							}
						}
#endif

						AddAffect(type, bonus, 4, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);

						item->Lock(true);
						item->SetSocket(0, true);

						AutoRecoveryItemProcess(type);
					}
				}
			}
			break;
			}
			break;

		case USE_CLEAR:
		{
			switch (item->GetVnum())
			{
#ifdef ENABLE_WOLFMAN_CHARACTER
			case 27124: // Bandage
				RemoveBleeding();
				break;
#endif
			case 27874: // Grilled Perch
			default:
				RemoveBadAffect();
				break;
			}
			item->SetCount(item->GetCount() - 1);
		}
		break;

		case USE_INVISIBILITY:
		{
			if (item->GetVnum() == 70026)
			{
				quest::CQuestManager& q = quest::CQuestManager::instance();
				quest::PC* pPC = q.GetPC(GetPlayerID());

				if (pPC != nullptr)
				{
					int last_use_time = pPC->GetFlag("mirror_of_disapper.last_use_time");

					if (get_global_time() - last_use_time < 10 * 60)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 508, "");
#endif
						return false;
					}

					pPC->SetFlag("mirror_of_disapper.last_use_time", get_global_time());
				}
			}

			AddAffect(AFFECT_INVISIBILITY, POINT_NONE, 0, AFF_INVISIBILITY, 300, 0, true);
			item->SetCount(item->GetCount() - 1);
		}
		break;

		case USE_POTION_NODELAY:
		{
			if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
			{
				if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 303, "");
#endif
					return false;
				}

				switch (item->GetVnum())
				{
				case 70020:
				case 71018:
				case 71019:
				case 71020:
					if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
					{
						if (m_nPotionLimit <= 0)
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 362, "");
#endif
							return false;
						}
					}
					break;

				default:
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 303, "");
#endif
					return false;
				}
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif

			bool used = false;

			if (item->GetValue(0) != 0) // HP Àý´ë°ª È¸º¹
			{
				if (GetHP() < GetMaxHP())
				{
					PointChange(POINT_HP, item->GetValue(0) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
					EffectPacket(SE_HPUP_RED);
					used = true;
				}
			}

			if (item->GetValue(1) != 0)	// SP Àý´ë°ª È¸º¹
			{
				if (GetSP() < GetMaxSP())
				{
					PointChange(POINT_SP, item->GetValue(1) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
					EffectPacket(SE_SPUP_BLUE);
					used = true;
				}
			}

			if (item->GetValue(3) != 0) // HP % È¸º¹
			{
				if (GetHP() < GetMaxHP())
				{
					PointChange(POINT_HP, item->GetValue(3) * GetMaxHP() / 100);
					EffectPacket(SE_HPUP_RED);
					used = true;
				}
			}

			if (item->GetValue(4) != 0) // SP % È¸º¹
			{
				if (GetSP() < GetMaxSP())
				{
					PointChange(POINT_SP, item->GetValue(4) * GetMaxSP() / 100);
					EffectPacket(SE_SPUP_BLUE);
					used = true;
				}
			}

			if (used)
			{
				if (item->GetVnum() == 50085 || item->GetVnum() == 50086) {
					SetUseSeedOrMoonBottleTime();
				}

				if (GetWarMap())
					GetWarMap()->UsePotion(this, item);

				m_nPotionLimit--;

				//RESTRICT_USE_SEED_OR_MOONBOTTLE
				item->SetCount(item->GetCount() - 1);
				//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
			}
		}
		break;

		case USE_POTION:
			if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
			{
				if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 303, "");
#endif
					return false;
				}
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif

			if (item->GetValue(1) != 0)
			{
				if (GetPoint(POINT_SP_RECOVERY) + GetSP() >= GetMaxSP())
				{
					return false;
				}

				PointChange(POINT_SP_RECOVERY, item->GetValue(1) * std::min((int64_t)200, (100 + GetPoint(POINT_POTION_BONUS))) / 100);
				StartAffectEvent();
				EffectPacket(SE_SPUP_BLUE);
			}

			if (item->GetValue(0) != 0)
			{
				if (GetPoint(POINT_HP_RECOVERY) + GetHP() >= GetMaxHP())
				{
					return false;
				}

				PointChange(POINT_HP_RECOVERY, item->GetValue(0) * std::min((int64_t)200, (100 + GetPoint(POINT_POTION_BONUS))) / 100);
				StartAffectEvent();
				EffectPacket(SE_HPUP_RED);
			}

			if (GetWarMap())
				GetWarMap()->UsePotion(this, item);

			item->SetCount(item->GetCount() - 1);
			m_nPotionLimit--;
			break;

		case USE_POTION_CONTINUE:
		{
			if (item->GetValue(0) != 0)
			{
				AddAffect(AFFECT_HP_RECOVER_CONTINUE, POINT_HP_RECOVER_CONTINUE, item->GetValue(0), 0, item->GetValue(2), 0, true);
			}
			else if (item->GetValue(1) != 0)
			{
				AddAffect(AFFECT_SP_RECOVER_CONTINUE, POINT_SP_RECOVER_CONTINUE, item->GetValue(1), 0, item->GetValue(2), 0, true);
			}
			else
				return false;
		}

		if (GetWarMap())
			GetWarMap()->UsePotion(this, item);

		item->SetCount(item->GetCount() - 1);
		break;

		case USE_ABILITY_UP:
		{
			switch (item->GetValue(0))
			{
			case APPLY_MOV_SPEED:
				AddAffect(AFFECT_MOV_SPEED, POINT_MOV_SPEED, item->GetValue(2), AFF_MOV_SPEED_POTION, item->GetValue(1), 0, true);
#ifdef ENABLE_EFFECT_EXTRAPOT
				EffectPacket(SE_DXUP_PURPLE);
#endif
				break;

			case APPLY_ATT_SPEED:
				AddAffect(AFFECT_ATT_SPEED, POINT_ATT_SPEED, item->GetValue(2), AFF_ATT_SPEED_POTION, item->GetValue(1), 0, true);
#ifdef ENABLE_EFFECT_EXTRAPOT
				EffectPacket(SE_SPEEDUP_GREEN);
#endif
				break;

			case APPLY_STR:
				AddAffect(AFFECT_STR, POINT_ST, item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_DEX:
				AddAffect(AFFECT_DEX, POINT_DX, item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_CON:
				AddAffect(AFFECT_CON, POINT_HT, item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_INT:
				AddAffect(AFFECT_INT, POINT_IQ, item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_CAST_SPEED:
				AddAffect(AFFECT_CAST_SPEED, POINT_CASTING_SPEED, item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_ATT_GRADE_BONUS:
				AddAffect(AFFECT_ATT_GRADE, POINT_ATT_GRADE_BONUS,
					item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;

			case APPLY_DEF_GRADE_BONUS:
				AddAffect(AFFECT_DEF_GRADE, POINT_DEF_GRADE_BONUS,
					item->GetValue(2), 0, item->GetValue(1), 0, true);
				break;
			}
		}

		if (GetWarMap())
			GetWarMap()->UsePotion(this, item);

		item->SetCount(item->GetCount() - 1);
		break;

		case USE_TALISMAN:
		{
			const int TOWN_PORTAL = 1;
			const int MEMORY_PORTAL = 2;


			// gm_guild_build, oxevent ¸Ê¿¡¼­ ±ÍÈ¯ºÎ ±ÍÈ¯±â¾ïºÎ ¸¦ »ç¿ë¸øÇÏ°Ô ¸·À½
			if (GetMapIndex() == 200 || GetMapIndex() == 113)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 489, "");
#endif
				return false;
			}

			if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif

			if (m_pkWarpEvent)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 434, "");
#endif
				return false;
			}

			// CONSUME_LIFE_WHEN_USE_WARP_ITEM
			int consumeLife = CalculateConsume(this);

			if (consumeLife < 0)
				return false;
			// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

			if (item->GetValue(0) == TOWN_PORTAL) // ±ÍÈ¯ºÎ
			{
				if (item->GetSocket(0) == 0)
				{
					if (!GetDungeon())
						if (!GiveRecallItem(item))
							return false;

					PIXEL_POSITION posWarp;

					if (ecs::GetRecallPosition(GetMapIndex(), GetEmpire(), posWarp))
					{
						// CONSUME_LIFE_WHEN_USE_WARP_ITEM
						PointChange(POINT_HP, -consumeLife, false);
						// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

						WarpSet(posWarp.x, posWarp.y);
					}
					else
					{
						sys_err("CHARACTER::UseItem : cannot find spawn position (name %s, %d x %d)", GetName(), GetX(), GetY());
					}
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					if (test_server) {
						ChatPacketNew(CHAT_TYPE_INFO, 415, "");
					}
#endif
					ProcessRecallItem(item);
				}
			}
			else if (item->GetValue(0) == MEMORY_PORTAL) // ±ÍÈ¯±â¾ïºÎ
			{
				if (item->GetSocket(0) == 0)
				{
					if (GetDungeon())
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 310, "%s", item->GetName());
#endif
						return false;
					}

					if (!GiveRecallItem(item))
						return false;
				}
				else
				{
					// CONSUME_LIFE_WHEN_USE_WARP_ITEM
					PointChange(POINT_HP, -consumeLife, false);
					// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

					ProcessRecallItem(item);
				}
			}
		}
		break;
#ifdef ENABLE_ATTR_COSTUMES
		case USE_CHANGE_ATTR_COSTUME:
		{
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if (item2->IsEquipped())
				BuffOnAttr_RemoveBuffsFromItem(item2);

			if (item2->GetType() != ITEM_COSTUME)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->GetSubType() != COSTUME_BODY) && (item2->GetSubType() != COSTUME_HAIR) && (item2->GetSubType() != COSTUME_WEAPON)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->IsExchanging()) || (item2->IsEquipped()))
				return false;

			if (item2->GetAttributeSetIndex() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}
			else if (item2->GetAttributeCount() == 0)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 354, "");
#endif
				return false;
			}

			item2->ChangeAttribute();

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "CHANGE_COSTUME_ATTR", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 392, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
		case USE_ADD_ATTR_COSTUME1:
		case USE_ADD_ATTR_COSTUME2:
		{
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if (item2->IsEquipped())
				BuffOnAttr_RemoveBuffsFromItem(item2);

			if (item2->GetType() != ITEM_COSTUME)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->GetSubType() != COSTUME_BODY) && (item2->GetSubType() != COSTUME_HAIR) && (item2->GetSubType() != COSTUME_WEAPON)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->IsExchanging()) || (item2->IsEquipped()))
				return false;

			if (item2->GetAttributeSetIndex() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 2) != 0) && (item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 1) != 0))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 87, "");
#endif
				return false;
			}

			uint8_t bAttrSocket = item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 2) == 0 ? ITEM_ATTRIBUTE_MAX_NUM - 2 : ITEM_ATTRIBUTE_MAX_NUM - 1;
			uint8_t bAttrSocketCheck = bAttrSocket == ITEM_ATTRIBUTE_MAX_NUM - 2 ? ITEM_ATTRIBUTE_MAX_NUM - 1 : ITEM_ATTRIBUTE_MAX_NUM - 2;
			if (item2->GetAttributeType(bAttrSocketCheck) == item->GetSocket(0))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 88, "");
#endif
				return false;
			}

			item2->SetForceAttribute(bAttrSocket, item->GetSocket(0), item->GetSocket(1));

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "ADD_COSTUME_ATTR", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 677, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
		case USE_REMOVE_ATTR_COSTUME:
		{
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if (item2->IsEquipped())
				BuffOnAttr_RemoveBuffsFromItem(item2);

			if (item2->GetType() != ITEM_COSTUME)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->GetSubType() != COSTUME_BODY) && (item2->GetSubType() != COSTUME_HAIR) && (item2->GetSubType() != COSTUME_WEAPON)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->IsExchanging()) || (item2->IsEquipped()))
				return false;

			if (item2->GetAttributeSetIndex() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if ((item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 2) == 0) && (item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 1) == 0))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 89, "");
#endif
				return false;
			}

			int iAttrSocket = GetAttrDialogRemove();
			if ((iAttrSocket == 0) && (item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 1) != 0))
			{
				item2->SetForceAttribute(ITEM_ATTRIBUTE_MAX_NUM - 2, item2->GetAttributeType(ITEM_ATTRIBUTE_MAX_NUM - 1), item2->GetAttributeValue(ITEM_ATTRIBUTE_MAX_NUM - 1));
				item2->SetForceAttribute(ITEM_ATTRIBUTE_MAX_NUM - 1, 0, 0);
			}
			else
				item2->SetForceAttribute(ITEM_ATTRIBUTE_MAX_NUM - 2 + iAttrSocket, 0, 0);

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "REMOVE_COSTUME_ATTR", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 90, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
#endif
#ifdef ENABLE_STOLE_COSTUME
		case USE_ENCHANT_STOLE: {
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if ((item2->GetType() != ITEM_COSTUME) || (item2->GetSubType() != COSTUME_STOLE)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 22, "%s", item->GetName());
#endif
				return false;
			}

			if ((item2->IsExchanging()) || (item2->IsEquipped()) || (item2->isLocked()))
				return false;

			uint8_t bGrade = item2->GetValue(0);
			if (bGrade < 1)
				return false;

			bGrade = bGrade > 4 ? 4 : bGrade;
			uint8_t bRandom = (bGrade * 4);
			for (int i = 0; i < MAX_ATTR; i++) {
				item2->SetForceAttribute(i, stoleInfoTable[i][0], stoleInfoTable[i][number(bRandom - 3, bRandom)]);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 21, "%s", item2->GetName());
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
#endif
#ifdef ENABLE_DS_ENCHANT
		case USE_DS_ENCHANT: {
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if (!item2->IsDragonSoul()) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 73, "");
#endif
				return false;
			}

			if ((DragonSoul_IsDeckActivated()) && (item2->IsEquipped())) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 76, "");
#endif
				return false;
			}

			if (item2->IsExchanging() /*|| item2->IsEquipped()*/) // ENABLE_BUG_FIXES
				return false;

			int iGrade = (item2->GetVnum() / 1000) % 10, iStep = (item2->GetVnum() / 100) % 10;
			if ((iGrade !=
#ifdef ENABLE_DS_GRADE_MYTH
				DRAGON_SOUL_GRADE_MYTH
#else
				DRAGON_SOUL_GRADE_LEGENDARY
#endif
				) || (iStep != DRAGON_SOUL_STEP_HIGHEST)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 75, "");
#endif
				return false;
			}

			for (int i = 0; i < ITEM_ATTRIBUTE_RARE_END; i++)
				item2->SetForceAttribute(i, 0, 0);

			bool bRet = DSManager::instance().PutAttributes(item2);
			if (!bRet)
				return false;

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "USE_DS_ENCHANT", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 74, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
#endif
#ifdef ENABLE_REMOTE_ATTR_SASH_REMOVE
		case USE_ATTR_SASH_REMOVE: {
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if ((item2->IsExchanging()) || (item2->IsEquipped()))
				return false;

			if ((item2->GetType() == ITEM_COSTUME) && (item2->GetSubType() == COSTUME_ACCE)) {
				if (item2->GetSocket(ACCE_ABSORBED_SOCKET) <= 0) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 71, "");
#endif
					return false;
				}

				bool bClean = CleanAcceAttr(item, item2);
				if (bClean) {
					{
						char buf[21];
						snprintf(buf, sizeof(buf), "%u", item2->GetID());
						LogManager::instance().ItemLog(this, item, "USE_ATTR_SASH_REMOVE", buf);
					}

#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 72, "");
#endif
				}

				return bClean;
			}
			else {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 70, "");
#endif
				return false;
			}
		}
#endif
#ifdef ENABLE_NEW_PET_EDITS
		case USE_PET_REVIVE: {
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if ((item2->GetVnum() < 55701) || (item2->GetVnum() > 55711)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 66, "");
#endif
				return false;
			}

			if (item2->GetSocket(0) != 0) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 67, "");
#endif
				return false;
			}

			if (item2->GetSocket(2) == 0) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 64, "");
#endif
				return false;
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // ENABLE_BUG_FIXES
				return false;

			if (item2->GetSocket(1) > int(1440 * 365)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 69, "");
#endif
				return false;
			}

			int iLimit = int(1440 * 365);
			int iValue = item->GetValue(0);
			int iNewDuration = iValue == 0 ? 1440 * 366 : 1440 * iValue;
			iNewDuration += item2->GetSocket(1);
			if ((iNewDuration >= iLimit) && (item->GetVnum() != 86074)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 68, "");
#endif
				return false;
			}

			iNewDuration = iNewDuration > iLimit ? iLimit : iNewDuration;
			if (item->GetVnum() == 86074)
				iNewDuration = 1440 * 366;

			item2->SetSocket(1, iNewDuration);
			item2->SetSocket(2, iNewDuration);
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE player.new_petsystem SET duration = %d, tduration = %d WHERE id = %lu ", iNewDuration, iNewDuration, item2->GetID()));

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "USE_PET_REVIVE", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 65, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}
		case USE_PET_ENCHANT: {
			LPITEM item2;
			if ((!IsValidItemPosition(DestCell)) || (!(item2 = GetItem(DestCell))))
				return false;

			if ((item2->GetVnum() < 55701) || (item2->GetVnum() > 55711)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 66, "");
#endif
				return false;
			}

			if (item2->GetSocket(0) != 0) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 67, "");
#endif
				return false;
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // ENABLE_BUG_FIXES
				return false;

			int idx = GetPetEnchant();
			if ((idx < 0) || (idx > 2))
				return false;

			int iValue = item2->GetAttributeValue(idx);
			if ((idx == 0) && (iValue >= 150)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 63, "");
#endif
				return false;
			}

			if ((idx == 1) && (iValue >= 100)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 63, "");
#endif
				return false;
			}

			if ((idx == 2) && (iValue >= 100)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 63, "");
#endif
				return false;
			}

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				LogManager::instance().ItemLog(this, item, "USE_PET_ENCHANT", buf);
			}

			if (number(1, 100) > 70) {
				int iMax;
				if (idx == 0)
					iMax = iValue + 5 > 150 ? 150 : iValue + 5;
				else
					iMax = iValue + 5 > 100 ? 100 : iValue + 5;

				item2->SetForceAttribute(idx, 1, iMax);
				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE player.new_petsystem SET bonus%d = %d WHERE id = %lu ", idx, iMax, item2->GetID()));

#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 61, "");
#endif
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ChatPacketNew(CHAT_TYPE_INFO, 62, "");
			}
#endif

			item->SetCount(item->GetCount() - 1);
			break;
		}
#endif
		case USE_TUNING:
		case USE_DETACHMENT:
		{
			LPITEM item2;

			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;

			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;

			if (item2->GetVnum() >= 28330 && item2->GetVnum() <= 28343) // ¿µ¼®+3
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 678, "%s", item->GetName());
#endif
				return false;
			}

#ifdef ENABLE_BUG_FIXES
			if (item2->IsEquipped())
				return false;
#endif

#ifdef ENABLE_ACCE_SYSTEM
			if (item->GetValue(0) == ACCE_CLEAN_ATTR_VALUE0)
			{
				if (!CleanAcceAttr(item, item2))
					return false;

				return true;
			}
#endif
			if (item2->GetVnum() >= 28430 && item2->GetVnum() <= 28443)  // ¿µ¼®+4
			{
				if (item->GetVnum() == 71056) // Ã»·æÀÇ¼û°á
				{
					RefineItem(item, item2);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 679, "%s", item->GetName());
				}
#endif
			}
			else
			{
				RefineItem(item, item2);
			}
		}
		break;
#ifdef ATTR_LOCK						
		case USE_ADD_ATTRIBUTE_LOCK:
		{
			LPITEM item2;
			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;

			if (ITEM_COSTUME == item2->GetType() || item2->GetWearFlag() == WEARABLE_PENDANT || item2->IsDragonSoul())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 791, "");
#endif
				return false;
			}

			if (item2->GetAttributeCount() < 5)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 792, "");
#endif
				return false;
			}

			if (item2->GetLockedAttr() != -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 793, "");
#endif
				return false;
			}

			if (item2->IsEquipped())
			{
				BuffOnAttr_RemoveBuffsFromItem(item2);
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;

			item2->AddLockedAttr();
			item->SetCount(item->GetCount() - 1);
		}
		break;
		case USE_CHANGE_ATTRIBUTE_LOCK:
		{
			LPITEM item2;
			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;


			if (item2->GetLockedAttr() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 795, "");
#endif
				return false;
			}

			if (ITEM_COSTUME == item2->GetType() /*|| item2->GetWearFlag() == WEARABLE_PENDANT*/ || item2->IsDragonSoul())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 791, "");
#endif
				return false;
			}

			if (item2->IsEquipped())
			{
				BuffOnAttr_RemoveBuffsFromItem(item2);
			}


			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;


			item2->ChangeLockedAttr();
			item->SetCount(item->GetCount() - 1);
		}
		break;
		case USE_DELETE_ATTRIBUTE_LOCK:
		{
			LPITEM item2;
			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;

			if (item2->GetLockedAttr() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 795, "");
#endif
				return false;
			}

			if (item2->IsEquipped())
			{
				BuffOnAttr_RemoveBuffsFromItem(item2);
			}

			if (ITEM_COSTUME == item2->GetType() || item2->IsDragonSoul())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 680, "");
#endif
				return false;
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;

			item2->RemoveLockedAttr();
			item->SetCount(item->GetCount() - 1);
		}
		break;
#endif
		case USE_CHANGE_COSTUME_ATTR:
		case USE_RESET_COSTUME_ATTR:
		{
			LPITEM item2;
			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;

			if (item2->IsEquipped())
			{
				BuffOnAttr_RemoveBuffsFromItem(item2);
			}

			if (ITEM_COSTUME != item2->GetType())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			{
				uint8_t bSubType = item2->GetSubType();
#ifdef ENABLE_ACCE_SYSTEM
				if (bSubType == COSTUME_ACCE)
					return false;
#endif

#ifdef ENABLE_STOLE_COSTUME
				if (bSubType == COSTUME_STOLE)
					return false;
#endif
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;

			if (item2->GetAttributeSetIndex() == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if (item2->GetAttributeCount() == 0)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 354, "");
#endif
				return false;
			}

			switch (item->GetSubType())
			{
			case USE_CHANGE_COSTUME_ATTR:
				item2->ChangeAttribute();
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "CHANGE_COSTUME_ATTR", buf);
				}
				break;
			case USE_RESET_COSTUME_ATTR:
				item2->ClearAttribute();
				item2->AlterToMagicItem();
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "RESET_COSTUME_ATTR", buf);
				}
				break;
			}

#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 392, "");
#endif
			item->SetCount(item->GetCount() - 1);
			break;
		}

		//  ACCESSORY_REFINE & ADD/CHANGE_ATTRIBUTES
		case USE_PUT_INTO_BELT_SOCKET:
		case USE_PUT_INTO_RING_SOCKET:
		case USE_PUT_INTO_ACCESSORY_SOCKET:
		case USE_ADD_ACCESSORY_SOCKET:
		case USE_CLEAN_SOCKET:
		case USE_CHANGE_ATTRIBUTE:
		case USE_CHANGE_ATTRIBUTE2:
		case USE_ADD_ATTRIBUTE:
		case USE_ADD_ATTRIBUTE2:
		{
			LPITEM item2;
			if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
				return false;

			if (item2->IsEquipped())
			{
				BuffOnAttr_RemoveBuffsFromItem(item2);
			}

			// [NOTE] ÄÚ½ºÆ¬ ¾ÆÀÌÅÛ¿¡´Â ¾ÆÀÌÅÛ ÃÖÃÊ »ý¼º½Ã ·£´ý ¼Ó¼ºÀ» ºÎ¿©ÇÏµÇ, Àç°æÀç°¡ µîµîÀº ¸·¾Æ´Þ¶ó´Â ¿äÃ»ÀÌ ÀÖ¾úÀ½.
			// ¿ø·¡ ANTI_CHANGE_ATTRIBUTE °°Àº ¾ÆÀÌÅÛ Flag¸¦ Ãß°¡ÇÏ¿© ±âÈ¹ ·¹º§¿¡¼­ À¯¿¬ÇÏ°Ô ÄÁÆ®·Ñ ÇÒ ¼ö ÀÖµµ·Ï ÇÒ ¿¹Á¤ÀÌ¾úÀ¸³ª
			// ±×µý°Å ÇÊ¿ä¾øÀ¸´Ï ´ÚÄ¡°í »¡¸® ÇØ´Þ·¡¼­ ±×³É ¿©±â¼­ ¸·À½... -_-
			if (ITEM_COSTUME == item2->GetType())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
				return false;

			switch (item->GetSubType())
			{
			case USE_CLEAN_SOCKET:
			{
				int i;
				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				{
					if (item2->GetSocket(i) == ITEM_BROKEN_METIN_VNUM)
						break;
				}

				if (i == ITEM_SOCKET_MAX_NUM)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 480, "");
#endif
					return false;
				}

				int j = 0;

				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				{
					if (item2->GetSocket(i) != ITEM_BROKEN_METIN_VNUM && item2->GetSocket(i) != 0)
						item2->SetSocket(j++, item2->GetSocket(i));
				}

				for (; j < ITEM_SOCKET_MAX_NUM; ++j)
				{
					if (item2->GetSocket(j) > 0)
						item2->SetSocket(j, 1);
				}

				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "CLEAN_SOCKET", buf);
				}

				item->SetCount(item->GetCount() - 1);

			}
			break;

			case USE_CHANGE_ATTRIBUTE:
			case USE_CHANGE_ATTRIBUTE2: // @fixme123
				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->GetAttributeCount() == 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 354, "");
#endif
					return false;
				}

				if ((GM_PLAYER == GetGMLevel()) && (false == test_server) && (g_dwItemBonusChangeTime > 0))
				{
					//
					// Event Flag ¸¦ ÅëÇØ ÀÌÀü¿¡ ¾ÆÀÌÅÛ ¼Ó¼º º¯°æÀ» ÇÑ ½Ã°£À¸·Î ºÎÅÍ ÃæºÐÇÑ ½Ã°£ÀÌ Èê·¶´ÂÁö °Ë»çÇÏ°í
					// ½Ã°£ÀÌ ÃæºÐÈ÷ Èê·¶´Ù¸é ÇöÀç ¼Ó¼ºº¯°æ¿¡ ´ëÇÑ ½Ã°£À» ¼³Á¤ÇØ ÁØ´Ù.
					//

					// uint32_t dwChangeItemAttrCycle = quest::CQuestManager::instance().GetEventFlag(msc_szChangeItemAttrCycleFlag);
					// if (dwChangeItemAttrCycle < msc_dwDefaultChangeItemAttrCycle)
						// dwChangeItemAttrCycle = msc_dwDefaultChangeItemAttrCycle;
					uint32_t dwChangeItemAttrCycle = g_dwItemBonusChangeTime;

					quest::PC* pPC = quest::CQuestManager::instance().GetPC(GetPlayerID());

					if (pPC)
					{
						uint32_t dwNowSec = get_global_time();

						uint32_t dwLastChangeItemAttrSec = pPC->GetFlag(msc_szLastChangeItemAttrFlag);

						if (dwLastChangeItemAttrSec + dwChangeItemAttrCycle > dwNowSec)
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 391, "%d#%d", dwChangeItemAttrCycle, dwChangeItemAttrCycle - (dwNowSec - dwLastChangeItemAttrSec));
#endif
							return false;
						}

						pPC->SetFlag(msc_szLastChangeItemAttrFlag, dwNowSec);
					}
				}

#ifdef ENABLE_CHANGE_ATTRIBUTE_RULES
				{
					uint32_t dwTargetVnum = item2->GetVnum();
					bool bZodiacItem = (

#ifdef DISABLE_ZODIAC_ATT

					(dwTargetVnum == 12314141)

						)
						? true : false;
#else
						((dwTargetVnum >= 19290) && (dwTargetVnum <= 19312)) ||
						((dwTargetVnum >= 19490) && (dwTargetVnum <= 19512)) ||
						((dwTargetVnum >= 19690) && (dwTargetVnum <= 19712)) ||
						((dwTargetVnum >= 19890) && (dwTargetVnum <= 19912)) ||
						((dwTargetVnum >= 300) && (dwTargetVnum <= 319)) ||
						(dwTargetVnum == 329) ||
						(dwTargetVnum == 339) ||
						(dwTargetVnum == 349) ||
						(dwTargetVnum == 359) ||
						(dwTargetVnum == 369) ||
						(dwTargetVnum == 379) ||
						(dwTargetVnum == 389) ||
						(dwTargetVnum == 399) ||
						((dwTargetVnum >= 1180) && (dwTargetVnum <= 1189)) ||
						(dwTargetVnum == 1199) ||
						(dwTargetVnum == 1209) ||
						(dwTargetVnum == 1219) ||
						(dwTargetVnum == 1229) ||
						((dwTargetVnum >= 2200) && (dwTargetVnum <= 2209)) ||
						(dwTargetVnum == 2219) ||
						(dwTargetVnum == 2229) ||
						(dwTargetVnum == 2239) ||
						(dwTargetVnum == 2249) ||
						((dwTargetVnum >= 3220) && (dwTargetVnum <= 3229)) ||
						(dwTargetVnum == 3239) ||
						(dwTargetVnum == 3249) ||
						(dwTargetVnum == 3259) ||
						(dwTargetVnum == 3269) ||
						((dwTargetVnum >= 5160) && (dwTargetVnum <= 5169)) ||
						(dwTargetVnum == 5179) ||
						(dwTargetVnum == 5189) ||
						(dwTargetVnum == 5199) ||
						(dwTargetVnum == 5209) ||
						((dwTargetVnum >= 7300) && (dwTargetVnum <= 7309)) ||
						(dwTargetVnum == 7319) ||
						(dwTargetVnum == 7329) ||
						(dwTargetVnum == 7339) ||
						(dwTargetVnum == 7349) ||
						((dwTargetVnum >= 8500) && (dwTargetVnum <= 8569)) ||
						((dwTargetVnum >= 8640) && (dwTargetVnum <= 8739))

						)
						? true : false;




					if (item->GetVnum() != 86060) {
						if (bZodiacItem) {
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 10, "%s", item2->GetName());
#endif
							return false;
						}
					}
					else if (!bZodiacItem) {
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 9, "%s", item2->GetName());
#endif
						return false;
					}
#endif
				}

#endif

#ifdef ENABLE_TALISMAN_ATTR
				if (item->GetVnum() == 86051 || item->GetVnum() == 88965)
				{
					if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
					{
						item2->ChangeAttribute();
						item->SetCount(item->GetCount() - 1);
#ifdef ENABLE_RANKING
						SetRankPoints(13, GetRankPoints(13) + 1);
#endif
						return true;
					}
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 681, "");
#endif
						return false;
					}
				}
				else if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 850, "");
#endif
					return false;
				}
#endif

				if (item->GetSubType() == USE_CHANGE_ATTRIBUTE2)
				{
					int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
					{
						0, 0, 30, 40, 3
					};

					item2->ChangeAttribute(aiChangeProb);
				}
				else if (item->GetVnum() == 76014)
				{
					int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
					{
						0, 10, 50, 39, 1
					};

					item2->ChangeAttribute(aiChangeProb);
				}
				else
				{
					// ¿¬Àç°æ Æ¯¼öÃ³¸®
					// Àý´ë·Î ¿¬Àç°¡ Ãß°¡ ¾ÈµÉ°Å¶ó ÇÏ¿© ÇÏµå ÄÚµùÇÔ.

					if (item->GetVnum() == 71151 || item->GetVnum() == 76023)
					{
						if ((item2->GetType() == ITEM_WEAPON)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_BODY)
#ifdef __USE_ADD_WITH_ALL_ITEMS__
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_HEAD)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_SHIELD)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_WRIST)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_FOOTS)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_NECK)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_EAR)
#endif
							)
						{
							bool bCanUse = true;
							for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
							{
#ifdef __ENABLE_GREEN_ITEM_LVL_30__
								if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 30)
#else
								if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 40)
#endif
								{
									bCanUse = false;
									break;
								}
							}
							if (false == bCanUse)
							{
#ifdef TEXTS_IMPROVEMENT
#ifdef __ENABLE_GREEN_ITEM_LVL_30__
								int iLimit = 30;
#else
								int iLimit = 40;
#endif
								ChatPacketNew(CHAT_TYPE_INFO, 682, "%d", iLimit);
#endif
								break;
							}
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 683, "");
#endif
							break;
						}
					}


#ifdef ENABLE_BATTLE_PASS
					uint8_t bBattlePassId = GetBattlePassId();
					if (bBattlePassId)
					{
						uint32_t dwItemVnum, dwUseCount;
						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM, &dwItemVnum, &dwUseCount))
						{
							if (dwItemVnum == item->GetVnum() && GetMissionProgress(USE_ITEM, bBattlePassId) < dwUseCount)
								UpdateMissionProgress(USE_ITEM, bBattlePassId, 1, dwUseCount);
						}

						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM1, &dwItemVnum, &dwUseCount))
						{
							if (dwItemVnum == item->GetVnum() && GetMissionProgress(USE_ITEM1, bBattlePassId) < dwUseCount)
								UpdateMissionProgress(USE_ITEM1, bBattlePassId, 1, dwUseCount);
						}

						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM2, &dwItemVnum, &dwUseCount))
						{
							if (dwItemVnum == item->GetVnum() && GetMissionProgress(USE_ITEM2, bBattlePassId) < dwUseCount)
								UpdateMissionProgress(USE_ITEM2, bBattlePassId, 1, dwUseCount);
						}
					}
#endif
					item2->ChangeAttribute();
				}

#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 392, "");
#endif

				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
					LogManager::instance().ItemLog(this, item, "CHANGE_ATTRIBUTE", buf);
				}

				item->SetCount(item->GetCount() - 1);
#ifdef ENABLE_RANKING
				if (item->GetVnum() == 86051 || item->GetVnum() == 88965)
					SetRankPoints(13, GetRankPoints(13) + 1);
				else
					SetRankPoints(12, GetRankPoints(12) + 1);
#endif
				break;

			case USE_ADD_ATTRIBUTE:
				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (item2->GetAttributeCount() < 5)
				{
#ifdef ENABLE_TALISMAN_ATTR
					if (item->GetVnum() == 86050 || item->GetVnum() == 88966) {
						if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
						{
#if defined(ENABLE_BUG_FIXES)
							if (item2->GetAttributeCount() == 4)
							{
#if defined(TEXTS_IMPROVEMENT)
								ChatPacketNew(CHAT_TYPE_INFO, 1359, "");
#endif
								return false;
							}
#endif

							item2->AddAttribute();
							item->SetCount(item->GetCount() - 1);
							return true;
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 681, "");
#endif
							return false;
						}
					}
					else if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 684, "");
#endif
						return false;
					}
#endif

					// ¿¬Àç°¡ Æ¯¼öÃ³¸®
					// Àý´ë·Î ¿¬Àç°¡ Ãß°¡ ¾ÈµÉ°Å¶ó ÇÏ¿© ÇÏµå ÄÚµùÇÔ.
					if (item->GetVnum() == 71152 || item->GetVnum() == 76024)
					{
						if ((item2->GetType() == ITEM_WEAPON)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_BODY)
#ifdef __USE_ADD_WITH_ALL_ITEMS__
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_HEAD)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_SHIELD)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_WRIST)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_FOOTS)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_NECK)
							|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_EAR)
#endif
							)
						{
							bool bCanUse = true;
							for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
							{
#ifdef __ENABLE_GREEN_ITEM_LVL_30__
								if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 30)
#else
								if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 40)
#endif
								{
									bCanUse = false;
									break;
								}
							}
							if (false == bCanUse)
							{
#ifdef TEXTS_IMPROVEMENT
#ifdef __ENABLE_GREEN_ITEM_LVL_30__
								int iLimit = 30;
#else
								int iLimit = 40;
#endif
								ChatPacketNew(CHAT_TYPE_INFO, 682, "%d", iLimit);
#endif
								break;
							}
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 683, "");
#endif
							break;
						}
					}
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());
#ifndef ENABLE_ENCHANT_CHANGES
					if (number(1, 100) <= aiItemAttributeAddPercent[item2->GetAttributeCount()])
#endif
					{
#ifdef ENABLE_MAX_ADD_ATTRIBUTE
						short AttributeCount = abs(1 - item->GetAttributeCount());//1 bonuszt ad hozz?a z?d er?
						for (int i = 0; i < AttributeCount; i++)
							item2->AddAttribute();
						item->SetCount(item->GetCount() - 1);// elvesz 1 db ot
#else
						item2->AddAttribute();
#endif
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 389, "");
#endif
						int iAddedIdx = item2->GetAttributeCount() - 1;
						LogManager::instance().ItemLog(
							GetPlayerID(),
							item2->GetAttributeType(iAddedIdx),
							item2->GetAttributeValue(iAddedIdx),
							item->GetID(),
							"ADD_ATTRIBUTE_SUCCESS",
							buf,
							GetDesc()->GetHostName(),
							item->GetOriginalVnum());
					}
#ifndef ENABLE_ENCHANT_CHANGES
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 390, "");
#endif
						LogManager::instance().ItemLog(this, item, "ADD_ATTRIBUTE_FAIL", buf);
					}

					item->SetCount(item->GetCount() - 1);
#endif
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ChatPacketNew(CHAT_TYPE_INFO, 308, "");
				}
#endif
				break;

			case USE_ADD_ATTRIBUTE2:
				// Ãàº¹ÀÇ ±¸½½
				// Àç°¡ºñ¼­¸¦ ÅëÇØ ¼Ó¼ºÀ» 4°³ Ãß°¡ ½ÃÅ² ¾ÆÀÌÅÛ¿¡ ´ëÇØ¼­ ÇÏ³ªÀÇ ¼Ó¼ºÀ» ´õ ºÙ¿©ÁØ´Ù.
				if (item2->GetAttributeSetIndex() == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				// ¼Ó¼ºÀÌ ÀÌ¹Ì 4°³ Ãß°¡ µÇ¾úÀ» ¶§¸¸ ¼Ó¼ºÀ» Ãß°¡ °¡´ÉÇÏ´Ù.
				if (item2->GetAttributeCount() == 4)
				{
#ifdef ENABLE_TALISMAN_ATTR
					if (item->GetVnum() == 86052 || item->GetVnum() == 88964)
					{
						if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
						{
							if (number(1, 100) <= 75) // % Successo di inserimeno Sfera Benedetta 75%
							{
								item2->AddAttribute();
								item->SetCount(item->GetCount() - 1);
								return true;
							}
							else
							{
								item->SetCount(item->GetCount() - 1);
#ifdef TEXTS_IMPROVEMENT
								ChatPacketNew(CHAT_TYPE_INFO, 390, "");
#endif
								return false;
							}
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 681, "");
#endif
							return false;
						}
					}
					else if (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_PENDANT)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 684, "");
#endif
						return false;
					}
#endif


					char buf[21];
					snprintf(buf, sizeof(buf), "%u", item2->GetID());

					if (number(1, 100) <= aiItemAttributeAddPercent[item2->GetAttributeCount()])
					{
#ifdef ENABLE_MAX_ADD_ATTRIBUTE
						short AttributeCount = abs(1 - item->GetAttributeCount());
						for (int i = 0; i < AttributeCount; i++)
							item2->AddAttribute();
#else
						item2->AddAttribute();
#endif
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 389, "");
#endif
						int iAddedIdx = item2->GetAttributeCount() - 1;
						LogManager::instance().ItemLog(
							GetPlayerID(),
							item2->GetAttributeType(iAddedIdx),
							item2->GetAttributeValue(iAddedIdx),
							item->GetID(),
							"ADD_ATTRIBUTE2_SUCCESS",
							buf,
							GetDesc()->GetHostName(),
							item->GetOriginalVnum());
					}
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 390, "");
#endif
						LogManager::instance().ItemLog(this, item, "ADD_ATTRIBUTE2_FAIL", buf);
					}

					item->SetCount(item->GetCount() - 1);
				}
				else if (item2->GetAttributeCount() == 5)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 308, "");
#endif
				}
				else if (item2->GetAttributeCount() < 4)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 339, "%d#%d#%d", 4, item2->GetAttributeCount(), 4);
#endif
				}
				else
				{
					// wtf ?!
					sys_err("ADD_ATTRIBUTE2 : Item has wrong AttributeCount(%d)", item2->GetAttributeCount());
				}
				break;

			case USE_ADD_ACCESSORY_SOCKET:
			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", item2->GetID());
				if (item2->GetType() == ITEM_BELT)
				{
					ChatPacket(CHAT_TYPE_INFO, "You can't add new slot's to belt items");
					return false;
					
				}
				if (item2->IsAccessoryForSocket())
				{
					if (item2->GetAccessorySocketMaxGrade() < ITEM_ACCESSORY_SOCKET_MAX_NUM)
					{
#ifdef ENABLE_ADDSTONE_FAILURE
						if (number(1, 100) <= 50)
#else
						if (1)
#endif
						{
							item2->SetAccessorySocketMaxGrade(item2->GetAccessorySocketMaxGrade() + 1);
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 387, "");
#endif
							LogManager::instance().ItemLog(this, item, "ADD_SOCKET_SUCCESS", buf);
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 386, "");
#endif
							LogManager::instance().ItemLog(this, item, "ADD_SOCKET_FAIL", buf);
						}

						item->SetCount(item->GetCount() - 1);
					}
#ifdef TEXTS_IMPROVEMENT
					else {
						ChatPacketNew(CHAT_TYPE_INFO, 428, "");
					}
#endif
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 425, "");
#endif
				}
			}
			break;

			case USE_PUT_INTO_BELT_SOCKET:
			case USE_PUT_INTO_ACCESSORY_SOCKET:
				if (item2->IsAccessoryForSocket())
				{
					if (item->CanPutInto(item2)) {
#ifdef ENABLE_INFINITE_RAFINES
						if (item2->GetSocket(0) > 86400 || item2->GetSocket(1) > 86400 || item2->GetSocket(2) > 86400) {
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 859, "");
#endif
							return false;
						}
#endif
						char buf[21];
						snprintf(buf, sizeof(buf), "%u", item2->GetID());

						if (item2->GetAccessorySocketGrade() < item2->GetAccessorySocketMaxGrade())
						{
							//if (number(1, 100) <= aiAccessorySocketPutPct[item2->GetAccessorySocketGrade()])
							//{
							item2->SetAccessorySocketGrade(item2->GetAccessorySocketGrade() + 1);
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 452, "");
#endif
							LogManager::instance().ItemLog(this, item, "PUT_SOCKET_SUCCESS", buf);
							//}
							//else
							//{
//#ifdef TEXTS_IMPROVEMENT
													//ChatPacketNew(CHAT_TYPE_INFO, 453, "");
//#endif
													//LogManager::instance().ItemLog(this, item, "PUT_SOCKET_FAIL", buf);
												//}

							item->SetCount(item->GetCount() - 1);
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							if (item2->GetAccessorySocketMaxGrade() == 0 || item2->GetAccessorySocketMaxGrade() < ITEM_ACCESSORY_SOCKET_MAX_NUM) {
								ChatPacketNew(CHAT_TYPE_INFO, 297, "");
								ChatPacketNew(CHAT_TYPE_INFO, 298, "");
							}
							else {
								ChatPacketNew(CHAT_TYPE_INFO, 337, "");
							}
#endif
						}
					}
#ifdef ENABLE_INFINITE_RAFINES
					else if (item->CanPutInto2(item2)) {
						if ((item2->GetSocket(0) > 5 && item2->GetSocket(0) <= 86400) || (item2->GetSocket(1) > 5 && item2->GetSocket(1) <= 86400) || (item2->GetSocket(2) > 5 && item2->GetSocket(2) <= 86400)) {
#ifdef TEXTS_IMPROVEMENT
							ChatPacketNew(CHAT_TYPE_INFO, 860, "");
#endif
							return false;
						}

						char buf[21];
						snprintf(buf, sizeof(buf), "%u", item2->GetID());

						if (item2->GetAccessorySocketGrade() < item2->GetAccessorySocketMaxGrade())
						{
							bool infinite = item->GetValue(0) == 1 ? true : false;
							if (infinite == true)
							{
								item2->SetAccessorySocketGrade(item2->GetAccessorySocketGrade() + 1, infinite);
#ifdef TEXTS_IMPROVEMENT
								ChatPacketNew(CHAT_TYPE_INFO, 452, "");
#endif
								LogManager::instance().ItemLog(this, item, "PUT_SOCKET_SUCCESS", buf);
							}
							else
							{
#ifdef TEXTS_IMPROVEMENT
								ChatPacketNew(CHAT_TYPE_INFO, 453, "");
#endif
								LogManager::instance().ItemLog(this, item, "PUT_SOCKET_FAIL", buf);
							}

							item->SetCount(item->GetCount() - 1);
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							if (item2->GetAccessorySocketMaxGrade() == 0 || item2->GetAccessorySocketMaxGrade() < ITEM_ACCESSORY_SOCKET_MAX_NUM) {
								ChatPacketNew(CHAT_TYPE_INFO, 297, "");
								ChatPacketNew(CHAT_TYPE_INFO, 298, "");
							}
							else {
								ChatPacketNew(CHAT_TYPE_INFO, 337, "");
							}
#endif
						}
					}
#endif
					else {
						ChatPacketNew(CHAT_TYPE_INFO, 425, "");
					}
				}
				else {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 425, "");
#endif
				}
				break;
			}
			if (item2->IsEquipped())
			{
				BuffOnAttr_AddBuffsFromItem(item2);
			}
		}
		break;
		//  END_OF_ACCESSORY_REFINE & END_OF_ADD_ATTRIBUTES & END_OF_CHANGE_ATTRIBUTES

		case USE_BAIT:
		{

			if (m_pkFishingEvent
#ifdef ENABLE_NEW_FISHING_SYSTEM
				|| m_pkFishingNewEvent
#endif
				)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 277, "");
#endif
				return false;
			}

			LPITEM weapon = GetWear(WEAR_WEAPON);

			if (!weapon || weapon->GetType() != ITEM_ROD)
				return false;

#ifdef TEXTS_IMPROVEMENT
			if (weapon->GetSocket(2)) {
				ChatPacketNew(CHAT_TYPE_INFO, 898, "%s", item->GetName());
			}
			else {
				ChatPacketNew(CHAT_TYPE_INFO, 282, "%s", item->GetName());
			}
#endif
			weapon->SetSocket(2, item->GetValue(0));
			item->SetCount(item->GetCount() - 1);
		}
		break;

		case USE_MOVE:
		case USE_TREASURE_BOX:
		case USE_MONEYBAG:
			break;

		case USE_AFFECT:
		{
			if (FindAffect(item->GetValue(0), aApplyInfo[item->GetValue(1)].bPointType)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
			}
			else
			{
				// PC_BANG_ITEM_ADD
				if (item->IsPCBangItem() == true)
				{
					// PC¹æÀÎÁö Ã¼Å©ÇØ¼­ Ã³¸®
					if (CPCBangManager::instance().IsPCBangIP(GetDesc()->GetHostName()) == false)
					{
#ifdef TEXTS_IMPROVEMENT
						ChatPacketNew(CHAT_TYPE_INFO, 426, "");
#endif
						return false;
					}
				}
				// END_PC_BANG_ITEM_ADD

				AddAffect(item->GetValue(0), aApplyInfo[item->GetValue(1)].bPointType, item->GetValue(2), 0, item->GetValue(3), 0, false);
				item->SetCount(item->GetCount() - 1);
			}
		}
		break;

		case USE_CREATE_STONE:
			AutoGiveItem(number(28000, 28013));
			item->SetCount(item->GetCount() - 1);
			break;

			// ¹°¾à Á¦Á¶ ½ºÅ³¿ë ·¹½ÃÇÇ Ã³¸®
		case USE_RECIPE:
		{
			LPITEM pSource1 = FindSpecifyItem(item->GetValue(1));
			int dwSourceCount1 = item->GetValue(2);

			LPITEM pSource2 = FindSpecifyItem(item->GetValue(3));
			int dwSourceCount2 = item->GetValue(4);

			if (dwSourceCount1 != 0)
			{
				if (pSource1 == nullptr)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 350, "");
#endif
					return false;
				}
			}

			if (dwSourceCount2 != 0)
			{
				if (pSource2 == nullptr)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 350, "");
#endif
					return false;
				}
			}

			if (pSource1 != nullptr)
			{
				if (pSource1->GetCount() < dwSourceCount1)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 454, "%s#%d#%d", pSource1->GetName(), dwSourceCount1, pSource1->GetCount());
#endif
					return false;
				}

				pSource1->SetCount(pSource1->GetCount() - dwSourceCount1);
			}

			if (pSource2 != nullptr)
			{
				if (pSource2->GetCount() < dwSourceCount2)
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 454, "%s#%d#%d", pSource2->GetName(), dwSourceCount1, pSource2->GetCount());
#endif
					return false;
				}

				pSource2->SetCount(pSource2->GetCount() - dwSourceCount2);
			}

			LPITEM pBottle = FindSpecifyItem(50901);

			if (!pBottle || pBottle->GetCount() < 1)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 359, "");
#endif
				return false;
			}

			pBottle->SetCount(pBottle->GetCount() - 1);

			if (number(1, 100) > item->GetValue(5))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 347, "");
#endif
				return false;
			}

			AutoGiveItem(item->GetValue(0));
		}
		break;
		}
	}
	break;
	case ITEM_METIN:
	{
		LPITEM item2;

		if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
			return false;

		if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
			return false;

		if (item2->GetType() == ITEM_PICK) return false;
		if (item2->GetType() == ITEM_ROD) return false;

		int i;

		for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			uint32_t dwVnum;

			if ((dwVnum = item2->GetSocket(i)) <= 2)
				continue;

			TItemTable* p = ITEM_MANAGER::instance().GetTable(dwVnum);

			if (!p)
				continue;
#ifdef KET_BONUSZOS_KOVEK
			const int32_t insV5 = item->GetValue(5);
			const int32_t insV4 = item->GetValue(4);

			

				const int32_t exV5 = p->alValues[5];
			const int32_t exV4 = p->alValues[4];

			// Ha barmelyi ko csopi egyezik barmelyikkel, akkor ne lehessen berakni csak ryuganak seggbe
			if ((insV5 && (insV5 == exV5 || insV5 == exV4)) ||
				(insV4 && (insV4 == exV5 || insV4 == exV4)))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 230, "");
#endif
				return false;
			}


#else
			if (item->GetValue(5) == p->alValues[5])
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 230, "");
#endif
				return false;
			}
#endif
		}

		if (item2->GetType() == ITEM_ARMOR)
		{
			if (!IS_SET(item->GetWearFlag(), WEARABLE_BODY) || !IS_SET(item2->GetWearFlag(), WEARABLE_BODY))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 420, "%s", item->GetName());
#endif
				return false;
			}
		}
		else if (item2->GetType() == ITEM_WEAPON)
		{
			if (!IS_SET(item->GetWearFlag(), WEARABLE_WEAPON))
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 419, "%s", item->GetName());
#endif
				return false;
			}
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 357, "");
#endif
			return false;
		}

		for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			if (item2->GetSocket(i) >= 1 && item2->GetSocket(i) <= 2 && item2->GetSocket(i) >= item->GetValue(2))
			{
				// ¼® È®·ü
#ifdef ENABLE_ADDSTONE_FAILURE
				if (number(1, 100) <= stone_chance) // Erfolgreich
#else
				if (number(1, 100) <= stone_chance) // Erfolgreich
#endif
				{
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 340, "");
#endif
					item2->SetSocket(i, item->GetVnum());
				}
				else
				{
					ChatPacketNew(CHAT_TYPE_INFO, 341, "");
					item2->SetSocket(i, ITEM_BROKEN_METIN_VNUM);
				}

				LogManager::instance().ItemLog(this, item2, "SOCKET", item->GetName());
#ifdef ENABLE_BUG_FIXES
				item->SetCount(item->GetCount() - 1);
#else
#ifdef ENABLE_STONE_STACKFIX
				item->SetCount(item->GetCount() - 1);
#else
				ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (METIN)");
#endif
#endif
				break;
			}

		if (i == ITEM_SOCKET_MAX_NUM)
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 357, "%s", item2->GetName());
#endif
	}
	break;

	case ITEM_AUTOUSE:
	case ITEM_MATERIAL:
	case ITEM_SPECIAL:
	case ITEM_TOOL:
	case ITEM_LOTTERY:
		break;

	case ITEM_TOTEM:
	{
		if (!item->IsEquipped())
			EquipItem(item);
	}
	break;

	case ITEM_BLEND:
		// »õ·Î¿î ¾àÃÊµé
		sys_log(0, "ITEM_BLEND!!");
		if (Blend_Item_find(item->GetVnum()))
		{
			int		affect_type = AFFECT_BLEND;
			int		apply_type = aApplyInfo[item->GetSocket(0)].bPointType;
			int		apply_value = item->GetSocket(1);
			int		apply_duration = item->GetSocket(2);

			if (FindAffect(affect_type, apply_type)) {
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
			}
			else
			{
				if (FindAffect(AFFECT_EXP_BONUS_EURO_FREE, POINT_RESIST_MAGIC)) {
#ifdef TEXTS_IMPROVEMENT
					ChatPacketNew(CHAT_TYPE_INFO, 442, "");
#endif
				}
				else
				{
#ifdef ENABLE_BUG_FIXES
					if (!m_bIsLoadedAffect) {
						return false;
					}
#endif

					AddAffect(affect_type, apply_type, apply_value, 0, apply_duration, 0, false);
					item->SetCount(item->GetCount() - 1);
				}
			}
		}
		break;
	case ITEM_EXTRACT:
	{
		LPITEM pDestItem = GetItem(DestCell);
		if (nullptr == pDestItem)
		{
			return false;
		}
		switch (item->GetSubType())
		{
		case EXTRACT_DRAGON_SOUL:
			if (pDestItem->IsDragonSoul())
			{
				return DSManager::instance().PullOut(this, NPOS, pDestItem, item);
			}
			return false;
		case EXTRACT_DRAGON_HEART:
			if (pDestItem->IsDragonSoul())
			{
				return DSManager::instance().ExtractDragonHeart(this, pDestItem, item);
			}
			return false;
		default:
			return false;
		}
	}
	break;

#ifdef ENABLE_SOUL_SYSTEM
	case ITEM_SOUL:
	{
		int iCurrentMinutes = (item->GetSocket(2) / 10000);
		int iCurrentStrike = (item->GetSocket(2) % 10000);

		if (iCurrentMinutes < 60)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 685, "");
#endif
			return false;
		}

		if (iCurrentStrike <= 0)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 686, "");
#endif
			return false;
		}

		uint8_t bSoulType = item->GetSubType();
		if (bSoulType >= SOUL_MAX_NUM)
			return false;

		int iAffectID = AFFECT_SOUL_RED + bSoulType;
		int iAffID = AFF_SOUL_RED + bSoulType;

		bool blockUse = false;
		const CAffect* pAffect = FindAffect(iAffectID);
		if (pAffect)
		{
			uint32_t dwSPCost = pAffect->lSPCost;
			if (item->GetID() == dwSPCost)
			{
				blockUse = true;
			}

			LPITEM currentItem = FindItemByID(pAffect->lSPCost);
			if (currentItem)
			{
				currentItem->Lock(false);
				currentItem->SetSocket(1, false);
			}

			RemoveAffect(const_cast<CAffect*>(pAffect));
		}

		if (!blockUse)
		{
			item->Lock(true);
			item->SetSocket(1, true);

			AddAffect(iAffectID, APPLY_NONE, 0, iAffID, INFINITE_AFFECT_DURATION, item->GetID(), true, false);
		}
	}
	break;
#endif

	case ITEM_NONE:
		sys_err("Item type NONE %s", item->GetName());
		break;

	default:
		sys_log(0, "UseItemEx: Unknown type %s %d", item->GetName(), item->GetType());
		return false;
	}

	return true;
}

int g_nPortalLimitTime = 10;

void TransformRefineItem(LPITEM pkOldItem, LPITEM pkNewItem);
void NotifyRefineSuccess(LPCHARACTER ch, LPITEM item, const char* way);
void NotifyRefineFail(LPCHARACTER ch, LPITEM item, const char* way, int success = 0);

void CHARACTER::ItemDivision(TItemPos Cell)
{
	ItemSystem::ItemDivision(GetEntityHandle(), Cell);
}

void CHARACTER::SetRefineNPC(LPCHARACTER ch)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ch->ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:: void CHARACTER::SetRefineNPC ");//INGAME_DEBUG_RAZOR93
#endif
	if (ch != nullptr)
	{
		m_dwRefineNPCVID = ch->GetPacketVID();
	}
	else
	{
		m_dwRefineNPCVID = 0;
	}
}

bool CHARACTER::DoRefine(LPITEM item, bool bMoneyOnly)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	this->ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:: bool CHARACTER::DoRefine ");
#endif
	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	//°³·® ½Ã°£Á¦ÇÑ : upgrade_refine_scroll.quest ¿¡¼­ °³·®ÈÄ 5ºÐÀÌ³»¿¡ ÀÏ¹Ý °³·®À»
	//ÁøÇàÇÒ¼ö ¾øÀ½
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			sys_log(0, "can't refine %d %s", GetPlayerID(), GetName());
			return false;
		}
	}

	const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());

	if (!prt)
		return false;

	uint32_t result_vnum = item->GetRefinedVnum();
	int64_t cost = ComputeRefineFee(prt->cost);

	if (result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_TUNING)
		return false;

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	// REFINE_COST
	if (GetGold() < cost)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 232, "");
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset_percent(this);
#endif
#endif
		return false;
	}

	if (!bMoneyOnly)
	{
		for (int i = 0; i < prt->material_count; ++i)
		{
			if (CountSpecifyItem(prt->materials[i].vnum) < prt->materials[i].count)
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 233, "");
#endif
				return false;
			}
		}

		for (int i = 0; i < prt->material_count; ++i)
			RemoveSpecifyItem(prt->materials[i].vnum, prt->materials[i].count);
	}

	int prob = number(1, 100);


#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	if (IsRefineThroughGuild() || bMoneyOnly)
	{
		prob -= 10;
	}

	int success_prob = prt->prob;
	success_prob += CRefineManager::instance().Result(this);
#else
	if (IsRefineThroughGuild() || bMoneyOnly)
		prob -= 10;

#endif
	// END_OF_REFINE_COST
#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	if (prob <= success_prob)
#else
	if (prob <= prt->prob)
#endif
	{
		// ¼º°ø! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ´Ù¸¥ ¾ÆÀÌÅÛ È¹µæ
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE SUCCESS", pkNewItem->GetName());

			uint8_t bCell = item->GetCell();


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == item->GetVnum() && GetMissionProgress(REFINE_ITEM, bBattlePassId) < dwCount)
						UpdateMissionProgress(REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			// DETAIL_REFINE_LOG
			NotifyRefineSuccess(this, item, IsRefineThroughGuild() ? "GUILD" : "POWER");
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");
			// END_OF_DETAIL_REFINE_LOG

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

			sys_log(0, "Refine Success %lld", (long long)cost);
			pkNewItem->AttrLog();
			//PointChange(POINT_GOLD, -cost);
			sys_log(0, "PayPee %lld", (long long)cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(this);
#endif
			PayRefineFee(cost);
			sys_log(0, "PayPee End %lld", cost);
		}
		else
		{
			// DETAIL_REFINE_LOG
			// ¾ÆÀÌÅÛ »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			sys_err("cannot create item %u", result_vnum);
			NotifyRefineFail(this, item, IsRefineThroughGuild() ? "GUILD" : "POWER");
			// END_OF_DETAIL_REFINE_LOG
		}
	}
	else
	{
		// ½ÇÆÐ! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁü.
		DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -cost);
		NotifyRefineFail(this, item, IsRefineThroughGuild() ? "GUILD" : "POWER");
		item->AttrLog();
		ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

		//PointChange(POINT_GOLD, -cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset(this);
#endif
		PayRefineFee(cost);
	}

	return true;
}

enum enum_RefineScrolls
{
	CHUKBOK_SCROLL = 0,
	HYUNIRON_CHN = 1, // Áß±¹¿¡¼­¸¸ »ç¿ë
	YONGSIN_SCROLL = 2,
	MUSIN_SCROLL = 3,
	YAGONG_SCROLL = 4,
	MEMO_SCROLL = 5,
	BDRAGON_SCROLL = 6,
#ifdef ENABLE_SOUL_SYSTEM
	SOUL_SCROLL = 9,
#endif
};

//#include <set>
#ifdef ENABLE_UPGRADE_NOTICE_BY_RAZOR93

std::set<uint32_t> allowedVnums = {
	1610, 1611, 1612, 1613,
	1630, 1631, 1632, 1633,
	1650, 1651, 1652, 1653,
	1670, 1671, 1672, 1673,
	1690, 1691, 1692, 1693,
	1710, 1711, 1712, 1713,
	1730, 1731, 1732, 1733,
	1750, 1751, 1752, 1753,
	1770, 1771, 1772, 1773,
	1790, 1791, 1792, 1793,
	1810, 1811, 1812, 1813,
	1850, 1851, 1852, 1853,
	1870, 1871, 1872, 1873,
	1890, 1891, 1892, 1893,
	1910, 1911, 1912, 1913,
	1930, 1931, 1932, 1933,
	1950, 1951, 1952, 1953,

	8060, 8061, 8062, 8063,
	8080, 8081, 8082, 8083,
	8100, 8101, 8102, 8103,
	8120, 8121, 8122, 8123,
	8140, 8141, 8142, 8143,
	8160, 8161, 8162, 8163,
	8200, 8201, 8202, 8203,
	8220, 8221, 8222, 8223,
	8240, 8241, 8242, 8243,
	8260, 8261, 8262, 8263,
	8280, 8281, 8282, 8283,
	8330, 8331, 8332, 8333,
	8360, 8361, 8362, 8363,
	8380, 8381, 8382, 8383,
	8400, 8401, 8402, 8403,
	8420, 8421, 8422, 8423,
	8440, 8441, 8442, 8443,

	12100, 12101, 12102, 12103,
	12104, 12105, 12106, 12107,
	12110, 12111,
	12112, 12113, 12114, 12115,

	12790, 12791, 12792, 12793,

	12810, 12811, 12812, 12813,
	12830, 12831, 12832, 12833,
	12850, 12851, 12852, 12853,
	12854, 12855, 12856, 12857,
	12860, 12861,
	12862, 12863, 12864, 12865,
	12866, 12867,

	13070, 13071, 13072, 13073,
	13090, 13091, 13092, 13093,

	13110, 13111, 13112, 13113,
	13130, 13131, 13132, 13133,
	13150, 13151, 13152, 13153,
	13170, 13171, 13172, 13173,

	14230, 14231, 14232, 14233,
	15010, 15011, 15012, 15013,

	15460, 15461, 15462, 15463,
	15464, 15465, 15466, 15467,

	16230, 16231, 16232, 16233,
	16590, 16591, 16592, 16593,
	17230, 17231, 17232, 17233,
	17580, 17581, 17582, 17583,
	19310, 19311, 19312,
	19510, 19511, 19512,
	19710, 19711, 19712,
	19910, 19911, 19912
};
#endif ENABLE_UPGRADE_NOTICE_BY_RAZOR93

#ifdef ENABLE_MUSIN_SCROLL_REFINE_100_SUCCESS_RAZOR93

bool CHARACTER::DoRefineWithScroll(LPITEM item)
{
	
	//if (item && IsRefineBlockedVnum(item->GetVnum()))
	//{
	//	ChatPacket(CHAT_TYPE_INFO, "Ezt a targyat nem lehet fejleszteni.");
	//	ClearRefineMode();
	//	return false;
	//}

	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	ClearRefineMode();

	//°³·® ½Ã°£Á¦ÇÑ : upgrade_refine_scroll.quest ¿¡¼­ °³·®ÈÄ 5ºÐÀÌ³»¿¡ ÀÏ¹Ý °³·®À»
		//ÁøÇàÇÒ¼ö ¾øÀ½
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			sys_log(0, "can't refine %d %s", GetPlayerID(), GetName());
			return false;
		}
	}

	const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());

	if (!prt)
		return false;

	LPITEM pkItemScroll;

	// °³·®¼­ Ã¼Å©
	if (m_iRefineAdditionalCell < 0)
		return false;

	pkItemScroll = GetInventoryItem(m_iRefineAdditionalCell);

	if (!pkItemScroll)
		return false;

	if (!(pkItemScroll->GetType() == ITEM_USE && pkItemScroll->GetSubType() == USE_TUNING))
		return false;

	if (pkItemScroll->GetVnum() == item->GetVnum())
		return false;

	uint32_t result_vnum = item->GetRefinedVnum();
	uint32_t result_fail_vnum = item->GetRefineFromVnum();

	if (result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	// MUSIN_SCROLL
	if (pkItemScroll->GetValue(0) == MUSIN_SCROLL)
	{
		
		//if (item->GetRefineLevel() >= 4)
		//{
		//	ChatPacket(CHAT_TYPE_INFO, "MAX +9 with this scroll!");
		//	return false;
		//}
	}
	// END_OF_MUSIC_SCROLL

	else if (pkItemScroll->GetValue(0) == MEMO_SCROLL)
	{
		if (item->GetRefineLevel() != pkItemScroll->GetValue(1))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 417, "%s#%s", item->GetName(), pkItemScroll->GetName());
#endif
			return false;
		}
	}
	else if (pkItemScroll->GetValue(0) == BDRAGON_SCROLL)
	{
		if (item->GetType() != ITEM_METIN || item->GetRefineLevel() != 4)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 665, "%s#%s", item->GetName(), pkItemScroll->GetName());
#endif
			return false;
		}
	}

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	if (GetGold() < prt->cost)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 232, "");
#endif
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset_percent(this);
#endif
		return false;
	}

	for (int i = 0; i < prt->material_count; ++i)
	{
		if (CountSpecifyItem(prt->materials[i].vnum) < prt->materials[i].count)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 233, "");
#endif
			return false;
		}
	}

	for (int i = 0; i < prt->material_count; ++i)
		RemoveSpecifyItem(prt->materials[i].vnum, prt->materials[i].count);

	int prob = number(1, 100);
	int success_prob = prt->prob;
	bool bDestroyWhenFail = false;

	const char* szRefineType = "SCROLL";

	if (pkItemScroll->GetValue(0) == HYUNIRON_CHN ||
		pkItemScroll->GetValue(0) == YONGSIN_SCROLL ||
		pkItemScroll->GetValue(0) == YAGONG_SCROLL) // ÇöÃ¶, ¿ë½ÅÀÇ Ãàº¹¼­, ¾ß°øÀÇ ºñÀü¼­  Ã³¸®
	{
		const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };
		const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

		if (pkItemScroll->GetValue(0) == YONGSIN_SCROLL)
		{
			success_prob = hyuniron_prob[MINMAX(0, item->GetRefineLevel(), 8)];
		}
		else if (pkItemScroll->GetValue(0) == YAGONG_SCROLL)
		{
			success_prob = yagong_prob[MINMAX(0, item->GetRefineLevel(), 8)];
		}
		else if (pkItemScroll->GetValue(0) == HYUNIRON_CHN) {} // @fixme121
		else
		{
			sys_err("REFINE : Unknown refine scroll item. Value0: %d", pkItemScroll->GetValue(0));
		}

		if (pkItemScroll->GetValue(0) == HYUNIRON_CHN) // ÇöÃ¶Àº ¾ÆÀÌÅÛÀÌ ºÎ¼­Á®¾ß ÇÑ´Ù.
			bDestroyWhenFail = true;

		// DETAIL_REFINE_LOG
		if (pkItemScroll->GetValue(0) == HYUNIRON_CHN)
		{
			szRefineType = "HYUNIRON";
		}
		else if (pkItemScroll->GetValue(0) == YONGSIN_SCROLL)
		{
			szRefineType = "GOD_SCROLL";
		}
		else if (pkItemScroll->GetValue(0) == YAGONG_SCROLL)
		{
			szRefineType = "YAGONG_SCROLL";
		}
		// END_OF_DETAIL_REFINE_LOG
	}
	// DETAIL_REFINE_LOG
	if (pkItemScroll->GetValue(0) == MUSIN_SCROLL)
	{
		
		success_prob += 100; // Musin izé mindig sikeres 
		if (success_prob > 100)
			success_prob = 100;

		szRefineType = "MUSIN_SCROLL";
	}
	// END_OF_DETAIL_REFINE_LOG
	else if (pkItemScroll->GetValue(0) == MEMO_SCROLL)
	{
		success_prob = 100;
		szRefineType = "MEMO_SCROLL";
	}
	else if (pkItemScroll->GetValue(0) == BDRAGON_SCROLL)
	{
		success_prob = 80;
		szRefineType = "BDRAGON_SCROLL";
	}

#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	success_prob += CRefineManager::instance().Result(this);

#endif
	pkItemScroll->SetCount(pkItemScroll->GetCount() - 1);

	if (prob <= success_prob)
	{
		// ¼º°ø! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ´Ù¸¥ ¾ÆÀÌÅÛ È¹µæ
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE SUCCESS", pkNewItem->GetName());

			uint8_t bCell = item->GetCell();


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == item->GetVnum() && GetMissionProgress(REFINE_ITEM, bBattlePassId) < dwCount)
						UpdateMissionProgress(REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			NotifyRefineSuccess(this, item, szRefineType);

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);



			pkNewItem->AttrLog();
			//PointChange(POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(this);
#endif
			PayRefineFee(prt->cost);
#ifdef ENABLE_UPGRADE_NOTICE_BY_RAZOR93
			if (pkNewItem->GetRefineLevel() >= 8)
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					pkNewItem->GetVnum(),
					pkNewItem->GetSocket(0),
					pkNewItem->GetSocket(1),
					pkNewItem->GetSocket(2),
					0, // transmute
					0  // transmute2 
				);

				// Bónuszok
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = pkNewItem->GetAttributeType(i);
					short val = pkNewItem->GetAttributeValue(i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				// debug log:
				//sys_log(0, "ItemLink Debug: %s", itemlink);
				//sys_log(0, "Socket0=%d Socket1=%d Socket2=%d",
					//pkNewItem->GetSocket(0),
					//pkNewItem->GetSocket(1),
					//pkNewItem->GetSocket(2));

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					GetName(), itemlink, pkNewItem->GetName());

				SPacketGGNotice packet;
				strlcpy(packet.szText, szChat, sizeof(packet.szText));
				//P2P_MANAGER::instance().Send(&packet, sizeof(packet));

				BroadcastNotice(szChat); // ez kell a jelenlegi ch-ra

			}


			if (allowedVnums.find(pkNewItem->GetVnum()) != allowedVnums.end())
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					pkNewItem->GetVnum(),
					pkNewItem->GetSocket(0),
					pkNewItem->GetSocket(1),
					pkNewItem->GetSocket(2),
					0, // transmute
					0  // transmute2 
				);

				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = pkNewItem->GetAttributeType(i);
					short val = pkNewItem->GetAttributeValue(i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					GetName(), itemlink, pkNewItem->GetName());

				ChatPacket(CHAT_TYPE_INFO, szChat);
			}
#endif ENABLE_UPGRADE_NOTICE_BY_RAZOR93
		}
		else
		{
			// ¾ÆÀÌÅÛ »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			sys_err("cannot create item %u", result_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}

	}
	else if (!bDestroyWhenFail && result_fail_vnum)
	{
		// ½ÇÆÐ! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ³·Àº µî±ÞÀÇ ¾ÆÀÌÅÛ È¹µæ
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_fail_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE FAIL", pkNewItem->GetName());

			uint8_t bCell = item->GetCell();


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == item->GetVnum() && GetMissionProgress(REFINE_ITEM, bBattlePassId) < dwCount)
						UpdateMissionProgress(REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
			NotifyRefineFail(this, item, szRefineType, -1);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

			pkNewItem->AttrLog();

			//PointChange(POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(this);
#endif
			PayRefineFee(prt->cost);
		}
		else
		{
			// ¾ÆÀÌÅÛ »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			sys_err("cannot create item %u", result_fail_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}
	}
	else
	{
		NotifyRefineFail(this, item, szRefineType); // °³·®½Ã ¾ÆÀÌÅÛ »ç¶óÁöÁö ¾ÊÀ½

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset(this);
#endif
		PayRefineFee(prt->cost);
	}

	return true;

}

#else

bool CHARACTER::DoRefineWithScroll(LPITEM item)
{
	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	ClearRefineMode();

	//°³·® ½Ã°£Á¦ÇÑ : upgrade_refine_scroll.quest ¿¡¼­ °³·®ÈÄ 5ºÐÀÌ³»¿¡ ÀÏ¹Ý °³·®À»
	//ÁøÇàÇÒ¼ö ¾øÀ½
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			sys_log(0, "can't refine %d %s", GetPlayerID(), GetName());
			return false;
		}
	}

	const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());

	if (!prt)
		return false;

	LPITEM pkItemScroll;

	// °³·®¼­ Ã¼Å©
	if (m_iRefineAdditionalCell < 0)
		return false;

	pkItemScroll = GetInventoryItem(m_iRefineAdditionalCell);

	if (!pkItemScroll)
		return false;

	if (!(pkItemScroll->GetType() == ITEM_USE && pkItemScroll->GetSubType() == USE_TUNING))
		return false;

	if (pkItemScroll->GetVnum() == item->GetVnum())
		return false;

	uint32_t result_vnum = item->GetRefinedVnum();
	uint32_t result_fail_vnum = item->GetRefineFromVnum();

	if (result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	// MUSIN_SCROLL
	if (pkItemScroll->GetValue(0) == MUSIN_SCROLL)
	{
		if (item->GetRefineLevel() >= 4)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 305, "");
#endif
			return false;
		}
	}
	// END_OF_MUSIC_SCROLL

	else if (pkItemScroll->GetValue(0) == MEMO_SCROLL)
	{
		if (item->GetRefineLevel() != pkItemScroll->GetValue(1))
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 417, "%s#%s", item->GetName(), pkItemScroll->GetName());
#endif
			return false;
		}
	}
	else if (pkItemScroll->GetValue(0) == BDRAGON_SCROLL)
	{
		if (item->GetType() != ITEM_METIN || item->GetRefineLevel() != 4)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 665, "%s#%s", item->GetName(), pkItemScroll->GetName());
#endif
			return false;
		}
	}

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	if (GetGold() < prt->cost)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 232, "");
#endif
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset_percent(this);
#endif
		return false;
	}

	for (int i = 0; i < prt->material_count; ++i)
	{
		if (CountSpecifyItem(prt->materials[i].vnum) < prt->materials[i].count)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 233, "");
#endif
			return false;
		}
	}

	for (int i = 0; i < prt->material_count; ++i)
		RemoveSpecifyItem(prt->materials[i].vnum, prt->materials[i].count);

	int prob = number(1, 100);
	int success_prob = prt->prob;
	bool bDestroyWhenFail = false;

	const char* szRefineType = "SCROLL";

	if (pkItemScroll->GetValue(0) == HYUNIRON_CHN ||
		pkItemScroll->GetValue(0) == YONGSIN_SCROLL ||
		pkItemScroll->GetValue(0) == YAGONG_SCROLL) // ÇöÃ¶, ¿ë½ÅÀÇ Ãàº¹¼­, ¾ß°øÀÇ ºñÀü¼­  Ã³¸®
	{
		const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };
		const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

		if (pkItemScroll->GetValue(0) == YONGSIN_SCROLL)
		{
			success_prob = hyuniron_prob[MINMAX(0, item->GetRefineLevel(), 8)];
		}
		else if (pkItemScroll->GetValue(0) == YAGONG_SCROLL)
		{
			success_prob = yagong_prob[MINMAX(0, item->GetRefineLevel(), 8)];
		}
		else if (pkItemScroll->GetValue(0) == HYUNIRON_CHN) {} // @fixme121
		else
		{
			sys_err("REFINE : Unknown refine scroll item. Value0: %d", pkItemScroll->GetValue(0));
		}

		if (pkItemScroll->GetValue(0) == HYUNIRON_CHN) // ÇöÃ¶Àº ¾ÆÀÌÅÛÀÌ ºÎ¼­Á®¾ß ÇÑ´Ù.
			bDestroyWhenFail = true;

		// DETAIL_REFINE_LOG
		if (pkItemScroll->GetValue(0) == HYUNIRON_CHN)
		{
			szRefineType = "HYUNIRON";
		}
		else if (pkItemScroll->GetValue(0) == YONGSIN_SCROLL)
		{
			szRefineType = "GOD_SCROLL";
		}
		else if (pkItemScroll->GetValue(0) == YAGONG_SCROLL)
		{
			szRefineType = "YAGONG_SCROLL";
		}
		// END_OF_DETAIL_REFINE_LOG
	}

	// DETAIL_REFINE_LOG
	if (pkItemScroll->GetValue(0) == MUSIN_SCROLL) // ¹«½ÅÀÇ Ãàº¹¼­´Â 100% ¼º°ø (+4±îÁö¸¸)
	{
		success_prob = 100;

		szRefineType = "MUSIN_SCROLL";
	}
	// END_OF_DETAIL_REFINE_LOG
	else if (pkItemScroll->GetValue(0) == MEMO_SCROLL)
	{
		success_prob = 100;
		szRefineType = "MEMO_SCROLL";
	}
	else if (pkItemScroll->GetValue(0) == BDRAGON_SCROLL)
	{
		success_prob = 80;
		szRefineType = "BDRAGON_SCROLL";
	}

#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	success_prob += CRefineManager::instance().Result(this);

#endif
	pkItemScroll->SetCount(pkItemScroll->GetCount() - 1);

	if (prob <= success_prob)
	{
		// ¼º°ø! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ´Ù¸¥ ¾ÆÀÌÅÛ È¹µæ
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE SUCCESS", pkNewItem->GetName());

			uint8_t bCell = item->GetCell();


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == item->GetVnum() && GetMissionProgress(REFINE_ITEM, bBattlePassId) < dwCount)
						UpdateMissionProgress(REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			NotifyRefineSuccess(this, item, szRefineType);

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);



			pkNewItem->AttrLog();
			//PointChange(POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(this);
#endif
			PayRefineFee(prt->cost);
#ifdef ENABLE_UPGRADE_NOTICE_BY_RAZOR93
			if (pkNewItem->GetRefineLevel() >= 8)
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					pkNewItem->GetVnum(),
					pkNewItem->GetSocket(0),
					pkNewItem->GetSocket(1),
					pkNewItem->GetSocket(2),
					0, // transmute
					0  // transmute2 
				);

				// Bónuszok
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = pkNewItem->GetAttributeType(i);
					short val = pkNewItem->GetAttributeValue(i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				// debug log:
				//sys_log(0, "ItemLink Debug: %s", itemlink);
				//sys_log(0, "Socket0=%d Socket1=%d Socket2=%d",
					//pkNewItem->GetSocket(0),
					//pkNewItem->GetSocket(1),
					//pkNewItem->GetSocket(2));

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					GetName(), itemlink, pkNewItem->GetName());

				SPacketGGNotice packet;
				strlcpy(packet.szText, szChat, sizeof(packet.szText));
				//P2P_MANAGER::instance().Send(&packet, sizeof(packet));

				BroadcastNotice(szChat); // ez kell a jelenlegi ch-ra

			}


			if (allowedVnums.find(pkNewItem->GetVnum()) != allowedVnums.end())
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					pkNewItem->GetVnum(),
					pkNewItem->GetSocket(0),
					pkNewItem->GetSocket(1),
					pkNewItem->GetSocket(2),
					0, // transmute
					0  // transmute2 
				);

				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = pkNewItem->GetAttributeType(i);
					short val = pkNewItem->GetAttributeValue(i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					GetName(), itemlink, pkNewItem->GetName());

				ChatPacket(CHAT_TYPE_INFO, szChat);
			}
#endif ENABLE_UPGRADE_NOTICE_BY_RAZOR93
		}
		else
		{
			// ¾ÆÀÌÅÛ »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			sys_err("cannot create item %u", result_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}

	}
	else if (!bDestroyWhenFail && result_fail_vnum)
	{
		// ½ÇÆÐ! ¸ðµç ¾ÆÀÌÅÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ³·Àº µî±ÞÀÇ ¾ÆÀÌÅÛ È¹µæ
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_fail_vnum, 1, 0, false);

		if (pkNewItem)
		{
			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE FAIL", pkNewItem->GetName());

			uint8_t bCell = item->GetCell();


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = GetBattlePassId();
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == item->GetVnum() && GetMissionProgress(REFINE_ITEM, bBattlePassId) < dwCount)
						UpdateMissionProgress(REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
			NotifyRefineFail(this, item, szRefineType, -1);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

			pkNewItem->AttrLog();

			//PointChange(POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(this);
#endif
			PayRefineFee(prt->cost);
		}
		else
		{
			// ¾ÆÀÌÅÛ »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			sys_err("cannot create item %u", result_fail_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}
	}
	else
	{
		NotifyRefineFail(this, item, szRefineType); // °³·®½Ã ¾ÆÀÌÅÛ »ç¶óÁöÁö ¾ÊÀ½

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset(this);
#endif
		PayRefineFee(prt->cost);
	}

	return true;

}

#endif
#ifdef ENABLE_SOUL_SYSTEM

bool CHARACTER::DoRefineItemSoul(LPITEM item)
{
	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	ClearRefineMode();

	LPITEM pkItemScroll;

	if (m_iRefineAdditionalCell < 0)
		return false;

	pkItemScroll = GetInventoryItem(m_iRefineAdditionalCell);

	if (!pkItemScroll)
		return false;

	if (!(pkItemScroll->GetType() == ITEM_USE && pkItemScroll->GetSubType() == USE_TUNING))
		return false;

	if (pkItemScroll->GetVnum() == item->GetVnum())
		return false;

	uint32_t resultVnum = item->GetRefinedVnum();

	if (resultVnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 666, "%s", item->GetName());
#endif
		return false;
	}

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
		sys_err("DoRefineWithScroll NOT GET ITEM PROTO %d", item->GetRefinedVnum());
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	int prob = number(1, 100);
	int successProb = pkItemScroll->GetValue(1);

	pkItemScroll->SetCount(pkItemScroll->GetCount() - 1);

	if (prob <= successProb)
	{
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(resultVnum, 1, 0, false);
		if (pkNewItem)
		{
			uint8_t bCell = item->GetCell();
			ChatPacket(CHAT_TYPE_COMMAND, "RefineSoulSuceeded");
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);
		}
		else
		{
			sys_err("Cannot create item soul %u", resultVnum);
			ChatPacket(CHAT_TYPE_COMMAND, "RefineSoulFailed");
		}
	}
	else
	{
		ChatPacket(CHAT_TYPE_COMMAND, "RefineSoulFailed");
	}

	return true;
}
#endif

bool CHARACTER::RefineInformation(uint8_t bCell, uint8_t bType, int iAdditionalCell)
{
	if (bCell > INVENTORY_MAX_NUM)
		return false;

	LPITEM item = GetInventoryItem(bCell);



	if (!item)
		return false;

#ifdef ATTR_LOCK
	if (item->GetLockedAttr() != -1)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 784, "");
#endif
		return false;
	}
#endif

	// REFINE_COST
	if (bType == REFINE_TYPE_MONEY_ONLY && !GetQuestFlag("deviltower_zone.can_refine"))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 361, "");
#endif
		return false;
	}
	// END_OF_REFINE_COST

	TPacketGCRefineInformation p;

	p.header = HEADER_GC_REFINE_INFORMATION;
	p.pos = bCell;
	p.src_vnum = item->GetVnum();
	p.result_vnum = item->GetRefinedVnum();
	p.type = bType;

	if (p.result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_TUNING)
	{
		if (bType == 0)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 424, "");
#endif
			return false;
		}
		else
		{
			LPITEM itemScroll = GetInventoryItem(iAdditionalCell);
			if (!itemScroll || item->GetVnum() == itemScroll->GetVnum())
			{
#ifdef TEXTS_IMPROVEMENT
				ChatPacketNew(CHAT_TYPE_INFO, 229, "");
#endif
				return false;
			}
		}
	}

#ifdef ENABLE_SOUL_SYSTEM
	if (bType == REFINE_TYPE_SOUL)
	{
		LPITEM itemScroll = GetInventoryItem(iAdditionalCell);
		if (!itemScroll)
			return false;

		p.cost = 0;
		p.prob = itemScroll->GetValue(1);
		p.material_count = 0;
		memset(p.materials, 0, sizeof(p.materials));

		GetDesc()->Packet(&p, sizeof(TPacketGCRefineInformation));

		SetRefineMode(iAdditionalCell);
		return true;
	}
#endif

	CRefineManager& rm = CRefineManager::instance();

	const TRefineTable* prt = rm.GetRefineRecipe(item->GetRefineSet());

	if (!prt)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	p.cost = ComputeRefineFee(prt->cost);
#ifdef NEW_POINT_EXP_DOUBLE_BONUS_RAZOR93
	int success_prob = prt->prob;

	// Kijelzett esély igazítása scroll típus alapján (hogy a kliens ugyanazt lássa, mint amit a szerver használ)
	if (bType != REFINE_TYPE_MONEY_ONLY)
	{
		LPITEM pkScroll = GetInventoryItem(iAdditionalCell);
		if (pkScroll && pkScroll->GetType() == ITEM_USE && pkScroll->GetSubType() == USE_TUNING)
		{
			const int scrollType = pkScroll->GetValue(0);

			if (scrollType == YONGSIN_SCROLL || scrollType == YAGONG_SCROLL || scrollType == HYUNIRON_CHN)
			{
				const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };
				const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

				if (scrollType == YONGSIN_SCROLL)
					success_prob = hyuniron_prob[MINMAX(0, item->GetRefineLevel(), 8)];
				else if (scrollType == YAGONG_SCROLL)
					success_prob = yagong_prob[MINMAX(0, item->GetRefineLevel(), 8)];
				// HYUNIRON_CHN: marad a prt->prob
			}
			else if (scrollType == MUSIN_SCROLL)
			{
				//if (item->GetRefineLevel() >= 9)
				//{
				//	ChatPacket(CHAT_TYPE_INFO, "MAX +9 with this scroll!");
				//	return false;
				//}
				success_prob += 100;
				if (success_prob > 100)
					success_prob = 100;
			}
			else if (scrollType == MEMO_SCROLL)
			{
				if (item->GetRefineLevel() != pkScroll->GetValue(1))
					return false;
				success_prob = 100;
			}
			else if (scrollType == BDRAGON_SCROLL)
			{
				if (item->GetType() != ITEM_METIN || item->GetRefineLevel() != 4)
					return false;
				success_prob = 80;
			}
		}
	}

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	success_prob += CRefineManager::instance().Result(this);
#endif

	success_prob = MINMAX(0, success_prob, 100);
	p.prob = success_prob;
#else
	p.prob = prt->prob;
#endif
	if (bType == REFINE_TYPE_MONEY_ONLY)
	{
		p.material_count = 0;
		memset(p.materials, 0, sizeof(p.materials));
	}
	else
	{
		p.material_count = prt->material_count;
		memcpy(&p.materials, prt->materials, sizeof(prt->materials));
	}

	GetDesc()->Packet(&p, sizeof(TPacketGCRefineInformation));

	SetRefineMode(iAdditionalCell);
	return true;
}

bool CHARACTER::RefineItem(LPITEM pkItem, LPITEM pkTarget)
{
	if (!CanHandleItem())
		return false;

#ifdef ENABLE_SOUL_SYSTEM
	uint32_t vnum = pkItem->GetVnum();
	if ((vnum == 70602 || vnum == 70603 || vnum == 88958) && pkTarget->GetType() != ITEM_SOUL) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 1294, "%s", pkItem->GetName());
#endif
		return false;
	}
#endif

	if (pkItem->GetSubType() == USE_TUNING)
	{
		// XXX ¼º´É, ¼ÒÄÏ °³·®¼­´Â »ç¶óÁ³½À´Ï´Ù...
		// XXX ¼º´É°³·®¼­´Â Ãàº¹ÀÇ ¼­°¡ µÇ¾ú´Ù!
		// MUSIN_SCROLL
		if (pkItem->GetValue(0) == MUSIN_SCROLL)
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_MUSIN, pkItem->GetCell());
		// END_OF_MUSIN_SCROLL

#ifdef ENABLE_SOUL_SYSTEM
		else if (pkItem->GetValue(0) == SOUL_SCROLL)
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_SOUL, pkItem->GetCell());
#endif

		else if (pkItem->GetValue(0) == HYUNIRON_CHN)
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_HYUNIRON, pkItem->GetCell());
		else if (pkItem->GetValue(0) == BDRAGON_SCROLL)
		{
			if (pkTarget->GetRefineSet() != 702) return false;
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_BDRAGON, pkItem->GetCell());
		}
		else
		{
			if (pkTarget->GetRefineSet() == 501) return false;
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_SCROLL, pkItem->GetCell());
		}
	}
	else if (pkItem->GetSubType() == USE_DETACHMENT && IS_SET(pkTarget->GetFlag(), ITEM_FLAG_REFINEABLE))
	{
		LogManager::instance().ItemLog(this, pkTarget, "USE_DETACHMENT", pkTarget->GetName());

		bool bHasMetinStone = false;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
		{
			int32_t socket = pkTarget->GetSocket(i);
			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
			{
				bHasMetinStone = true;
				break;
			}
		}

		if (bHasMetinStone)
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			{
				int32_t socket = pkTarget->GetSocket(i);
				if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				{
					AutoGiveItem(socket);
					//TItemTable* pTable = ITEM_MANAGER::instance().GetTable(pkTarget->GetSocket(i));
					//pkTarget->SetSocket(i, pTable->alValues[2]);
					// ±úÁøµ¹·Î ´ëÃ¼ÇØÁØ´Ù
					pkTarget->SetSocket(i, ITEM_BROKEN_METIN_VNUM);
				}
			}
			pkItem->SetCount(pkItem->GetCount() - 1);
			return true;
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 360, "");
#endif
			return false;
		}
	}

	return false;
}

void CHARACTER::__OpenPrivateShop(
#ifdef KASMIR_PAKET_SYSTEM
	bool bKasmir
#endif
)
{
#ifdef ENABLE_OPEN_SHOP_WITH_ARMOR
#ifdef KASMIR_PAKET_SYSTEM
	if (bKasmir) {
		ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShopKasmir");
		return;
	}
#endif
	ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShop");
#else
	unsigned bodyPart = GetPart(PART_MAIN);
	switch (bodyPart)
	{
	case 0:
	case 1:
	case 2: {
#ifdef KASMIR_PAKET_SYSTEM
		if (bKasmir) {
			ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShopKasmir");
			break;
		}
#endif

		ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShop");
	}
		  break;
	default:
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 503, "");
#endif
		break;
	}
#endif
}

// MYSHOP_PRICE_LIST

void CHARACTER::SendMyShopPriceListCmd(uint32_t dwItemVnum, int64_t dwItemPrice)
{
	char szLine[256];
	snprintf(szLine, sizeof(szLine), "MyShopPriceList %u %lld", dwItemVnum, dwItemPrice);
	ChatPacket(CHAT_TYPE_COMMAND, szLine);
	sys_log(0, szLine);
}


//
// DB Ä³½Ã·Î ºÎÅÍ ¹ÞÀº ¸®½ºÆ®¸¦ User ¿¡°Ô Àü¼ÛÇÏ°í »óÁ¡À» ¿­¶ó´Â Ä¿¸Çµå¸¦ º¸³½´Ù.
//

void CHARACTER::UseSilkBotaryReal(const TPacketMyshopPricelistHeader * p)
{
	const TItemPriceInfo* pInfo = (const TItemPriceInfo*)(p + 1);

	if (!p->byCount)
		// °¡°Ý ¸®½ºÆ®°¡ ¾ø´Ù. dummy µ¥ÀÌÅÍ¸¦ ³ÖÀº Ä¿¸Çµå¸¦ º¸³»ÁØ´Ù.
		SendMyShopPriceListCmd(1, 0);
	else {
		for (int idx = 0; idx < p->byCount; idx++)
			SendMyShopPriceListCmd(pInfo[idx].dwVnum, pInfo[idx].dwPrice);
	}

#ifdef KASMIR_PAKET_SYSTEM
	__OpenPrivateShop(m_bKasmirPaketDurum);
#else
	__OpenPrivateShop();
#endif
}

//
// ÀÌ¹ø Á¢¼Ó ÈÄ Ã³À½ »óÁ¡À» Open ÇÏ´Â °æ¿ì ¸®½ºÆ®¸¦ Load ÇÏ±â À§ÇØ DB Ä³½Ã¿¡ °¡°ÝÁ¤º¸ ¸®½ºÆ® ¿äÃ» ÆÐÅ¶À» º¸³½´Ù.
// ÀÌÈÄºÎÅÍ´Â ¹Ù·Î »óÁ¡À» ¿­¶ó´Â ÀÀ´äÀ» º¸³½´Ù.
//

void CHARACTER::UseSilkBotary(void)
{
	if (m_bNoOpenedShop) {
		uint32_t dwPlayerID = GetPlayerID();
		db_clientdesc->DBPacket(HEADER_GD_MYSHOP_PRICELIST_REQ, GetDesc()->GetHandle(), &dwPlayerID, sizeof(uint32_t));
		m_bNoOpenedShop = false;
	}
	else {
#ifdef KASMIR_PAKET_SYSTEM
		__OpenPrivateShop(m_bKasmirPaketDurum);
#else
		__OpenPrivateShop();
#endif
	}
}
// END_OF_MYSHOP_PRICE_LIST

void CHARACTER::SetRefineMode(int iAdditionalCell)
{
	m_iRefineAdditionalCell = iAdditionalCell;
	m_bUnderRefine = true;
}

void CHARACTER::ClearRefineMode()
{
	m_bUnderRefine = false;
	SetRefineNPC(nullptr);
}


void TransformRefineItem(LPITEM pkOldItem, LPITEM pkNewItem)
{

	// ACCESSORY_REFINE
	if (pkOldItem->IsAccessoryForSocket())
	{
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			pkNewItem->SetSocket(i, pkOldItem->GetSocket(i));
		}
		//pkNewItem->StartAccessorySocketExpireEvent();
	}
	// END_OF_ACCESSORY_REFINE
	else
	{
		// ¿©±â¼­ ±úÁø¼®ÀÌ ÀÚµ¿ÀûÀ¸·Î Ã»¼Ò µÊ
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			if (!pkOldItem->GetSocket(i))
				break;
			else
				pkNewItem->SetSocket(i, 1);
		}

		// ¼ÒÄÏ ¼³Á¤
		int slot = 0;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			int32_t socket = pkOldItem->GetSocket(i);

			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				pkNewItem->SetSocket(slot++, socket);
		}

	}

	// ¸ÅÁ÷ ¾ÆÀÌÅÛ ¼³Á¤
	pkOldItem->CopyAttributeTo(pkNewItem);
}

void NotifyRefineSuccess(LPCHARACTER ch, LPITEM item, const char* way)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ch->ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::void NotifyRefineSuccess ");//INGAME_DEBUG_RAZOR93
#endif
	if (nullptr != ch && item != nullptr)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "RefineSuceeded");

		LogManager::instance().RefineLog(ch->GetPlayerID(), item->GetName(), item->GetID(), item->GetRefineLevel(), 1, way);
	}
}

void NotifyRefineFail(LPCHARACTER ch, LPITEM item, const char* way, int success)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ch->ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:: void NotifyRefineFail ");//INGAME_DEBUG_RAZOR93
#endif
	if (nullptr != ch && nullptr != item)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "RefineFailed");

		LogManager::instance().RefineLog(ch->GetPlayerID(), item->GetName(), item->GetID(), item->GetRefineLevel(), success, way);
	}
}


void CHARACTER::RemoveSpecifyTypeItem(uint8_t type, int count)
{
	if (0 == count)
		return;

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	for (int i = 0; i < Inventory_Size(); ++i)
#else
	for (UINT i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
	{
		LPITEM item = GetInventoryItem(i);
		if (!item)
			continue;

		if (GetInventoryItem(i)->GetType() != type)
			continue;


		if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
			continue;

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
}

void CHARACTER::AutoGiveItem(LPITEM item, bool longOwnerShip
#ifdef __HIGHLIGHT_SYSTEM__
	, bool isHighLight
#endif
)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::void CHARACTER::AutoGiveItem(LPITEM item, bool longOwnerShip,");//INGAME_DEBUG_RAZOR93
#endif
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	sys_log(0, "Razor93 LOG:: Called: void CHARACTER::AutoGiveItem(LPITEM item, bool longOwnerShip");
#endif
	if (nullptr == item)
	{
		sys_err("NULL point.");
		return;
	}
	if (item->GetOwner())
	{
		sys_err("item %d 's owner exists!", item->GetID());
		return;
	}

#ifdef ENABLE_EXTRA_INVENTORY
	if (item->IsExtraItem() && item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
	{
#ifdef ENABLE_NEW_STACK_LIMIT
		int
#else
		uint8_t
#endif
			bCount = item->GetCount();
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
				item2->SetCount(item2->GetCount() + bCount2);
				if (bCount == 0) {
					item->SetCount(0);
					M2_DESTROY_ITEM(item);
					return;
				}
				else {
					item->SetCount(bCount);
				}
			}
		}
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
				item2->SetCount(item2->GetCount() + bCount2);
				if (bCount == 0) {
					item->SetCount(0);
					M2_DESTROY_ITEM(item);
					return;
				}
				else {
					item->SetCount(bCount);
				}
			}
		}
	}

	int cell;
	if (item->IsDragonSoul())
	{
		cell = GetEmptyDragonSoulInventory(item);
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (item->IsExtraItem())
	{
		cell = GetEmptyExtraInventory(item);
	}
#endif
	else
	{
		cell = GetEmptyInventory(item->GetSize());
	}

	if (cell != -1)
	{
		if (item->IsDragonSoul())
			item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, cell)
#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);
#ifdef ENABLE_EXTRA_INVENTORY
		else if (item->IsExtraItem())
			item->AddToCharacter(this, TItemPos(EXTRA_INVENTORY, cell)
#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);
#endif
		else
			item->AddToCharacter(this, TItemPos(INVENTORY, cell)
#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);

		LogManager::instance().ItemLog(this, item, "SYSTEM", item->GetName());

		if (item->GetType() == ITEM_USE && item->GetSubType() == USE_POTION)
		{
			TQuickslot* pSlot;

			if (GetQuickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE)
			{
				TQuickslot slot;
				slot.type = QUICKSLOT_TYPE_ITEM;
				slot.pos = cell;
				SetQuickslot(0, slot);
			}
		}
	}
	else
	{
		item->AddToGround(GetMapIndex(), GetXYZ());
#ifdef ENABLE_NEWSTUFF
		item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
		item->StartDestroyEvent();
#endif

		if (longOwnerShip)
			item->SetOwnership(this, 300);
		else
			item->SetOwnership(this, 60);
		LogManager::instance().ItemLog(this, item, "SYSTEM_DROP", item->GetName());
	}
}

#ifdef ENABLE_DS_REFINE_ALL
bool CHARACTER::AutoGiveDS(LPITEM item, bool longOwnerShip) {
	if (item == nullptr) {
		sys_err("NULL point.");
		return false;
	}

	if (item->GetOwner()) {
		sys_err("item %d 's owner exists!", item->GetID());
		return false;
	}

	if (!item->IsDragonSoul()) {
		sys_err("item %d is not alchemy!", item->GetID());
		return false;
	}

	int cell = GetEmptyDragonSoulInventory(item);
	if (cell != -1)
	{
		item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, cell)
#ifdef __HIGHLIGHT_SYSTEM__
			, true
#endif
		);

		LogManager::instance().ItemLog(this, item, "SYSTEM", item->GetName());
	}
	else
	{
		item->AddToGround(GetMapIndex(), GetXYZ());
#ifdef ENABLE_NEWSTUFF
		item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
		item->StartDestroyEvent();
#endif

		if (longOwnerShip) {
			item->SetOwnership(this, 300);
		}
		else {
			item->SetOwnership(this, 60);
		}

		LogManager::instance().ItemLog(this, item, "SYSTEM_DROP", item->GetName());
	}

	return true;
}
#endif

LPITEM CHARACTER::AutoGiveItem(uint32_t dwItemVnum,
#ifdef ENABLE_NEW_STACK_LIMIT
	int
#else
	uint8_t
#endif
	bCount, int iRarePct, bool bMsg
#ifdef __HIGHLIGHT_SYSTEM__
	, bool isHighLight
#endif
)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::LPITEM CHARACTER::AutoGiveItem(uint32_t dwItemVnum,");//INGAME_DEBUG_RAZOR93
#endif
	TItemTable* p = ITEM_MANAGER::instance().GetTable(dwItemVnum);

	if (!p)
		return nullptr;

	DBManager::instance().SendMoneyLog(MONEY_LOG_DROP, dwItemVnum, bCount);


#ifdef ENABLE_EXTRA_INVENTORY
	if (p->dwFlags & ITEM_FLAG_STACKABLE && ITEM_MANAGER::instance().IsExtraItem(dwItemVnum))
	{
		for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
		{
			LPITEM item = GetExtraInventoryItem(i);

			if (!item)
				continue;

			if (item->GetVnum() == dwItemVnum && FN_check_item_socket(item))
			{
				if (IS_SET(p->dwFlags, ITEM_FLAG_MAKECOUNT))
				{
					if (bCount < p->alValues[1])
						bCount = p->alValues[1];
				}

#ifdef ENABLE_NEW_STACK_LIMIT
				int
#else
				uint8_t
#endif
					bCount2 = std::min(g_bItemCountLimit - item->GetCount(), bCount); // change type for some
				bCount -= bCount2;

				item->SetCount(item->GetCount() + bCount2);

				if (bCount == 0)
				{
#ifdef TEXTS_IMPROVEMENT
					if (bMsg) {
						ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
							CHAT_TYPE_INFO_ITEM
#else
							CHAT_TYPE_INFO
#endif
							, 102, "%d#%s", bCount2, item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
						//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 08 |cffffff00%u jelenlegi:%u x %s|r", bCount2, item->GetCount(), item->GetName());

					}
#endif

					return item;
				}
			}
		}
	}
	else if (p->dwFlags & ITEM_FLAG_STACKABLE && p->bType != ITEM_BLEND)
#else
	if (p->dwFlags & ITEM_FLAG_STACKABLE && p->bType != ITEM_BLEND)
#endif
	{
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
		{
			LPITEM item = GetInventoryItem(i);

			if (!item)
				continue;

#ifdef ENABLE_SORT_INVEN
			if (item->GetOriginalVnum() == dwItemVnum && FN_check_item_socket(item))
#else
			if (item->GetVnum() == dwItemVnum && FN_check_item_socket(item))
#endif
			{
				if (IS_SET(p->dwFlags, ITEM_FLAG_MAKECOUNT))
				{
					if (bCount < p->alValues[1])
						bCount = p->alValues[1];
				}

#ifdef ENABLE_NEW_STACK_LIMIT
				int
#else
				uint8_t
#endif
					bCount2 = std::min(g_bItemCountLimit - item->GetCount(), bCount);
				bCount -= bCount2;

				item->SetCount(item->GetCount() + bCount2);

				if (bCount == 0)
				{
#ifdef TEXTS_IMPROVEMENT
					if (bMsg) {
						ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
							CHAT_TYPE_INFO_ITEM
#else
							CHAT_TYPE_INFO
#endif
							, 102, "%d#%s", bCount2, item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
						//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 09 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

					}
#endif

					return item;
				}
			}
		}
	}

	LPITEM item = ITEM_MANAGER::instance().CreateItem(dwItemVnum, bCount, 0, true);

	if (!item)
	{
		sys_err("cannot create item by vnum %u (name: %s)", dwItemVnum, GetName());
		return nullptr;
	}

	if (item->GetType() == ITEM_BLEND)
	{
		for (int i = 0; i < INVENTORY_MAX_NUM; i++)
		{
			LPITEM inv_item = GetInventoryItem(i);

			if (inv_item == nullptr) continue;

			if (inv_item->GetType() == ITEM_BLEND)
			{
				if (inv_item->GetVnum() == item->GetVnum())
				{
					if (inv_item->GetSocket(0) == item->GetSocket(0) &&
						inv_item->GetSocket(1) == item->GetSocket(1) &&
						inv_item->GetSocket(2) == item->GetSocket(2) &&
						inv_item->GetCount() < g_bItemCountLimit)
					{
						inv_item->SetCount(inv_item->GetCount() + item->GetCount());
						return inv_item;
					}
				}
			}
		}
	}

	int iEmptyCell;
	if (item->IsDragonSoul())
	{
		iEmptyCell = GetEmptyDragonSoulInventory(item);
	}
#ifdef ENABLE_EXTRA_INVENTORY
	else if (item->IsExtraItem())
		iEmptyCell = GetEmptyExtraInventory(item);
#endif
	else
		iEmptyCell = GetEmptyInventory(item->GetSize());

	if (iEmptyCell != -1)
	{
#ifdef TEXTS_IMPROVEMENT
		if (bMsg) {
			ChatPacketNew(
#ifdef ENABLE_NEW_CHAT
				CHAT_TYPE_INFO_ITEM
#else
				CHAT_TYPE_INFO
#endif
				, 102, "%d#%s", bCount, item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
			//ChatPacket(CHAT_TYPE_INFO, "|cffffc700[10:]|r 10 ");

		}
#endif

		if (item->IsDragonSoul())
			item->AddToCharacter(this, TItemPos(DRAGON_SOUL_INVENTORY, iEmptyCell)
#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);
#ifdef ENABLE_EXTRA_INVENTORY
		else if (item->IsExtraItem())
			item->AddToCharacter(this, TItemPos(EXTRA_INVENTORY, iEmptyCell)

#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);
#endif
		else
			item->AddToCharacter(this, TItemPos(INVENTORY, iEmptyCell)
#ifdef __HIGHLIGHT_SYSTEM__
				, isHighLight
#endif
			);
		LogManager::instance().ItemLog(this, item, "SYSTEM", item->GetName());

		if (item->GetType() == ITEM_USE && item->GetSubType() == USE_POTION)
		{
			TQuickslot* pSlot;

			if (GetQuickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE)
			{
				TQuickslot slot;
				slot.type = QUICKSLOT_TYPE_ITEM;
				slot.pos = iEmptyCell;
				SetQuickslot(0, slot);
			}
		}
	}
	else
	{
		item->AddToGround(GetMapIndex(), GetXYZ());
#ifdef ENABLE_NEWSTUFF
		item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
		item->StartDestroyEvent();
#endif
		// ¾ÈÆ¼ µå¶ø flag°¡ °É·ÁÀÖ´Â ¾ÆÀÌÅÛÀÇ °æ¿ì,
		// ÀÎº¥¿¡ ºó °ø°£ÀÌ ¾ø¾î¼­ ¾îÂ¿ ¼ö ¾øÀÌ ¶³¾îÆ®¸®°Ô µÇ¸é,
		// ownershipÀ» ¾ÆÀÌÅÛÀÌ »ç¶óÁú ¶§±îÁö(300ÃÊ) À¯ÁöÇÑ´Ù.
		if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_DROP))
			item->SetOwnership(this, 300);
		else
			item->SetOwnership(this, 60);
		LogManager::instance().ItemLog(this, item, "SYSTEM_DROP", item->GetName());
	}

	sys_log(0,
		"7: %d %d", dwItemVnum, bCount);
	return item;
}

bool CHARACTER::GiveItem(LPCHARACTER victim, TItemPos Cell)
{
	if (!CanHandleItem())
		return false;

	// @fixme150 BEGIN
	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 740, "");
#endif
		return false;
	}
	// @fixme150 END

	LPITEM item = GetItem(Cell);

	if (item && !item->IsExchanging())
	{
		if (victim->CanReceiveItem(this, item))
		{
			victim->ReceiveItem(this, item);
			return true;
		}
	}

	return false;
}

bool CHARACTER::CanReceiveItem(LPCHARACTER from, LPITEM item) const
{
	if (IsPC())
		return false;

	// TOO_LONG_DISTANCE_EXCHANGE_BUG_FIX
	if (DISTANCE_APPROX(GetX() - from->GetX(), GetY() - from->GetY()) > 2000)
		return false;
	// END_OF_TOO_LONG_DISTANCE_EXCHANGE_BUG_FIX

	uint32_t racenum = GetRaceNum();

	if (racenum == DEVILTOWER_BLACKSMITH_WEAPON_MOB ||
		racenum == DEVILTOWER_BLACKSMITH_ARMOR_MOB ||
		racenum == DEVILTOWER_BLACKSMITH_ACCESSORY_MOB) {
		bool bCanProced = true;

		for (uint8_t i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
			if (item->GetLimitType(i) == LIMIT_LEVEL && item->GetLimitValue(i) >= 90) {
				bCanProced = false;
				break;
			}
		}

		if (!bCanProced) {
#ifdef TEXTS_IMPROVEMENT
			from->ChatPacketNew(CHAT_TYPE_INFO, 1360, "");
#endif
			return false;
		}
	}

	switch (racenum)
	{
	case fishing::CAMPFIRE_MOB:
		if (item->GetType() == ITEM_FISH &&
			(item->GetSubType() == FISH_ALIVE || item->GetSubType() == FISH_DEAD))
			return true;
		break;

	case fishing::FISHER_MOB:
		if (item->GetType() == ITEM_ROD)
			return true;
		break;

	case BLACKSMITH_WEAPON_MOB:
	case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
		if (item->GetType() == ITEM_WEAPON && item->GetRefinedVnum()) {
			return true;
		}
		else {
			return false;
		}
		break;
	case BLACKSMITH_ARMOR_MOB:
	case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
		if ((item->GetType() == ITEM_BELT || (item->GetType() == ITEM_ARMOR && (item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_SHIELD || item->GetSubType() == ARMOR_HEAD))) && item->GetRefinedVnum()) {
			return true;
		}
		else {
			return false;
		}
		break;
	case BLACKSMITH_ACCESSORY_MOB:
	case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
		if (item->GetType() == ITEM_ARMOR && !(item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_SHIELD || item->GetSubType() == ARMOR_HEAD
#ifdef ENABLE_PENDANT
			|| item->GetSubType() == ARMOR_PENDANT
#endif
			) && item->GetRefinedVnum()) {
			return true;
		}
		else {
			return false;
		}
		break;
	case BLACKSMITH_MOB:
	case BLACKSMITH2_MOB:
		if (item->GetRefinedVnum() && item->GetRefineSet()) {
			return true;
		}
		else {
			return false;
		}
	case ALCHEMIST_MOB:
		if (item->GetRefinedVnum())
			return true;
		break;

	case 20101:
	case 20102:
	case 20103:
		// ÃÊ±Þ ¸»
		if (item->GetVnum() == ITEM_REVIVE_HORSE_1)
		{
			if (!IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_1)
		{
			if (IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 466, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_2 || item->GetVnum() == ITEM_HORSE_FOOD_3)
		{
			return false;
		}
		break;
	case 20104:
	case 20105:
	case 20106:
		// Áß±Þ ¸»
		if (item->GetVnum() == ITEM_REVIVE_HORSE_2)
		{
			if (!IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_2)
		{
			if (IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 466, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_1 || item->GetVnum() == ITEM_HORSE_FOOD_3)
		{
			return false;
		}
		break;
	case 20107:
	case 20108:
	case 20109:
		// °í±Þ ¸»
		if (item->GetVnum() == ITEM_REVIVE_HORSE_3)
		{
			if (!IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_3)
		{
			if (IsDead())
			{
#ifdef TEXTS_IMPROVEMENT
				from->ChatPacketNew(CHAT_TYPE_INFO, 466, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_1 || item->GetVnum() == ITEM_HORSE_FOOD_2)
		{
			return false;
		}
		break;
	}

	//if (IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_GIVE))
	{
		return true;
	}

	return false;
}

void CHARACTER::ReceiveItem(LPCHARACTER from, LPITEM item)
{
	if (IsPC())
		return;
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
	// Rune Dungeon: key pedestal (20507) consumes 89103 and progresses floor 5
	if (CRuneDungeon::instance().OnNpcTakeItem(from, this, item))
		return;
	if (CHalloween2022Dungeon::instance().OnNpcTakeItem(from, this, item))
		return;
	if (CVikingDungeon::instance().OnNpcTakeItem(from, this, item))
		return;
	// LostCastle Dungeon: statue/totem item usage
	//if (CLostCastleDungeon::instance().OnNpcTakeItem(from, this, item))
	//	return;
#endif
	switch (GetRaceNum())
	{
	case fishing::CAMPFIRE_MOB:
		if (item->GetType() == ITEM_FISH && (item->GetSubType() == FISH_ALIVE || item->GetSubType() == FISH_DEAD))
			fishing::Grill(from, item);
		else
		{
			// TAKE_ITEM_BUG_FIX
			from->SetQuestNPCID(GetPacketVID());
			// END_OF_TAKE_ITEM_BUG_FIX
			quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
		}
		break;

		// DEVILTOWER_NPC
	case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
	case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
	case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB: {
		int set = item->GetRefineSet();
		if (item->GetRefinedVnum() != 0 && set != 0 /*&& item->GetRefineSet() < 500*/
#ifdef ENABLE_ITEM_EXTRA_PROTO
			&& set != 1021
			&& set != 1022
			&& set != 1023
			&& set != 1024
			&& set != 19
			&& set != 20
			&& set != 21
			&& set != 22
			&& set != 28
			&& set != 29
			&& set != 30
			&& set != 31
			&& set != 32
			&& set != 396
			&& set != 397
			&& set != 398
			&& set != 399
			&& set != 640
			&& set != 641
			&& set != 642
			&& set != 643
			&& set != 370
			&& set != 371
			&& set != 372
			&& set != 373
			&& set != 461
			&& set != 462
			&& set != 463
			&& set != 464
			&& set != 474
			&& set != 475
			&& set != 476
			&& set != 477
			&& set != 487
			&& set != 488
			&& set != 489
			&& set != 490
			&& set != 235
			&& set != 236
			&& set != 237
			&& set != 238
			&& set != 383
			&& set != 384
			&& set != 385
			&& set != 386
			&& set != 769
			&& set != 770
			&& set != 771
			&& set != 772
			&& set != 995
			&& set != 996
			&& set != 997
			&& set != 998
			&& set != 1017
			&& set != 1018
			&& set != 1019
			&& set != 1020
			&& set != 448
			&& set != 449
			&& set != 450
			&& set != 451
			&& set != 430
			&& set != 431
			&& set != 432
			&& set != 433
			&& set != 325
			&& set != 326
			&& set != 327
			&& set != 328
#endif
			)
		{
			from->SetRefineNPC(this);
			from->RefineInformation(item->GetCell(), REFINE_TYPE_MONEY_ONLY);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			from->ChatPacketNew(CHAT_TYPE_INFO, 427, "");
		}
#endif
		break;
	}
											// END_OF_DEVILTOWER_NPC

	case BLACKSMITH_MOB:
	case BLACKSMITH2_MOB:
	case BLACKSMITH_WEAPON_MOB:
	case BLACKSMITH_ARMOR_MOB:
	case BLACKSMITH_ACCESSORY_MOB:
		if (item->GetRefinedVnum())
		{
			from->SetRefineNPC(this);
			from->RefineInformation(item->GetCell(), REFINE_TYPE_NORMAL);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			from->ChatPacketNew(CHAT_TYPE_INFO, 427, "");
		}
#endif
		break;
	case 20101:
	case 20102:
	case 20103:
	case 20104:
	case 20105:
	case 20106:
	case 20107:
	case 20108:
	case 20109:
		if (item->GetVnum() == ITEM_REVIVE_HORSE_1 ||
			item->GetVnum() == ITEM_REVIVE_HORSE_2 ||
			item->GetVnum() == ITEM_REVIVE_HORSE_3)
		{
			from->ReviveHorse();
			item->SetCount(item->GetCount() - 1);
#ifdef TEXTS_IMPROVEMENT
			from->ChatPacketNew(CHAT_TYPE_INFO, 329, "%s", item->GetName());
#endif
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_1 ||
			item->GetVnum() == ITEM_HORSE_FOOD_2 ||
			item->GetVnum() == ITEM_HORSE_FOOD_3)
		{
			from->FeedHorse();
#ifdef TEXTS_IMPROVEMENT
			from->ChatPacketNew(CHAT_TYPE_INFO, 112, "%s", item->GetName());
#endif
			item->SetCount(item->GetCount() - 1);
			EffectPacket(SE_HPUP_RED);
		}
		break;

	default:
		sys_log(0, "TakeItem %s %d %s", from->GetName(), GetRaceNum(), item->GetName());
		from->SetQuestNPCID(GetPacketVID());
		quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
		break;
	}
}

bool CHARACTER::GiveItemFromSpecialItemGroup(uint32_t dwGroupNum, std::vector<uint32_t> &dwItemVnums,
	std::vector<uint32_t> &dwItemCounts, std::vector <LPITEM> &item_gets, int& count)
{
	const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(dwGroupNum);

	if (!pGroup)
	{
		sys_err("cannot find special item group %d", dwGroupNum);
		return false;
	}

	std::vector <int> idxes;
	int n = pGroup->GetMultiIndex(idxes);

	bool bSuccess;

	for (int i = 0; i < n; i++)
	{
		bSuccess = false;
		int idx = idxes[i];
		uint32_t dwVnum = pGroup->GetVnum(idx);
		uint32_t dwCount = pGroup->GetCount(idx);
		int	iRarePct = pGroup->GetRarePct(idx);
		LPITEM item_get = nullptr;
		switch (dwVnum)
		{
		case CSpecialItemGroup::GOLD:
			PointChange(POINT_GOLD, dwCount);
			LogManager::instance().CharLog(this, dwCount, "TREASURE_GOLD", "");

			bSuccess = true;
			break;
		case CSpecialItemGroup::EXP:
		{
			PointChange(POINT_EXP, dwCount);
			LogManager::instance().CharLog(this, dwCount, "TREASURE_EXP", "");

			bSuccess = true;
		}
		break;

		case CSpecialItemGroup::MOB:
		{
			sys_log(0, "CSpecialItemGroup::MOB %d", dwCount);
			int x = GetX() + number(-500, 500);
			int y = GetY() + number(-500, 500);

			auto* ch = CHARACTER_MANAGER::instance().SpawnMob(dwCount, GetMapIndex(), x, y, 0, true, -1);
			if (ch)
				ch->SetAggressive();
			bSuccess = true;
		}
		break;
		case CSpecialItemGroup::SLOW:
		{
			sys_log(0, "CSpecialItemGroup::SLOW %d", -(int)dwCount);
			AddAffect(AFFECT_SLOW, POINT_MOV_SPEED, -(int)dwCount, AFF_SLOW, 300, 0, true);
			bSuccess = true;
		}
		break;
		case CSpecialItemGroup::DRAIN_HP:
		{
			int64_t iDropHP = GetMaxHP() * dwCount / 100;
			sys_log(0, "CSpecialItemGroup::DRAIN_HP %d", -iDropHP);
			iDropHP = std::min(iDropHP, GetHP() - 1);
			sys_log(0, "CSpecialItemGroup::DRAIN_HP %d", -iDropHP);
			PointChange(POINT_HP, -iDropHP);
			bSuccess = true;
		}
		break;
		case CSpecialItemGroup::POISON:
		{
			AttackedByPoison(nullptr);
			bSuccess = true;
		}
		break;
#ifdef ENABLE_WOLFMAN_CHARACTER
		case CSpecialItemGroup::BLEEDING:
		{
			AttackedByBleeding(NULL);
			bSuccess = true;
		}
		break;
#endif
		case CSpecialItemGroup::MOB_GROUP:
		{
			int sx = GetX() - number(300, 500);
			int sy = GetY() - number(300, 500);
			int ex = GetX() + number(300, 500);
			int ey = GetY() + number(300, 500);
			CHARACTER_MANAGER::instance().SpawnGroup(dwCount, GetMapIndex(), sx, sy, ex, ey, nullptr, true);

			bSuccess = true;
		}
		break;
		default:
		{
			item_get = AutoGiveItem(dwVnum, dwCount, iRarePct);

			if (item_get)
			{
				bSuccess = true;
			}
		}
		break;
		}

		if (bSuccess)
		{
			dwItemVnums.push_back(dwVnum);
			dwItemCounts.push_back(dwCount);
			item_gets.push_back(item_get);
			count++;

		}
		else
		{
			return false;
		}
	}
	return bSuccess;
}

bool CHARACTER::DestroyItem(TItemPos Cell)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::bool CHARACTER::DestroyItem(TItemPos Cell),");//INGAME_DEBUG_RAZOR93
#endif
	LPITEM item = nullptr;
	if (!CanHandleItem()) {
#ifdef TEXTS_IMPROVEMENT
		if (nullptr != DragonSoul_RefineWindow_GetOpener()) {
			ChatPacketNew(CHAT_TYPE_INFO, 232, "");
		}
#endif

		return false;
	}

	if (IsDead())
		return false;

	if (!IsValidItemPosition(Cell) || !(item = GetItem(Cell)))
		return false;

	if (item->IsEquipped())
		return false;

	if (item->IsExchanging())
		return false;

	if (true == item->isLocked())
		return false;

	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
		return false;

	if ((item->GetVnum() >= 55701) && (item->GetVnum() <= 55711)) {
		if (item->GetSocket(0) != 0)
			return false;
	}

#ifdef ENABLE_EXTRA_INVENTORY
	if (item->IsExtraItem()) {
		SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, Cell.cell, 255);
	}
	else {
		SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 255);
	}
#else
	SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 255);
#endif

#ifdef ENABLE_BATTLE_PASS
	uint8_t bBattlePassId = GetBattlePassId();
	if (bBattlePassId)
	{
		uint32_t dwItemVnum, dwCnt;
		if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, DESTROY_ITEM, &dwItemVnum, &dwCnt))
		{
			if (dwItemVnum == item->GetVnum() && GetMissionProgress(DESTROY_ITEM, bBattlePassId) < dwCnt)
				UpdateMissionProgress(DESTROY_ITEM, bBattlePassId, item->GetCount(), dwCnt);
		}
	}
#endif

#ifdef TEXTS_IMPROVEMENT
	ChatPacketNew(CHAT_TYPE_INFO, 47, "%s", item->GetName());
#endif
	ITEM_MANAGER::instance().RemoveItem(item, "DESTROY");
	return true;
}

const char CHARACTER::msc_szLastChangeItemAttrFlag[] = "Item.LastChangeItemAttr";
// const char CHARACTER::msc_szChangeItemAttrCycleFlag[] = "change_itemattr_cycle";
// END_OF_CHANGE_ITEM_ATTRIBUTES

const uint8_t g_aBuffOnAttrPoints[] = { POINT_ENERGY, POINT_COSTUME_ATTR_BONUS };

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

using LegacyCharHandle = decltype(std::declval<ecs::LegacyCharPtr>().ptr);

struct FFindStone
{
	std::map<uint32_t, LegacyCharHandle> m_mapStone;

	void operator()(LPENTITY pEnt)
	{
		if (pEnt->IsType(ENTITY_CHARACTER) == true)
		{
			auto* pChar = static_cast<LegacyCharHandle>(pEnt);

			if (pChar->IsStone() == true)
			{
				m_mapStone[pChar->GetPacketVID()] = pChar;
			}
		}
	}
};


//±ÍÈ¯ºÎ, ±ÍÈ¯±â¾ïºÎ, °áÈ¥¹ÝÁö
static bool IS_SUMMON_ITEM(int vnum)
{
	switch (vnum)
	{
	case 22000:
	case 22010:
	case 22011:
	case 22020:
	case ITEM_MARRIAGE_RING:
		return true;
	}

	return false;
}

bool IS_SUMMONABLE_ZONE(int map_index)
{
	switch (map_index)
	{
	case 66: // »ç±ÍÅ¸¿ö
	case 71: // °Å¹Ì ´øÀü 2Ãþ
	case 72: // ÃµÀÇ µ¿±¼
	case 73: // ÃµÀÇ µ¿±¼ 2Ãþ
	case 193: // °Å¹Ì ´øÀü 2-1Ãþ
#if 0
	case 184: // ÃµÀÇ µ¿±¼(½Å¼ö)
	case 185: // ÃµÀÇ µ¿±¼ 2Ãþ(½Å¼ö)
	case 186: // ÃµÀÇ µ¿±¼(ÃµÁ¶)
	case 187: // ÃµÀÇ µ¿±¼ 2Ãþ(ÃµÁ¶)
	case 188: // ÃµÀÇ µ¿±¼(Áø³ë)
	case 189: // ÃµÀÇ µ¿±¼ 2Ãþ(Áø³ë)
#endif
		//		case 206 : // ¾Æ±Íµ¿±¼
	case 216: // ¾Æ±Íµ¿±¼
	case 217: // °Å¹Ì ´øÀü 3Ãþ
	case 208: // ÃµÀÇ µ¿±¼ (¿ë¹æ)

	case 113: // OX Event ¸Ê
		return false;
	}

	if (CBattleArena::IsBattleArenaMap(map_index)) return false;

	// ¸ðµç private ¸ÊÀ¸·Ð ¿öÇÁ ºÒ°¡´É
	if (map_index > 10000) return false;

	return true;
}

bool IS_BOTARYABLE_ZONE(int nMapIndex)
{
	if (!g_bEnableBootaryCheck) return true;

	switch (nMapIndex)
	{
	case 1:
	case 3:
	case 21:
	case 23:
	case 41:
	case 43:
		return true;
	}

	return false;
}

// item socket ÀÌ ÇÁ·ÎÅäÅ¸ÀÔ°ú °°ÀºÁö Ã¼Å© -- by mhh
static bool FN_check_item_sex(LegacyCharHandle ch, LPITEM item)
{

#ifdef ENABLE_SORT_INVEN
	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_AFFECT)
		return true;
#endif

	// ³²ÀÚ ±ÝÁö
	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MALE))
	{
		if (SEX_MALE == GET_SEX(ch))
			return false;
	}
	// ¿©ÀÚ±ÝÁö
	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_FEMALE))
	{
		if (SEX_FEMALE == GET_SEX(ch))
			return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////
// ITEM HANDLING
/////////////////////////////////////////////////////////////////////////////

bool CHARACTER::CanHandleItem(bool bSkipCheckRefine, bool bSkipObserver)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::bool CHARACTER::CanHandleItem");//INGAME_DEBUG_RAZOR93
	sys_log(0, "Razor93 LOG:: bool CHARACTER::CanHandleItem");
#endif
	if (!bSkipObserver)
		if (m_bIsObserver)
			return false;

	if (GetMyShop())
		return false;

	if (!bSkipCheckRefine)
		if (m_bUnderRefine)
			return false;

	if (IsCubeOpen() || nullptr != DragonSoul_RefineWindow_GetOpener())
		return false;

#ifdef __ATTR_TRANSFER_SYSTEM__
	if (IsAttrTransferOpen())
		return false;
#endif

	if (IsWarping())
		return false;

#ifdef ENABLE_ACCE_SYSTEM
	if (IsAcceOpen())
		return false;
#endif

	return true;
}

#ifdef ENABLE_EXTRA_INVENTORY
#endif

#ifdef __HIGHLIGHT_SYSTEM__
void CHARACTER::SetItem(TItemPos Cell, LPITEM pItem, bool isHighLight)
#else
void CHARACTER::SetItem(TItemPos Cell, LPITEM pItem)
#endif
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::RequestLanguage ");//INGAME_DEBUG_RAZOR93
#endif
	uint16_t wCell = Cell.cell;
	uint8_t window_type = Cell.window_type;
	if ((unsigned long)((CItem*)pItem) == 0xff || (unsigned long)((CItem*)pItem) == 0xffffffff)
	{
		sys_err("!!! FATAL ERROR !!! item == 0xff (char: %s cell: %u)", GetName(), wCell);
		core_dump();
		return;
	}

	if (pItem && pItem->GetOwner())
	{
		assert(!"GetOwner exist");
		return;
	}
	// ±âº» ÀÎº¥Åä¸®
	switch (window_type)
	{
	case INVENTORY:
	case EQUIPMENT:
	{
		if (wCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
		{
			sys_err("CHARACTER::SetItem: invalid item cell %d", wCell);
			return;
		}

		LPITEM pOld = m_pointsInstant.pItems[wCell];

		if (pOld)
		{
			if (wCell < INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pOld->GetSize(); ++i)
				{
					int p = wCell + (i * 5);

					if (p >= INVENTORY_MAX_NUM)
						continue;

					if (m_pointsInstant.pItems[p] && m_pointsInstant.pItems[p] != pOld)
						continue;

					m_pointsInstant.bItemGrid[p] = 0;
				}
			}
			else
				m_pointsInstant.bItemGrid[wCell] = 0;
		}

		if (pItem)
		{
			if (wCell < INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pItem->GetSize(); ++i)
				{
					int p = wCell + (i * 5);

					if (p >= INVENTORY_MAX_NUM)
						continue;

					// wCell + 1 ·Î ÇÏ´Â °ÍÀº ºó°÷À» Ã¼Å©ÇÒ ¶§ °°Àº
					// ¾ÆÀÌÅÛÀº ¿¹¿ÜÃ³¸®ÇÏ±â À§ÇÔ
					m_pointsInstant.bItemGrid[p] = wCell + 1;
				}
			}
			else
				m_pointsInstant.bItemGrid[wCell] = wCell + 1;
		}

		m_pointsInstant.pItems[wCell] = pItem;
	}
	break;
	// ¿ëÈ¥¼® ÀÎº¥Åä¸®
	case DRAGON_SOUL_INVENTORY:
	{
		LPITEM pOld = m_pointsInstant.pDSItems[wCell];

		if (pOld)
		{
			if (wCell < DRAGON_SOUL_INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pOld->GetSize(); ++i)
				{
					int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

					if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
						continue;

					if (m_pointsInstant.pDSItems[p] && m_pointsInstant.pDSItems[p] != pOld)
						continue;

					m_pointsInstant.wDSItemGrid[p] = 0;
				}
			}
			else
				m_pointsInstant.wDSItemGrid[wCell] = 0;
		}

		if (pItem)
		{
			if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
			{
				sys_err("CHARACTER::SetItem: invalid DS item cell %d", wCell);
				return;
			}

			if (wCell < DRAGON_SOUL_INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pItem->GetSize(); ++i)
				{
					int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

					if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
						continue;

					// wCell + 1 ·Î ÇÏ´Â °ÍÀº ºó°÷À» Ã¼Å©ÇÒ ¶§ °°Àº
					// ¾ÆÀÌÅÛÀº ¿¹¿ÜÃ³¸®ÇÏ±â À§ÇÔ
					m_pointsInstant.wDSItemGrid[p] = wCell + 1;
				}
			}
			else
				m_pointsInstant.wDSItemGrid[wCell] = wCell + 1;
		}

		m_pointsInstant.pDSItems[wCell] = pItem;
	}
	break;



#ifdef ENABLE_EXTRA_INVENTORY
	case EXTRA_INVENTORY:
	{
		if (wCell >= EXTRA_INVENTORY_MAX_NUM)
		{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::if (wCell >= EXTRA_INVENTORY_MAX_NUM)");//INGAME_DEBUG_RAZOR93
#endif
			sys_err("CHARACTER::SetItem: invalid EXTRA item cell %d", wCell);
			return;
		}

		LPITEM pOld = m_pointsInstant.pExtraItems[wCell];

		if (pOld)
		{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::if (pOld)");//INGAME_DEBUG_RAZOR93
#endif

			if (wCell < EXTRA_INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pOld->GetSize(); ++i)
				{
					int p = wCell + (i * EXTRA_INVENTORY_PAGE_COLUMN);

					if (p >= EXTRA_INVENTORY_MAX_NUM)
						continue;

					if (m_pointsInstant.pExtraItems[p] && m_pointsInstant.pExtraItems[p] != pOld)
						continue;

					m_pointsInstant.wExtraItemGrid[p] = 0;
				}
			}
			else
				m_pointsInstant.wExtraItemGrid[wCell] = 0;
		}

		if (pItem)
		{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::if (pItem)");//INGAME_DEBUG_RAZOR93
#endif
			if (wCell < EXTRA_INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < pItem->GetSize(); ++i)
				{
					int p = wCell + (i * EXTRA_INVENTORY_PAGE_COLUMN);

					if (p >= EXTRA_INVENTORY_MAX_NUM)
						continue;

					m_pointsInstant.wExtraItemGrid[p] = wCell + 1;
				}
			}
			else
				m_pointsInstant.wExtraItemGrid[wCell] = wCell + 1;
		}

		m_pointsInstant.pExtraItems[wCell] = pItem;
	}
	break;
#endif

#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
	{
		LPITEM pOld = m_pointsInstant.pSwitchbotItems[wCell];
		if (pItem && pOld)
		{
			return;
		}

		if (wCell >= SWITCHBOT_SLOT_COUNT)
		{
			sys_err("CHARACTER::SetItem: invalid switchbot item cell %d", wCell);
			return;
		}

		if (pItem)
		{
			CSwitchbotManager::Instance().RegisterItem(GetPlayerID(), pItem->GetID(), wCell);
		}
		else
		{
			CSwitchbotManager::Instance().UnregisterItem(GetPlayerID(), wCell);
		}

		m_pointsInstant.pSwitchbotItems[wCell] = pItem;
	}
	break;
#endif
	default:
		sys_err("Invalid Inventory type %d", window_type);
		return;
	}

	if (GetDesc())
	{
		// È®Àå ¾ÆÀÌÅÛ: ¼­¹ö¿¡¼­ ¾ÆÀÌÅÛ ÇÃ·¡±× Á¤º¸¸¦ º¸³½´Ù
		if (pItem)
		{
			TPacketGCItemSet pack;
			pack.header = HEADER_GC_ITEM_SET;
			pack.Cell = Cell;

			pack.count = pItem->GetCount();
#ifdef ATTR_LOCK
			pack.lockedattr = pItem->GetLockedAttr();
#endif
			pack.vnum = pItem->GetVnum();
			pack.flags = pItem->GetFlag();
			pack.anti_flags = pItem->GetAntiFlag();
#ifdef __HIGHLIGHT_SYSTEM__
			pack.highlight = isHighLight;
#else
			pack.highlight = (Cell.window_type == DRAGON_SOUL_INVENTORY);
#endif

			memcpy(pack.alSockets, pItem->GetSockets(), sizeof(pack.alSockets));
			memcpy(pack.aAttr, pItem->GetAttributes(), sizeof(pack.aAttr));

			GetDesc()->Packet(&pack, sizeof(TPacketGCItemSet));
		}
		else
		{
			TPacketGCItemDelDeprecated pack;
			pack.header = HEADER_GC_ITEM_DEL;
			pack.Cell = Cell;
			pack.count = 0;
#ifdef ATTR_LOCK
			pack.lockedattr = -1;
#endif
			pack.vnum = 0;
			memset(pack.alSockets, 0, sizeof(pack.alSockets));
			memset(pack.aAttr, 0, sizeof(pack.aAttr));

			GetDesc()->Packet(&pack, sizeof(TPacketGCItemDelDeprecated));
		}
	}

	if (pItem)
	{
		pItem->SetCell(this, wCell);
		switch (window_type)
		{
		case INVENTORY:
		case EQUIPMENT:
			if (wCell >= BELT_INVENTORY_SLOT_START && wCell < BELT_INVENTORY_SLOT_END)
			{
				if (CBeltInventoryHelper::CanMoveIntoBeltInventory(pItem))
					pItem->SetWindow(INVENTORY);
				else
					pItem->SetWindow(EQUIPMENT); // vagy return is lehet, ha nem engedelyezett
			}
			else if (wCell < INVENTORY_MAX_NUM)
			{
				pItem->SetWindow(INVENTORY);
			}
			else
			{
				pItem->SetWindow(EQUIPMENT);
			}

			break;
		case DRAGON_SOUL_INVENTORY:
			pItem->SetWindow(DRAGON_SOUL_INVENTORY);
			break;
#ifdef ENABLE_EXTRA_INVENTORY
		case EXTRA_INVENTORY:
			pItem->SetWindow(EXTRA_INVENTORY);
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			sys_log(0, "Razor93 LOG:: Called: Char_item.cpp line :653: case switch :pItem->SetWindow(EXTRA_INVENTORY);");
#endif
			break;
#endif
#ifdef ENABLE_SWITCHBOT
		case SWITCHBOT:
			pItem->SetWindow(SWITCHBOT);
			break;
#endif
		}
	}
}

void CHARACTER::ClearItem()
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp:: void CHARACTER::ClearItem ");//INGAME_DEBUG_RAZOR93
#endif
	int		i;
	LPITEM	item;

	for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
	{
		if ((item = GetInventoryItem(i)))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);

			SyncQuickslot(QUICKSLOT_TYPE_ITEM, i, 255);
		}
	}
	for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
	{
		if ((item = GetItem(TItemPos(DRAGON_SOUL_INVENTORY, i))))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);
		}
	}

#ifdef ENABLE_EXTRA_INVENTORY
	for (i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
	{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
		sys_log(0, "Razor93 LOG:: Called: Char_item.cpp line :739: for (i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)");
#endif
		if ((item = GetExtraInventoryItem(i)))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);

			SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, i, 255);
		}
	}
#endif

#ifdef ENABLE_SWITCHBOT
	for (i = 0; i < SWITCHBOT_SLOT_COUNT; ++i)
	{
		if ((item = GetItem(TItemPos(SWITCHBOT, i))))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);
		}
	}
#endif
}


bool CHARACTER::IsEmptyItemGrid(TItemPos Cell, uint8_t bSize, int iExceptionCell) const
{


#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	switch (Cell.window_type)
	{
	case INVENTORY:
	{
		int bCell = Cell.cell;

		// bItemCell? 0? false?? ???? ?? + 1 ?? ????.
		// ??? iExceptionCell? 1? ?? ????.
		++iExceptionCell;

		/* 			if (Cell.IsBeltInventoryPosition())
					{
						LPITEM beltItem = GetWear(WEAR_BELT);

						if (NULL == beltItem)
							return false;

						if (false == CBeltInventoryHelper::IsAvailableCell(bCell - BELT_INVENTORY_SLOT_START, beltItem->GetValue(0)))
							return false;

						if (m_pointsInstant.bItemGrid[bCell])
						{
							if (m_pointsInstant.bItemGrid[bCell] == iExceptionCell)
								return true;

							return false;
						}

						if (bSize == 1)
							return true;

					} */
		if (Cell.IsBeltInventoryPosition())
		{
			// NE nezd meg, hogy van-e felszerelve ov
			// NE ellen?rizd az ov tipusat
			// --> mindig engedelyezett

			if (m_pointsInstant.bItemGrid[bCell])
			{
				if (m_pointsInstant.bItemGrid[bCell] == iExceptionCell)
					return true;

				return false;
			}

			if (bSize == 1)
				return true;
		}

		//black
		else if (bCell >= Inventory_Size())
			return false;

		if (m_pointsInstant.bItemGrid[bCell])
		{
			if (m_pointsInstant.bItemGrid[bCell] == iExceptionCell)
			{
				if (bSize == 1)
					return true;

				int j = 1;
				uint8_t bPage = bCell / (INVENTORY_MAX_NUM / 4);
				do
				{
					uint8_t p = bCell + (5 * j);

					if (p >= Inventory_Size())
						return false;

					if (p / (INVENTORY_MAX_NUM / 4) != bPage)
						return false;

					if (m_pointsInstant.bItemGrid[p])
						if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
							return false;
				} while (++j < bSize);

				return true;
			}
			else
				return false;
		}

		// ??? 1?? ??? ???? ???? ?? ??
		if (1 == bSize)
			return true;
		else
		{
			int j = 1;
			uint8_t bPage = bCell / (INVENTORY_MAX_NUM / 4);

			do
			{
				uint8_t p = bCell + (5 * j);

				if (p >= Inventory_Size())
					return false;
				if (p / (INVENTORY_MAX_NUM / 4) != bPage)
					return false;

				if (m_pointsInstant.bItemGrid[p])
					if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
						return false;
			} while (++j < bSize);

			return true;
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	break;
	case EXTRA_INVENTORY:
	{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
		sys_log(0, "Razor93 LOG:: Called: Char_item.cpp line :894 /case switch/ : case EXTRA_INVENTORY:");
#endif
		uint16_t bCell = Cell.cell;

		if (bCell > ExtraInventoryMaxSlots(bCell, true))
			return false;

		++iExceptionCell;

		if (m_pointsInstant.wExtraItemGrid[bCell])
		{
			if (m_pointsInstant.wExtraItemGrid[bCell] == iExceptionCell)
			{
				if (bSize == 1)
					return true;

				int j = 1;
				uint8_t bPage = bCell / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT);

				do
				{
					int p = bCell + (5 * j);

					if (p > ExtraInventoryMaxSlots(bCell, true))
						return false;

					if (p / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT) != bPage)
						return false;

					if (m_pointsInstant.wExtraItemGrid[p])
						if (m_pointsInstant.wExtraItemGrid[p] != iExceptionCell)
							return false;
				} while (++j < bSize);

				return true;
			}
			else
				return false;
		}

		if (1 == bSize)
			return true;
		else
		{
			int j = 1;
			uint8_t bPage = bCell / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT);

			do
			{
				int p = bCell + (5 * j);

				if (p > ExtraInventoryMaxSlots(bCell, true))
					return false;

				if (p / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT) != bPage)
					return false;

				if (m_pointsInstant.wExtraItemGrid[p])
					if (m_pointsInstant.wExtraItemGrid[p] != iExceptionCell)
						return false;
			} while (++j < bSize);

			return true;
		}
	}
#endif
	break;
#else
	switch (Cell.window_type)
	{
	case INVENTORY:
	{
		uint8_t bCell = Cell.cell;

		// bItemCell? 0? false?? ???? ?? + 1 ?? ????.
		// ??? iExceptionCell? 1? ?? ????.
		++iExceptionCell;

		if (Cell.IsBeltInventoryPosition())
		{
			LPITEM beltItem = GetWear(WEAR_BELT);

			if (NULL == beltItem)
				return false;

			if (false == CBeltInventoryHelper::IsAvailableCell(bCell - BELT_INVENTORY_SLOT_START, beltItem->GetValue(0)))
				return false;

			if (m_pointsInstant.bItemGrid[bCell])
			{
				if (m_pointsInstant.bItemGrid[bCell] == iExceptionCell)
					return true;

				return false;
			}

			if (bSize == 1)
				return true;

		}
		//black
		else if (bCell >= INVENTORY_MAX_NUM)
			return false;

		if (m_pointsInstant.bItemGrid[bCell])
		{
			if (m_pointsInstant.bItemGrid[bCell] == iExceptionCell)
			{
				if (bSize == 1)
					return true;

				int j = 1;
				uint8_t bPage = bCell / (INVENTORY_MAX_NUM / 4);

				do
				{
					uint8_t p = bCell + (5 * j);

					if (p >= INVENTORY_MAX_NUM)
						return false;

					if (p / (INVENTORY_MAX_NUM / 4) != bPage)
						return false;

					if (m_pointsInstant.bItemGrid[p])
						if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
							return false;
				} while (++j < bSize);

				return true;
			}
			else
				return false;
		}

		// ??? 1?? ??? ???? ???? ?? ??
		if (1 == bSize)
			return true;
		else
		{
			int j = 1;
			uint8_t bPage = bCell / (INVENTORY_MAX_NUM / 4);

			do
			{
				uint8_t p = bCell + (5 * j);

				if (p >= INVENTORY_MAX_NUM)
					return false;
				if (p / (INVENTORY_MAX_NUM / 4) != bPage)
					return false;

				if (m_pointsInstant.bItemGrid[p])
					if (m_pointsInstant.bItemGrid[p] != iExceptionCell)
						return false;
			} while (++j < bSize);

			return true;
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	break;
	case EXTRA_INVENTORY:
	{
		uint16_t bCell = Cell.cell;

		if (bCell >= EXTRA_INVENTORY_MAX_NUM)
			return false;

		++iExceptionCell;

		if (m_pointsInstant.wExtraItemGrid[bCell])
		{
			if (m_pointsInstant.wExtraItemGrid[bCell] == iExceptionCell)
			{
				if (bSize == 1)
					return true;

				int j = 1;
				uint8_t bPage = bCell / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT);

				do
				{
					uint8_t p = bCell + (5 * j);

					if (p >= EXTRA_INVENTORY_MAX_NUM)
						return false;

					if (p / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT) != bPage)
						return false;

					if (m_pointsInstant.wExtraItemGrid[p])
						if (m_pointsInstant.wExtraItemGrid[p] != iExceptionCell)
							return false;
				} while (++j < bSize);

				return true;
			}
			else
				return false;
		}

		if (1 == bSize)
			return true;
		else
		{
			int j = 1;
			uint8_t bPage = bCell / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT);

			do
			{
				uint8_t p = bCell + (5 * j);

				if (p >= EXTRA_INVENTORY_MAX_NUM)
					return false;

				if (p / (EXTRA_INVENTORY_MAX_NUM / EXTRA_INVENTORY_PAGE_COUNT) != bPage)
					return false;

				if (m_pointsInstant.wExtraItemGrid[p])
					if (m_pointsInstant.wExtraItemGrid[p] != iExceptionCell)
						return false;
			} while (++j < bSize);

			return true;
		}
	}
#endif
	break;
#endif


#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
	{
		uint16_t wCell = Cell.cell;
		if (wCell >= SWITCHBOT_SLOT_COUNT)
		{
			return false;
		}

		if (m_pointsInstant.pSwitchbotItems[wCell])
		{
			return false;
		}

		return true;
	}
#endif
	case DRAGON_SOUL_INVENTORY:
	{
		uint16_t wCell = Cell.cell;
		if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
			return false;

		// bItemCellÀº 0ÀÌ falseÀÓÀ» ³ªÅ¸³»±â À§ÇØ + 1 ÇØ¼­ Ã³¸®ÇÑ´Ù.
		// µû¶ó¼­ iExceptionCell¿¡ 1À» ´õÇØ ºñ±³ÇÑ´Ù.
		iExceptionCell++;

		if (m_pointsInstant.wDSItemGrid[wCell])
		{
			if (m_pointsInstant.wDSItemGrid[wCell] == iExceptionCell)
			{
				if (bSize == 1)
					return true;

				int j = 1;

				do
				{
					int p = wCell + (DRAGON_SOUL_BOX_COLUMN_NUM * j);

					if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
						return false;

					if (m_pointsInstant.wDSItemGrid[p])
						if (m_pointsInstant.wDSItemGrid[p] != iExceptionCell)
							return false;
				} while (++j < bSize);

				return true;
			}
			else
				return false;
		}

		// Å©±â°¡ 1ÀÌ¸é ÇÑÄ­À» Â÷ÁöÇÏ´Â °ÍÀÌ¹Ç·Î ±×³É ¸®ÅÏ
		if (1 == bSize)
			return true;
		else
		{
			int j = 1;

			do
			{
				int p = wCell + (DRAGON_SOUL_BOX_COLUMN_NUM * j);

				if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
					return false;

				if (m_pointsInstant.bItemGrid[p])
					if (m_pointsInstant.wDSItemGrid[p] != iExceptionCell)
						return false;
			} while (++j < bSize);

			return true;
		}
	}
	}
	return false;
	}

int CHARACTER::GetEmptyInventory(uint8_t size) const
{
	// NOTE: ÇöÀç ÀÌ ÇÔ¼ö´Â ¾ÆÀÌÅÛ Áö±Þ, È¹µæ µîÀÇ ÇàÀ§¸¦ ÇÒ ¶§ ÀÎº¥Åä¸®ÀÇ ºó Ä­À» Ã£±â À§ÇØ »ç¿ëµÇ°í ÀÖ´Âµ¥,
	//		º§Æ® ÀÎº¥Åä¸®´Â Æ¯¼ö ÀÎº¥Åä¸®ÀÌ¹Ç·Î °Ë»çÇÏÁö ¾Êµµ·Ï ÇÑ´Ù. (±âº» ÀÎº¥Åä¸®: INVENTORY_MAX_NUM ±îÁö¸¸ °Ë»ç)
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	const int inventoryLimit = std::min(Inventory_Size(), (int)INVENTORY_MAX_NUM);
	for (int i = 0; i < inventoryLimit; ++i)
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		if (IsEmptyItemGrid(TItemPos(INVENTORY, i), size))
			return i;
	return -1;
}

#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
int CHARACTER::ExtraInventoryMaxSlots(int iArg1, bool bAuto) const {

	if (bAuto) {
		if ((iArg1 >= 0) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 1)))
			iArg1 = 0;
		else if ((iArg1 >= (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 1)) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 2)))
			iArg1 = 1;
		else if ((iArg1 >= (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 2)) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 3)))
			iArg1 = 2;
		else if ((iArg1 >= (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 3)) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 4)))
			iArg1 = 3;
		else if ((iArg1 >= (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 4)) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 5)))
			iArg1 = 4;
		else if ((iArg1 >= (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 5)) && (iArg1 < (EXTRA_INVENTORY_CATEGORY_MAX_NUM * 6)))
			iArg1 = 5;
	}

	if ((iArg1 < 0) || (iArg1 > 5))
		return 0;

	int iUnlock;
	switch (iArg1) {
	case 0: {
		iUnlock = GetQuestFlag("lock_extra.cat1") * 5;
		break;
	}
	case 1: {
		iUnlock = GetQuestFlag("lock_extra.cat2") * 5;
		break;
	}
	case 2: {
		iUnlock = GetQuestFlag("lock_extra.cat3") * 5;
		break;
	}
	case 3: {
		iUnlock = GetQuestFlag("lock_extra.cat4") * 5;
		break;
	}
	case 4: {
		iUnlock = GetQuestFlag("lock_extra.cat5") * 5;
		break;
	}
	case 5: {
		iUnlock = GetQuestFlag("lock_extra.cat6") * 5;
		break;
	}
	default: {
		iUnlock = 0;
		break;
	}
	}

	//int iUnlock = GetPoint(POINT_EXTRA_INVENTORY1 + iArg1) * 5;
	int iMaxUnlock = 25 + EXTRA_INVENTORY_PAGE_SIZE;
	int iStart = EXTRA_INVENTORY_CATEGORY_MAX_NUM * iArg1;
	int iFree = (EXTRA_INVENTORY_PAGE_SIZE * 2) + 20;
	return iUnlock > iMaxUnlock ? iMaxUnlock + iStart + iFree : iUnlock + iStart + iFree;
}

static int NeedKeysForExtraInventory[] = {
											1, // 20-25
											1, // 25-30
											1, // 30-35
											2, // 35-40
											2, // 40-45 : end page 3
											2, // 45-50
											3, // 50-55
											3, // 55-60
											3, // 60-65
											4, // 65-70
											4, // 70-75
											4, // 75-80
											5, // 80-85
											6, // 90-95 : end page 4
};

void CHARACTER::UnlockExtraInventory(uint8_t category) {
	if (category > 5) {
		return;
	}

#ifdef ENABLE_SPAM_CHECK
	int32_t time = GetLastUnlock() - get_global_time();
	if (time > 0) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 234, "%d", time);
#endif
		return;
	}
#endif

	std::string stageName;
	switch (category) {
	case 1: {
		stageName = "lock_extra.cat2";
	} break;
	case 2: {
		stageName = "lock_extra.cat3";
	} break;
	case 3: {
		stageName = "lock_extra.cat4";
	} break;
	case 4: {
		stageName = "lock_extra.cat5";
	} break;
	case 5: {
		stageName = "lock_extra.cat6";
	} break;
	default: {
		stageName = "lock_extra.cat1";
	} break;
	}

	uint8_t stage = GetQuestFlag(stageName.c_str());
	if (stage < 0 || stage >= 14)
		return;

	int needKeys = NeedKeysForExtraInventory[stage];
	if (CountSpecifyItem(72320) >= needKeys) {
		RemoveSpecifyItem(72320, needKeys);

		SetQuestFlag(stageName.c_str(), stage + 1);
		PointChange(POINT_EXTRA_INVENTORY1 + category, stage + 1);
		ChatPacket(CHAT_TYPE_COMMAND, "RefreshExpandInventory");
#ifdef ENABLE_SPAM_CHECK
		SetLastUnlock();
#endif
	}
	else {
		ChatPacket(CHAT_TYPE_COMMAND, "update_envanter_need %d", needKeys - CountSpecifyItem(72320));
	}
}
#endif

#ifdef ENABLE_EXTRA_INVENTORY
int CHARACTER::GetEmptyExtraInventory(LPITEM pItem) const
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	sys_log(0, "Razor93 LOG:: Called: Char_item.cpp  CHARACTER::GetEmptyExtraInventory(LPITEM pItem) const");
#endif
	uint8_t category = pItem->GetExtraCategory();
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	for (int i = EXTRA_INVENTORY_CATEGORY_MAX_NUM * category; i < ExtraInventoryMaxSlots(category); ++i)
#else
	for (int i = EXTRA_INVENTORY_CATEGORY_MAX_NUM * category; i < EXTRA_INVENTORY_CATEGORY_MAX_NUM * (category + 1); ++i)
#endif
		if (IsEmptyItemGrid(TItemPos(EXTRA_INVENTORY, i), pItem->GetSize()))
			return i;

	return -1;
}

int CHARACTER::GetEmptyExtraInventory(uint8_t size, uint8_t category) const // needed for offline shop
{
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	for (int i = EXTRA_INVENTORY_CATEGORY_MAX_NUM * category; i < ExtraInventoryMaxSlots(category); ++i)
#else
	for (int i = EXTRA_INVENTORY_CATEGORY_MAX_NUM * category; i < EXTRA_INVENTORY_CATEGORY_MAX_NUM * (category + 1); ++i)
#endif
		if (IsEmptyItemGrid(TItemPos(EXTRA_INVENTORY, i), size))
			return i;

	return -1;
}
#endif

int CHARACTER::GetEmptyDragonSoulInventory(LPITEM pItem) const
{

	if (nullptr == pItem || !pItem->IsDragonSoul())
		return -1;

	uint8_t bSize = pItem->GetSize();
	uint16_t wBaseCell = DSManager::instance().GetBasePosition(pItem);

	if (WORD_MAX == wBaseCell)
		return -1;

	for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; ++i)
		if (IsEmptyItemGrid(TItemPos(DRAGON_SOUL_INVENTORY, i + wBaseCell), bSize))
			return i + wBaseCell;

	return -1;
}

void CHARACTER::CopyDragonSoulItemGrid(std::vector<uint16_t>&vDragonSoulItemGrid) const
{
	vDragonSoulItemGrid.resize(DRAGON_SOUL_INVENTORY_MAX_NUM);

	std::copy(m_pointsInstant.wDSItemGrid, m_pointsInstant.wDSItemGrid + DRAGON_SOUL_INVENTORY_MAX_NUM, vDragonSoulItemGrid.begin());
}

int CHARACTER::CountEmptyInventory() const
{
	int	count = 0;

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	const int inventoryLimit = std::min(Inventory_Size(), (int)INVENTORY_MAX_NUM);
	for (int i = 0; i < inventoryLimit; ++i)
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		if (GetInventoryItem(i))
			count += GetInventoryItem(i)->GetSize();

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	return (inventoryLimit - count);
#else
	return (INVENTORY_MAX_NUM - count);
#endif
}

bool CHARACTER::GiveRecallItem(LPITEM item)
{
	int idx = GetMapIndex();
	int iEmpireByMapIndex = -1;

	if (idx < 20)
		iEmpireByMapIndex = 1;
	else if (idx < 40)
		iEmpireByMapIndex = 2;
	else if (idx < 60)
		iEmpireByMapIndex = 3;
	else if (idx < 10000)
		iEmpireByMapIndex = 0;

	switch (idx)
	{
	case 66:
	case 216:
		iEmpireByMapIndex = -1;
		break;
	}

	if (iEmpireByMapIndex && GetEmpire() != iEmpireByMapIndex)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 270, "");
#endif
		return false;
	}

	int pos;

	if (item->GetCount() == 1)	// ¾ÆÀÌÅÛÀÌ ÇÏ³ª¶ó¸é ±×³É ¼ÂÆÃ.
	{
		item->SetSocket(0, GetX());
		item->SetSocket(1, GetY());
	}
	else if ((pos = GetEmptyInventory(item->GetSize())) != -1) // ±×·¸Áö ¾Ê´Ù¸é ´Ù¸¥ ÀÎº¥Åä¸® ½½·ÔÀ» Ã£´Â´Ù.
	{
		LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), 1);

		if (nullptr != item2)
		{
			item2->SetSocket(0, GetX());
			item2->SetSocket(1, GetY());
			item2->AddToCharacter(this, TItemPos(INVENTORY, pos));

			item->SetCount(item->GetCount() - 1);
		}
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 366, "");
#endif
		return false;
	}

	return true;
}

void CHARACTER::ProcessRecallItem(LPITEM item)
{
	int idx;

	if ((idx = ecs::GetMapIndex(item->GetSocket(0), item->GetSocket(1))) == 0)
		return;

	int iEmpireByMapIndex = -1;

	if (idx < 20)
		iEmpireByMapIndex = 1;
	else if (idx < 40)
		iEmpireByMapIndex = 2;
	else if (idx < 60)
		iEmpireByMapIndex = 3;
	else if (idx < 10000)
		iEmpireByMapIndex = 0;

	switch (idx)
	{
	case 66:
	case 216:
		iEmpireByMapIndex = -1;
		break;
		// ¾Ç·æ±ºµµ ÀÏ¶§
	case 301:
	case 302:
	case 303:
	case 304:
		if (GetLevel() < 90)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 325, "%d", 90);
#endif
			return;
		}
		else
			break;
	}

	if (iEmpireByMapIndex && GetEmpire() != iEmpireByMapIndex)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 270, "");
#endif
		item->SetSocket(0, 0);
		item->SetSocket(1, 0);
	}
	else
	{
		sys_log(1, "Recall: %s %d %d -> %d %d", GetName(), GetX(), GetY(), item->GetSocket(0), item->GetSocket(1));
		WarpSet(item->GetSocket(0), item->GetSocket(1));
		item->SetCount(item->GetCount() - 1);
	}
}

bool CHARACTER::SwapItem(uint8_t bCell, uint8_t bDestCell)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ChatPacket(CHAT_TYPE_INFO, "char_item.cpp::bool bool CHARACTER::SwapItem ");//INGAME_DEBUG_RAZOR93
#endif
	if (!CanHandleItem())
		return false;

	TItemPos srcCell(INVENTORY, bCell), destCell(INVENTORY, bDestCell);

	// ¿Ã¹Ù¸¥ Cell ÀÎÁö °Ë»ç
	// ¿ëÈ¥¼®Àº SwapÇÒ ¼ö ¾øÀ¸¹Ç·Î, ¿©±â¼­ °É¸².
	//if (bCell >= INVENTORY_MAX_NUM + WEAR_MAX_NUM || bDestCell >= INVENTORY_MAX_NUM + WEAR_MAX_NUM)
	if (srcCell.IsDragonSoulEquipPosition() || destCell.IsDragonSoulEquipPosition())
		return false;

	// °°Àº CELL ÀÎÁö °Ë»ç
	if (bCell == bDestCell)
		return false;

	// µÑ ´Ù ÀåºñÃ¢ À§Ä¡¸é Swap ÇÒ ¼ö ¾ø´Ù.
	if (srcCell.IsEquipPosition() && destCell.IsEquipPosition())
		return false;

	LPITEM item1, item2;

	// item2°¡ ÀåºñÃ¢¿¡ ÀÖ´Â °ÍÀÌ µÇµµ·Ï.
	if (srcCell.IsEquipPosition())
	{
		item1 = GetInventoryItem(bDestCell);
		item2 = GetInventoryItem(bCell);
	}
	else
	{
		item1 = GetInventoryItem(bCell);
		item2 = GetInventoryItem(bDestCell);
	}

	if (!item1 || !item2)
		return false;

	if (item1 == item2)
	{
		sys_log(0, "[WARNING][WARNING][HACK USER!] : %s %d %d", m_stName.c_str(), bCell, bDestCell);
		return false;
	}

	// item2°¡ bCellÀ§Ä¡¿¡ µé¾î°¥ ¼ö ÀÖ´ÂÁö È®ÀÎÇÑ´Ù.
	if (!IsEmptyItemGrid(TItemPos(INVENTORY, item1->GetCell()), item2->GetSize(), item1->GetCell()))
		return false;

	// ¹Ù²Ü ¾ÆÀÌÅÛÀÌ ÀåºñÃ¢¿¡ ÀÖÀ¸¸é
	if (TItemPos(EQUIPMENT, item2->GetCell()).IsEquipPosition())
	{
		uint8_t bEquipCell = item2->GetCell() - INVENTORY_MAX_NUM;
		uint8_t bInvenCell = item1->GetCell();

		// Âø¿ëÁßÀÎ ¾ÆÀÌÅÛÀ» ¹þÀ» ¼ö ÀÖ°í, Âø¿ë ¿¹Á¤ ¾ÆÀÌÅÛÀÌ Âø¿ë °¡´ÉÇÑ »óÅÂ¿©¾ß¸¸ ÁøÇà
		if (item2->IsDragonSoul() || item2->GetType() == ITEM_BELT) // @fixme117
		{
			if (false == CanUnequipNow(item2) || false == CanEquipNow(item1))
				return false;
		}
		if (bEquipCell != item1->FindEquipCell(this, bEquipCell))
			return false;

		item2->RemoveFromCharacter();

		if (item1->EquipTo(this, bEquipCell))
		{
			item2->AddToCharacter(this, TItemPos(INVENTORY, bInvenCell)
#ifdef __HIGHLIGHT_SYSTEM__
				, false
#endif
			);
			////item2->ModifyPoints(false);
			////ComputePoints();
		}
		else {
			sys_err("SwapItem cannot equip %s! item1 %s", item2->GetName(), item1->GetName());
		}
	}
	else
	{
		uint8_t bCell1 = item1->GetCell();
		uint8_t bCell2 = item2->GetCell();

		item1->RemoveFromCharacter();
		item2->RemoveFromCharacter();

#ifdef __HIGHLIGHT_SYSTEM__
		item1->AddToCharacter(this, TItemPos(INVENTORY, bCell2), false);
		item2->AddToCharacter(this, TItemPos(INVENTORY, bCell1), false);
#else
		item1->AddToCharacter(this, TItemPos(INVENTORY, bCell2));
		item2->AddToCharacter(this, TItemPos(INVENTORY, bCell1));
#endif
	}

	return true;
}

//
// @version	05/07/05 Bang2ni - Skill »ç¿ëÈÄ 1.5 ÃÊ ÀÌ³»¿¡ Àåºñ Âø¿ë ±ÝÁö
//
void CHARACTER::BuffOnAttr_AddBuffsFromItem(LPITEM pItem)
{
	for (size_t i = 0; i < sizeof(g_aBuffOnAttrPoints) / sizeof(g_aBuffOnAttrPoints[0]); i++)
	{
		TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.find(g_aBuffOnAttrPoints[i]);
		if (it != m_map_buff_on_attrs.end())
		{
			it->second->AddBuffFromItem(pItem);
		}
	}
}

void CHARACTER::BuffOnAttr_RemoveBuffsFromItem(LPITEM pItem)
{
	for (size_t i = 0; i < sizeof(g_aBuffOnAttrPoints) / sizeof(g_aBuffOnAttrPoints[0]); i++)
	{
		TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.find(g_aBuffOnAttrPoints[i]);
		if (it != m_map_buff_on_attrs.end())
		{
			it->second->RemoveBuffFromItem(pItem);
		}
	}
}

void CHARACTER::BuffOnAttr_ClearAll()
{
	for (TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.begin(); it != m_map_buff_on_attrs.end(); it++)
	{
		CBuffOnAttributes* pBuff = it->second;
		if (pBuff)
		{
			pBuff->Initialize();
		}
	}
}

void CHARACTER::BuffOnAttr_ValueChange(uint8_t bType, uint8_t bOldValue, uint8_t bNewValue)
{
	TMapBuffOnAttrs::iterator it = m_map_buff_on_attrs.find(bType);

	if (0 == bNewValue)
	{
		if (m_map_buff_on_attrs.end() == it)
			return;
		else
			it->second->Off();
	}
	else if (0 == bOldValue)
	{
		CBuffOnAttributes* pBuff = nullptr;
		if (m_map_buff_on_attrs.end() == it)
		{
			switch (bType)
			{
			case POINT_ENERGY:
			{
				static uint8_t abSlot[] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_SHIELD };
				static std::vector <uint8_t> vec_slots(abSlot, abSlot + _countof(abSlot));
				pBuff = M2_NEW CBuffOnAttributes(this, bType, &vec_slots);
			}
			break;
			case POINT_COSTUME_ATTR_BONUS:
			{
				static uint8_t abSlot[] = {
					WEAR_COSTUME_BODY,
					WEAR_COSTUME_HAIR,
					WEAR_COSTUME_MOUNT,
#ifdef ENABLE_COSTUME_EFFECT_ATTR_BONUS_RAZOR93


						WEAR_COSTUME_PET_SKIN,
						WEAR_COSTUME_EFFECT_BODY,
						WEAR_COSTUME_EFFECT_WEAPON,
#endif // ENABLE_COSTUME_EFFECT_ATTR_BONUS_RAZOR93
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
						WEAR_COSTUME_WEAPON,
#endif
#ifdef ENABLE_STOLE_COSTUME
						WEAR_COSTUME_ACCE,
#endif
						WEAR_COSTUME_ACCE_SLOT,
				};

				static std::vector <uint8_t> vec_slots(abSlot, abSlot + _countof(abSlot));
				pBuff = M2_NEW CBuffOnAttributes(this, bType, &vec_slots);
			}
			break;
			default:
				break;
			}
			m_map_buff_on_attrs.insert(TMapBuffOnAttrs::value_type(bType, pBuff));

		}
		else
			pBuff = it->second;
		if (pBuff != nullptr)
			pBuff->On(bNewValue);
	}
	else
	{
		assert(m_map_buff_on_attrs.end() != it);
		it->second->ChangeBuffValue(bNewValue);
	}
}


// CHECK_UNIQUE_GROUP
// END_OF_CHECK_UNIQUE_GROUP

// NEW_HAIR_STYLE_ADD
bool CHARACTER::ItemProcess_Hair(LPITEM item, int iDestCell)
{
	if (item->CheckItemUseLevel(GetLevel()) == false)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 405, "");
#endif
		return false;
	}

	uint32_t hair = item->GetVnum();

	switch (GetJob())
	{
	case JOB_WARRIOR:
		hair -= 72000; // 73001 - 72000 = 1001 ºÎÅÍ Çì¾î ¹øÈ£ ½ÃÀÛ
		break;

	case JOB_ASSASSIN:
		hair -= 71250;
		break;

	case JOB_SURA:
		hair -= 70500;
		break;

	case JOB_SHAMAN:
		hair -= 69750;
		break;
#ifdef ENABLE_WOLFMAN_CHARACTER
	case JOB_WOLFMAN:
		break; // NOTE: ÀÌ Çì¾îÄÚµå´Â ¾È ¾²ÀÌ¹Ç·Î ÆÐ½º. (ÇöÀç Çì¾î½Ã½ºÅÛÀº ÀÌ¹Ì ÄÚ½ºÆ¬À¸·Î ´ëÃ¼ µÈ »óÅÂÀÓ)
#endif
	default:
		return false;
		break;
	}

	if (hair == GetPart(PART_HAIR))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 311, "");
#endif
		return true;
	}

	item->SetCount(item->GetCount() - 1);

	SetPart(PART_HAIR, hair);
	UpdatePacket();

	return true;
}
// END_NEW_HAIR_STYLE_ADD

bool CHARACTER::ItemProcess_Polymorph(LPITEM item)
{

#ifdef ENABLE_PVP_ADVANCED
	if ((GetDuel("BlockPoly")))
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 516, "");
#endif
		return false;
	}
#endif

	if (IsPolymorphed()) {
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 437, "");
#endif
		return false;
	}

	if (true == IsRiding())
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 741, "");
#endif
		return false;
	}

	uint32_t dwVnum = item->GetSocket(0);

	if (dwVnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 450, "");
#endif
		item->SetCount(item->GetCount() - 1);
		return false;
	}

	const CMob* pMob = CMobManager::instance().Get(dwVnum);

	if (pMob == nullptr)
	{
#ifdef TEXTS_IMPROVEMENT
		ChatPacketNew(CHAT_TYPE_INFO, 451, "");
#endif
		item->SetCount(item->GetCount() - 1);
		return false;
	}

	switch (item->GetVnum())
	{
	case 70104:
	case 70105:
	case 70106:
	case 70107:
	case 71093:
	{
		// µÐ°©±¸ Ã³¸®
		sys_log(0, "USE_POLYMORPH_BALL PID(%d) vnum(%d)", GetPlayerID(), dwVnum);

		// ·¹º§ Á¦ÇÑ Ã¼Å©
		int iPolymorphLevelLimit = std::max(0, 20 - GetLevel() * 3 / 10);
		if (pMob->m_table.bLevel >= GetLevel() + iPolymorphLevelLimit)
		{
#ifdef TEXTS_IMPROVEMENT
			ChatPacketNew(CHAT_TYPE_INFO, 275, "");
#endif
			return false;
		}

		int iDuration = GetSkillLevel(POLYMORPH_SKILL_ID) == 0 ? 5 : (5 + (5 + GetSkillLevel(POLYMORPH_SKILL_ID) / 40 * 25));
		iDuration *= 60;

		uint32_t dwBonus = 0;

		dwBonus = (2 + GetSkillLevel(POLYMORPH_SKILL_ID) / 40) * 100;

		AddAffect(AFFECT_POLYMORPH, POINT_POLYMORPH, dwVnum, AFF_POLYMORPH, iDuration, 0, true);
		AddAffect(AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonus, AFF_POLYMORPH, iDuration, 0, false);

		item->SetCount(item->GetCount() - 1);
	}
	break;

	case 50322:
	{
		// º¸·ù

		// µÐ°©¼­ Ã³¸®
		// ¼ÒÄÏ0                ¼ÒÄÏ1           ¼ÒÄÏ2
		// µÐ°©ÇÒ ¸ó½ºÅÍ ¹øÈ£   ¼ö·ÃÁ¤µµ        µÐ°©¼­ ·¹º§
		sys_log(0, "USE_POLYMORPH_BOOK: %s(%u) vnum(%u)", GetName(), GetPlayerID(), dwVnum);

		if (CPolymorphUtils::instance().PolymorphCharacter(this, item, pMob) == true)
		{
			CPolymorphUtils::instance().UpdateBookPracticeGrade(this, item);
		}
		else
		{
		}
	}
	break;

	default:
		sys_err("POLYMORPH invalid item passed PID(%d) vnum(%d)", GetPlayerID(), item->GetOriginalVnum());
		return false;
	}

	return true;
}

bool CHARACTER::CanDoCube() const
{
	if (m_bIsObserver)	return false;
	if (GetShop())		return false;
	if (GetMyShop())	return false;
	if (m_bUnderRefine)	return false;
	if (IsWarping())	return false;

	return true;
}

#ifdef ENABLE_RECALL
void CHARACTER::AutoRecallProcess()
{
	if (!IsPC())
		return;

#ifdef __PET_SYSTEM__
	{
		const CAffect* pAffect = FindAffect(AFFECT_RECALL1);
		if (pAffect) {
			LPITEM pItem = FindItemByID(pAffect->dwFlag);
			if (pItem) {
				if (pItem->GetSocket(2) == false) {
					CPetSystem* petSystem = GetPetSystem();
					if (petSystem) {
						if (petSystem->CountSummoned() < 1) {
							CPetActor* pPet = petSystem->Summon(pItem->GetValue(1), pItem, "", false);
							if (!pPet)
								RemoveAffect(const_cast<CAffect*>(pAffect));
						}
					}
					else
						RemoveAffect(const_cast<CAffect*>(pAffect));
				}
			}
			else
				RemoveAffect(const_cast<CAffect*>(pAffect));
		}
	}
#endif
#ifdef __NEWPET_SYSTEM__
	{
		const CAffect* pAffect = FindAffect(AFFECT_RECALL2);
		if (pAffect) {
			LPITEM pItem = FindItemByID(pAffect->dwFlag);
			if (pItem) {
				if (pItem->GetSocket(0) == false) {
					CNewPetSystem* petSystem = GetNewPetSystem();
					if (petSystem) {
						if (petSystem->CountSummoned() < 1) {
							CNewPetActor* pPet = petSystem->Summon(pItem->GetValue(0), pItem, "", false);
							if (!pPet)
								RemoveAffect(const_cast<CAffect*>(pAffect));
						}
					}
					else
						RemoveAffect(const_cast<CAffect*>(pAffect));
				}
			}
			else
				RemoveAffect(const_cast<CAffect*>(pAffect));
		}
	}
#endif
}
#endif

void CHARACTER::AutoRecoveryItemProcess(const EAffectTypes type)
{
	if (true == IsDead() || true == IsStun())
		return;

	if (false == IsPC())
		return;

#ifdef ENABLE_PVP_ADVANCED	
	if (
#ifdef ENABLE_NEW_USE_POTION
	((type == AFFECT_AUTO_HP_RECOVERY2) ||
#endif
		(type == AFFECT_AUTO_HP_RECOVERY)
#ifdef ENABLE_NEW_USE_POTION
	)
#endif
		&& (GetDuel("BlockPotion")))
		return;
#endif

	if ((type != AFFECT_AUTO_HP_RECOVERY) && (type != AFFECT_AUTO_SP_RECOVERY)
#ifdef ENABLE_NEW_USE_POTION
		&& (type != AFFECT_AUTO_HP_RECOVERY2) && (type != AFFECT_AUTO_SP_RECOVERY2)
#endif
		)
		return;

	if (nullptr != FindAffect(AFFECT_STUN))
		return;

	{
		const uint32_t stunSkills[] = { SKILL_TANHWAN, SKILL_GEOMPUNG, SKILL_BYEURAK, SKILL_GIGUNG };

		for (size_t i = 0; i < sizeof(stunSkills) / sizeof(uint32_t); ++i)
		{
			const CAffect* p = FindAffect(stunSkills[i]);

			if (nullptr != p && AFF_STUN == p->dwFlag)
				return;
		}
	}

	const CAffect* pAffect = FindAffect(type);
	const size_t idx_of_amount_of_used = 1;
	const size_t idx_of_amount_of_full = 2;

	if (nullptr != pAffect)
	{
		LPITEM pItem = FindItemByID(pAffect->dwFlag);

		if (nullptr != pItem && true == pItem->GetSocket(0))
		{
			if (!CArenaManager::instance().IsArenaMap(GetMapIndex())
#ifdef ENABLE_NEWSTUFF
				&& !(g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(pItem->GetVnum()))
#endif
				)
			{
				const int32_t amount_of_used = pItem->GetSocket(idx_of_amount_of_used);
				const int32_t amount_of_full = pItem->GetSocket(idx_of_amount_of_full);

				const int32_t avail = amount_of_full - amount_of_used;

				int32_t amount = 0;
#ifdef ENABLE_NEW_USE_POTION
				if ((type == AFFECT_AUTO_HP_RECOVERY) || (type == AFFECT_AUTO_HP_RECOVERY2))
#else
				if (AFFECT_AUTO_HP_RECOVERY == type)
#endif
				{
					amount = GetMaxHP() - (GetHP() + GetPoint(POINT_HP_RECOVERY));
				}
#ifdef ENABLE_NEW_USE_POTION
				else if ((type == AFFECT_AUTO_SP_RECOVERY) || (type == AFFECT_AUTO_SP_RECOVERY2))
#else
				else if (AFFECT_AUTO_SP_RECOVERY == type)
#endif
				{
					amount = GetMaxSP() - (GetSP() + GetPoint(POINT_SP_RECOVERY));
				}

				if (amount > 0)
				{
					if (avail > amount)
					{
						const int pct_of_used = amount_of_used * 100 / amount_of_full;
						const int pct_of_will_used = (amount_of_used + amount) * 100 / amount_of_full;

						bool bLog = false;
						// »ç¿ë·®ÀÇ 10% ´ÜÀ§·Î ·Î±×¸¦ ³²±è
						// (»ç¿ë·®ÀÇ %¿¡¼­, ½ÊÀÇ ÀÚ¸®°¡ ¹Ù²ð ¶§¸¶´Ù ·Î±×¸¦ ³²±è.)
						if ((pct_of_will_used / 10) - (pct_of_used / 10) >= 1)
							bLog = true;

#ifdef ENABLE_NEW_USE_POTION
						if (pItem->GetVnum() != ITEM_AUTO_HP_RECOVERY_X && pItem->GetVnum() != ITEM_AUTO_SP_RECOVERY_X)
							pItem->SetSocket(idx_of_amount_of_used, amount_of_used + amount, bLog);
#else
						pItem->SetSocket(idx_of_amount_of_used, amount_of_used + amount, bLog);
#endif
					}
					else if (pItem->GetVnum() != ITEM_AUTO_HP_RECOVERY_X && pItem->GetVnum() != ITEM_AUTO_SP_RECOVERY_X)
					{
						amount = avail;

						ITEM_MANAGER::instance().RemoveItem(pItem);
					}

#ifdef ENABLE_NEW_USE_POTION
					if ((type == AFFECT_AUTO_HP_RECOVERY) || (type == AFFECT_AUTO_HP_RECOVERY2))
#else
					if (AFFECT_AUTO_HP_RECOVERY == type)
#endif
					{
						PointChange(POINT_HP_RECOVERY, amount);
						EffectPacket(SE_AUTO_HPUP);
					}
#ifdef ENABLE_NEW_USE_POTION
					else if ((type == AFFECT_AUTO_SP_RECOVERY) || (type == AFFECT_AUTO_SP_RECOVERY2))
#else
					else if (AFFECT_AUTO_SP_RECOVERY == type)
#endif
					{
						PointChange(POINT_SP_RECOVERY, amount);
						EffectPacket(SE_AUTO_SPUP);
					}
				}
			}
			else
			{
				pItem->Lock(false);
				pItem->SetSocket(0, false);
				RemoveAffect(const_cast<CAffect*>(pAffect));
			}
		}
		else
		{
			RemoveAffect(const_cast<CAffect*>(pAffect));
		}
	}
}

bool CHARACTER::IsValidItemPosition(TItemPos Pos) const
{

	uint8_t window_type = Pos.window_type;
	uint16_t cell = Pos.cell;

	switch (window_type)
	{
	case RESERVED_WINDOW:
		return false;

	case INVENTORY:
	case EQUIPMENT:
		return cell < (INVENTORY_AND_EQUIP_SLOT_MAX);

	case DRAGON_SOUL_INVENTORY:
		return cell < (DRAGON_SOUL_INVENTORY_MAX_NUM);
#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
		return cell < SWITCHBOT_SLOT_COUNT;
#endif
	case SAFEBOX:
		if (nullptr != m_pkSafebox)
			return m_pkSafebox->IsValidPosition(cell);
		else
			return false;

	case MALL:
		if (nullptr != m_pkMall)
			return m_pkMall->IsValidPosition(cell);
		else
			return false;

#ifdef ENABLE_EXTRA_INVENTORY
	case EXTRA_INVENTORY:
		return cell < (EXTRA_INVENTORY_MAX_NUM);
#endif
	default:
		return false;
	}
}

/// ÇöÀç Ä³¸¯ÅÍÀÇ »óÅÂ¸¦ ¹ÙÅÁÀ¸·Î ÁÖ¾îÁø itemÀ» Âø¿ëÇÒ ¼ö ÀÖ´Â Áö È®ÀÎÇÏ°í, ºÒ°¡´É ÇÏ´Ù¸é Ä³¸¯ÅÍ¿¡°Ô ÀÌÀ¯¸¦ ¾Ë·ÁÁÖ´Â ÇÔ¼ö
#

/// ÇöÀç Ä³¸¯ÅÍÀÇ »óÅÂ¸¦ ¹ÙÅÁÀ¸·Î Âø¿ë ÁßÀÎ itemÀ» ¹þÀ» ¼ö ÀÖ´Â Áö È®ÀÎÇÏ°í, ºÒ°¡´É ÇÏ´Ù¸é Ä³¸¯ÅÍ¿¡°Ô ÀÌÀ¯¸¦ ¾Ë·ÁÁÖ´Â ÇÔ¼ö
#ifdef __ATTR_TRANSFER_SYSTEM__
bool CHARACTER::CanDoAttrTransfer() const
{
	if (m_bIsObserver)
		return false;

	if (GetShop())
		return false;

	if (GetMyShop())
		return false;

	if (m_bUnderRefine)
		return false;

	if (IsWarping())
		return false;

#ifdef ENABLE_ACCE_SYSTEM
	if ((m_bAcceCombination) || (m_bAcceAbsorption))
		return false;
#endif

	return true;
}
#endif

void CItem::Initialize()
{
	CEntity::Initialize(ENTITY_ITEM);

	m_bWindow = RESERVED_WINDOW;
	m_pOwner = nullptr;
	m_dwID = 0;
	m_bEquipped = false;
	m_dwVID = m_wCell = m_dwCount = m_lFlag = 0;
	m_pProto = nullptr;
	m_bExchanging = false;
#ifdef ENABLE_SOUL_SYSTEM
	m_pkSoulItemEvent = nullptr;
#endif
	m_pkUniqueExpireEvent = nullptr;
	memset(&m_alSockets, 0, sizeof(m_alSockets));
	memset(&m_aAttr, 0, sizeof(m_aAttr));
#ifdef ATTR_LOCK
	m_sLockedAttr = -1;
#endif

	m_pkDestroyEvent = nullptr;
	m_pkOwnershipEvent = nullptr;
	m_dwOwnershipPID = 0;

	m_pkTimerBasedOnWearExpireEvent = nullptr;
	m_pkRealTimeExpireEvent = nullptr;

	m_pkAccessorySocketExpireEvent = nullptr;

	m_bSkipSave = false;
	m_dwLastOwnerPID = 0;
}

void CItem::Destroy()
{
	event_cancel(&m_pkDestroyEvent);
	event_cancel(&m_pkOwnershipEvent);
	event_cancel(&m_pkUniqueExpireEvent);

#ifdef ENABLE_SOUL_SYSTEM
	event_cancel(&m_pkSoulItemEvent);
#endif

	event_cancel(&m_pkTimerBasedOnWearExpireEvent);
	event_cancel(&m_pkRealTimeExpireEvent);
	event_cancel(&m_pkAccessorySocketExpireEvent);

	CEntity::Destroy();

	const entt::entity e = ItemEntityOf(this);
	if (GetSectree())
		GetSectree()->RemoveEntity(this);
	if (e != entt::null && g_registry.valid(e))
	{
		g_registry.remove<ecs::SectorPlacement>(e);
		g_registry.remove<ecs::ViewActiveTag>(e);
	}

	const entt::entity e2 = e;
	if (e2 != entt::null)
		g_dispatcher.trigger(ecs::EvItemDestroyed { e2, GetID() });
}

void CItem::Save()
{
	if (m_bSkipSave)
		return;

	ITEM_MANAGER::instance().DelayedSave(this);
}

void CItem::SetProto(const TItemTable* table)
{
	assert(table != NULL);
	m_pProto = table;
	SetFlag(m_pProto->dwFlags);
}

#ifdef ENABLE_ITEM_EXTRA_PROTO
void CItem::SetExtraProto(TItemExtraProto* Proto)
{
	m_ExtraProto = Proto;
}

TItemExtraProto* CItem::GetExtraProto()
{
	return m_ExtraProto;
}
#endif

void CItem::SetDestroyEvent(LPEVENT pkEvent)
{
	m_pkDestroyEvent = pkEvent;
}

void CItem::StartDestroyEvent(int iSec)
{
	if (m_pkDestroyEvent)
		return;

	item_event_info* info = AllocEventInfo<item_event_info>();
	info->item = this;

	SetDestroyEvent(event_create(item_destroy_event, info, PASSES_PER_SEC(iSec)));
}

void CItem::SetUniqueExpireEvent(LPEVENT pkEvent)
{
	m_pkUniqueExpireEvent = pkEvent;
}

void CItem::StartUniqueExpireEvent()
{
	if (GetType() != ITEM_UNIQUE)
		return;

	if (m_pkUniqueExpireEvent)
		return;

	if (IsRealTimeItem() || IsRealTimeFirstUseItem() || IsUnlimitedTimeUnique())
		return;

	// HARD CODING
	/*if (GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		m_pOwner->ShowAlignment(false);*/

	int iSec = GetSocket(ITEM_SOCKET_UNIQUE_SAVE_TIME);

	if (iSec == 0)
		iSec = 60;
	else
		iSec = MIN(iSec, 60);

	SetSocket(ITEM_SOCKET_UNIQUE_SAVE_TIME, 0);

	item_event_info* info = AllocEventInfo<item_event_info>();
	info->item = this;

	SetUniqueExpireEvent(event_create(unique_expire_event, info, PASSES_PER_SEC(iSec)));

	const entt::entity e = ItemEntityOf(this);
	if (e != entt::null)
		g_dispatcher.trigger(ecs::EvItemExpired { e, GetID() });
}

void CItem::StopUniqueExpireEvent()
{
	if (!m_pkUniqueExpireEvent)
		return;

	if (GetValue(2) != 0)
		return;

	// HARD CODING
	/*if (GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		m_pOwner->ShowAlignment(true);*/

	SetSocket(ITEM_SOCKET_UNIQUE_SAVE_TIME, event_time(m_pkUniqueExpireEvent) / passes_per_sec);
	event_cancel(&m_pkUniqueExpireEvent);

	ITEM_MANAGER::instance().SaveSingleItem(this);
}

void CItem::SetTimerBasedOnWearExpireEvent(LPEVENT pkEvent)
{
	m_pkTimerBasedOnWearExpireEvent = pkEvent;
}

void CItem::StartTimerBasedOnWearExpireEvent()
{
	if (m_pkTimerBasedOnWearExpireEvent)
		return;

	if (IsRealTimeItem())
		return;

	if (-1 == GetProto()->cLimitTimerBasedOnWearIndex)
		return;

	int iSec = GetSocket(0);

	if (0 != iSec)
	{
		iSec %= 60;
		if (0 == iSec)
			iSec = 60;
	}

	item_event_info* info = AllocEventInfo<item_event_info>();
	info->item = this;

	SetTimerBasedOnWearExpireEvent(event_create(timer_based_on_wear_expire_event, info, PASSES_PER_SEC(iSec)));

	const entt::entity e = ItemEntityOf(this);
	if (e != entt::null)
		g_dispatcher.trigger(ecs::EvItemExpired { e, GetID() });
}

void CItem::StopTimerBasedOnWearExpireEvent()
{
	if (!m_pkTimerBasedOnWearExpireEvent)
		return;

	int remain_time = GetSocket(ITEM_SOCKET_REMAIN_SEC) - event_processing_time(m_pkTimerBasedOnWearExpireEvent) / passes_per_sec;

	SetSocket(ITEM_SOCKET_REMAIN_SEC, remain_time);
	event_cancel(&m_pkTimerBasedOnWearExpireEvent);

	ITEM_MANAGER::instance().SaveSingleItem(this);
}

void CItem::StartRealTimeExpireEvent()
{
	if (m_pkRealTimeExpireEvent)
		return;

	for (auto aLimit : GetProto()->aLimits)
	{
		if (LIMIT_REAL_TIME == aLimit.bType || LIMIT_REAL_TIME_START_FIRST_USE == aLimit.bType)
		{
			item_vid_event_info* info = AllocEventInfo<item_vid_event_info>();
			info->item_vid = GetVID();
#ifdef ENABLE_NEW_USE_POTION
			if ((GetType() == ITEM_USE) && (GetSubType() == USE_NEW_POTIION)) {
				int32_t remainSec = GetSocket(0);
				if (remainSec <= 0) {
					if (GetSocket(1) == 1) {
						auto* pkOwner = GetOwner();
						if (pkOwner) {
							if (pkOwner->FindAffect(GetValue(0))) {
								pkOwner->RemoveAffect(GetValue(0));
							}

#ifdef TEXTS_IMPROVEMENT
							pkOwner->ChatPacketNew(CHAT_TYPE_INFO, 27, "%s", GetName());
#endif
						}
					}

					ITEM_MANAGER::instance().RemoveItem(this, "REAL_TIME_EXPIRE");
					return;
				}

				info->newpotion = true;
				m_pkRealTimeExpireEvent = event_create(real_time_expire_event, info, PASSES_PER_SEC(remainSec > 60 ? 60 : remainSec));

				const entt::entity e = ItemEntityOf(this);
				if (e != entt::null)
					g_dispatcher.trigger(ecs::EvItemExpired { e, GetID() });
			}
			else {
				info->newpotion = false;
				m_pkRealTimeExpireEvent = event_create(real_time_expire_event, info, PASSES_PER_SEC(1));
				const entt::entity e = ItemEntityOf(this);
				if (e != entt::null)
					g_dispatcher.trigger(ecs::EvItemExpired { e, GetID() });
			}
#else
			m_pkRealTimeExpireEvent = event_create(real_time_expire_event, info, PASSES_PER_SEC(1));

			const entt::entity e = ItemEntityOf(this);
			if (e != entt::null)
				g_dispatcher.trigger(ecs::EvItemExpired { e, GetID() });
#endif

			sys_log(0, "REAL_TIME_EXPIRE: StartRealTimeExpireEvent");
			return;
		}
	}
}

void CItem::StartAccessorySocketExpireEvent()
{
	if (!IsAccessoryForSocket())
		return;

	if (m_pkAccessorySocketExpireEvent)
		return;

	if (GetAccessorySocketMaxGrade() == 0)
		return;

	if (GetAccessorySocketGrade() == 0)
		return;

	int iSec = GetAccessorySocketDownGradeTime();
#ifdef ENABLE_INFINITE_RAFINES
	if (iSec > 86400) {
		return;
	}
#endif
	SetAccessorySocketExpireEvent(nullptr);

	if (iSec <= 1)
		iSec = 5;
	else
		iSec = MIN(iSec, 60);

	item_vid_event_info* info = AllocEventInfo<item_vid_event_info>();
	info->item_vid = GetVID();

	SetAccessorySocketExpireEvent(event_create(accessory_socket_expire_event, info, PASSES_PER_SEC(iSec)));

	const entt::entity e = ItemEntityOf(this);
	if (e != entt::null)
		g_dispatcher.trigger(ecs::EvItemExpired { e, GetID() });
}

void CItem::StopAccessorySocketExpireEvent()
{
	if (!m_pkAccessorySocketExpireEvent)
		return;

	if (!IsAccessoryForSocket())
		return;

	int new_time = GetAccessorySocketDownGradeTime() - (60 - event_time(m_pkAccessorySocketExpireEvent) / passes_per_sec);

	event_cancel(&m_pkAccessorySocketExpireEvent);

	if (new_time <= 1)
	{
		AccessorySocketDegrade();
	}
	else
	{
		SetAccessorySocketDownGradeTime(new_time);
	}
}

void CItem::SetAccessorySocketExpireEvent(LPEVENT pkEvent)
{
	m_pkAccessorySocketExpireEvent = pkEvent;
}

#ifdef ENABLE_SOUL_SYSTEM
void CItem::SetSoulItemEvent(LPEVENT pkEvent)
{
	m_pkSoulItemEvent = pkEvent;
}

void CItem::StartSoulItemEvent()
{
	if (GetType() != ITEM_SOUL)
		return;

	if (m_pkSoulItemEvent)
		return;

	int iMinutes = (GetSocket(2) / 10000);
	if (iMinutes >= GetLimitValue(1))
		return;

	item_vid_event_info* pInfo = AllocEventInfo<item_vid_event_info>();
	pInfo->item_vid = GetVID();
	SetSoulItemEvent(event_create(soul_item_event, pInfo, PASSES_PER_SEC(test_server ? 5 : 60)));

	const entt::entity e = ItemEntityOf(this);
	if (e != entt::null)
		g_dispatcher.trigger(ecs::EvItemExpired { e, GetID() });
}
#endif

#ifdef ENABLE_RUNE_SYSTEM
void CItem::InitializeRune() {
	if ((GetType() == ITEM_USE) && (GetSubType() == USE_RUNE_PERC_CHARGE)) {
		SetSocket(0, GetValue(0));
		UpdatePacket();
		return;
	}

	if (!IsRune())
		return;

	int32_t lTime = 0, lAttr = 0, lValue = 0;
	for (int i = 0; i < RUNE_ATTR_EACH; ++i) {
		lTime = GetSocket(0);
		lAttr = GetRuneAttrType(i);
		lValue = GetRuneAttrValue(i, lTime);
		if ((lAttr > 0) && (lValue > 0)) {
			SetForceAttribute(i, lAttr, lValue);
		}
	}
}

void CItem::ChangeRuneAttr(int32_t lTime) {
	int32_t lValue = GetRuneAttrValue(0, lTime);
	bool bChange = lValue != GetAttributeValue(0) ? true : false;
	if (!bChange)
		return;

	bool isActive = GetSocket(1) == 1 ? true : false;
	if (isActive)
		ModifyPoints(false);

	for (int i = 0; i < RUNE_ATTR_EACH; ++i) {
		lValue = GetRuneAttrValue(i, lTime);
		SetForceAttribute(i, GetAttributeType(i), lValue);
	}

	if (isActive)
		ModifyPoints(true);

	UpdatePacket();
}

void CItem::ActivateRuneBonus() {
	if (!m_pOwner)
		return;

	LPITEM pkItem1 = m_pOwner->GetWear(WEAR_RUNE7);
	if (!pkItem1)
		return;

	if (pkItem1->GetSocket(1) == 1)
		return;

	bool bCan = true;
	int iMaxSubTypes = RUNE_SUBTYPES - 1;
	LPITEM pkItem2 = nullptr;
	for (int i = 0; i < iMaxSubTypes; i++) {
		pkItem2 = m_pOwner->GetWear(WEAR_RUNE1 + i);
		if (pkItem2) {
			if (pkItem2->GetSocket(1) != 1) {
				bCan = false;
				break;
			}
			else {
				if (int32_t(pkItem2->GetSocket(0) / (pkItem2->GetValue(0) / 100)) < 50) {
					bCan = false;
					break;
				}
			}
		}
		else {
			bCan = false;
			break;
		}
	}

	if (!bCan) {
		if (m_pOwner->FindAffect(AFFECT_RUNE2))
			m_pOwner->RemoveAffect(AFFECT_RUNE2);

		if (!m_pOwner->FindAffect(AFFECT_RUNE1))
			m_pOwner->AddAffect(AFFECT_RUNE1, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, false, false);

		return;
	}
	else {
		if (m_pOwner->FindAffect(AFFECT_RUNE1))
			m_pOwner->RemoveAffect(AFFECT_RUNE1);

		if (!m_pOwner->FindAffect(AFFECT_RUNE2))
			m_pOwner->AddAffect(AFFECT_RUNE2, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, false, false);
	}

	pkItem1->SetSocket(1, 1);
	pkItem1->ModifyPoints(true);
	pkItem1->UpdatePacket();
#ifdef TEXTS_IMPROVEMENT
	m_pOwner->ChatPacketNew(CHAT_TYPE_INFO, 31, "%s", pkItem1->GetName());
#endif
}

void CItem::DeactivateRuneBonus() {
	if (!m_pOwner)
		return;

	LPITEM pkItem1 = m_pOwner->GetWear(WEAR_RUNE7);
	if (!pkItem1)
		return;

	if (pkItem1->GetSocket(1) != 1)
		return;

	if (m_pOwner->FindAffect(AFFECT_RUNE2))
		m_pOwner->RemoveAffect(AFFECT_RUNE2);

	pkItem1->SetSocket(1, 0);
	pkItem1->ModifyPoints(false);
	pkItem1->UpdatePacket();
#ifdef TEXTS_IMPROVEMENT
	m_pOwner->ChatPacketNew(CHAT_TYPE_INFO, 901, "%s", pkItem1->GetName());
#endif
}

void CItem::DeactivateRuneBonusRefresh() {
	int iMaxSubTypes = RUNE_SUBTYPES - 1;
	bool bAdd = false;
	LPITEM pkItem2 = nullptr;
	if (!m_pOwner->FindAffect(AFFECT_RUNE1)) {
		for (int i = 0; i < iMaxSubTypes; i++) {
			pkItem2 = m_pOwner->GetWear(WEAR_RUNE1 + i);
			if (pkItem2) {
				if (pkItem2->GetSocket(1) != 0) {
					bAdd = true;
					break;
				}
			}
			else {
				bAdd = true;
				break;
			}
		}

		if (bAdd)
			m_pOwner->AddAffect(AFFECT_RUNE1, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, false, false);
	}
	else {
		for (int i = 0; i < iMaxSubTypes; i++) {
			pkItem2 = m_pOwner->GetWear(WEAR_RUNE1 + i);
			if (pkItem2) {
				if (pkItem2->GetSocket(1) != 0) {
					bAdd = true;
					break;
				}
			}
			else {
				bAdd = true;
				break;
			}
		}

		if (!bAdd)
			m_pOwner->RemoveAffect(AFFECT_RUNE1);
	}
}

void CItem::ActivateRune() {
	if (!IsRune())
		return;

	if (GetSocket(1) == 1)
		return;

	if (GetSocket(ITEM_SOCKET_REMAIN_SEC) <= 0) {
#ifdef TEXTS_IMPROVEMENT
		if (m_pOwner) {
			m_pOwner->ChatPacketNew(CHAT_TYPE_INFO, 30, "%s", GetName());
		}
#endif
		return;
	}

	SetSocket(1, 1);
	ModifyPoints(true);
	UpdatePacket();
#ifdef TEXTS_IMPROVEMENT
	if (m_pOwner) {
		m_pOwner->ChatPacketNew(CHAT_TYPE_INFO, 31, "%s", GetName());
	}
#endif

	ActivateRuneBonus();
}

void CItem::DeactivateRune() {
	if (!IsRune())
		return;

	if (GetSocket(1) == 0)
		return;

	DeactivateRuneBonus();

	SetSocket(1, 0);
	ModifyPoints(false);
	UpdatePacket();
	DeactivateRuneBonusRefresh();
#ifdef TEXTS_IMPROVEMENT
	if (m_pOwner) {
		m_pOwner->ChatPacketNew(CHAT_TYPE_INFO, 32, "%s", GetName());
	}
#endif
}
#endif

bool CItem::IsAccessoryForSocket()
{
	return (m_pProto->bType == ITEM_ARMOR && (m_pProto->bSubType == ARMOR_WRIST || m_pProto->bSubType == ARMOR_NECK || m_pProto->bSubType == ARMOR_EAR)) || (m_pProto->bType == ITEM_BELT);	
}

void CItem::SetAccessorySocketGrade(int iGrade
#ifdef ENABLE_INFINITE_RAFINES
	, bool infinite
#endif
)
{
	SetSocket(0, MINMAX(0, iGrade, GetAccessorySocketMaxGrade()));

	int iDownTime =
#ifdef ENABLE_INFINITE_RAFINES
		infinite == true ? 86410 : aiAccessorySocketDegradeTime[GetAccessorySocketGrade()];
#else
		aiAccessorySocketDegradeTime[GetAccessorySocketGrade()]
#endif
		;

	//if (test_server)
	//	iDownTime /= 60;

	SetAccessorySocketDownGradeTime(iDownTime);
}

void CItem::SetAccessorySocketMaxGrade(int iMaxGrade)
{
	SetSocket(1, MINMAX(0, iMaxGrade, ITEM_ACCESSORY_SOCKET_MAX_NUM));
}

void CItem::SetAccessorySocketDownGradeTime(uint32_t time)
{
	SetSocket(2, time);
}

void CItem::AccessorySocketDegrade()
{
	if (GetAccessorySocketGrade() > 0)
	{
		auto* ch = GetOwner();
#ifdef TEXTS_IMPROVEMENT
		if (ch) {
			ch->ChatPacketNew(CHAT_TYPE_INFO, 117, "%s", GetName());
		}
#endif

		ModifyPoints(false);
		SetAccessorySocketGrade(GetAccessorySocketGrade() - 1);
		ModifyPoints(true);

		int iDownTime = aiAccessorySocketDegradeTime[GetAccessorySocketGrade()];

		if (test_server)
			iDownTime /= 60;

		SetAccessorySocketDownGradeTime(iDownTime);

		if (iDownTime)
			StartAccessorySocketExpireEvent();
	}
}

int CItem::GetAccessorySocketGrade()
{
	return MINMAX(0, GetSocket(0), GetAccessorySocketMaxGrade());
}

int CItem::GetAccessorySocketMaxGrade()
{
	return MINMAX(0, GetSocket(1), ITEM_ACCESSORY_SOCKET_MAX_NUM);
}

int CItem::GetAccessorySocketDownGradeTime()
{
#ifdef ENABLE_INFINITE_RAFINES
	return GetSocket(2);
#else
	return MINMAX(0, GetSocket(2), aiAccessorySocketDegradeTime[GetAccessorySocketGrade()]);
#endif
}

bool CItem::CreateSocket(uint8_t bSlot, uint8_t bGold)
{
	assert(bSlot < ITEM_SOCKET_MAX_NUM);

	if (m_alSockets[bSlot] != 0)
	{
		sys_err("Item::CreateSocket : socket already exist %s %d", GetName(), bSlot);
		return false;
	}

	if (bGold)
		m_alSockets[bSlot] = 2;
	else
		m_alSockets[bSlot] = 1;

	UpdatePacket();

	Save();
	return true;
}

void CItem::AlterToSocketItem(int iSocketCount)
{
	if (iSocketCount >= ITEM_SOCKET_MAX_NUM)
	{
		sys_log(0, "Invalid Socket Count %d, set to maximum", ITEM_SOCKET_MAX_NUM);
		iSocketCount = ITEM_SOCKET_MAX_NUM;
	}

	for (int i = 0; i < iSocketCount; ++i)
		SetSocket(i, 1);
}

void CItem::AlterToMagicItem()
{
	if (GetAttributeSetIndex() < 0)
	{
		return;
	}

	int iSecondPct;
	int iThirdPct;

	switch (GetType())
	{
	case ITEM_WEAPON:
	{
		iSecondPct = 20;
		iThirdPct = 5;
	}
	break;

	case ITEM_ARMOR:
	{
		if (GetSubType() == ARMOR_BODY)
		{
			iSecondPct = 10;
			iThirdPct = 2;
		}
		else
		{
			iSecondPct = 10;
			iThirdPct = 1;
		}
	}
	break;
#ifdef ENABLE_ATTR_COSTUMES
	case ITEM_COSTUME:
	{
		uint8_t subtype = GetSubType();
		iSecondPct = subtype == COSTUME_BODY || subtype == COSTUME_HAIR
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
			|| subtype == COSTUME_WEAPON
#endif
			? 100 : 0;
		iThirdPct = subtype == COSTUME_BODY || subtype == COSTUME_HAIR
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
			|| subtype == COSTUME_WEAPON
#endif
			? 100 : 0;
	}
	break;
#endif
	default:
	{
		iSecondPct = 0;
		iThirdPct = 0;
	}
	break;
	}

	if (iSecondPct == 0 && iThirdPct == 0)
	{
		return;
	}

	PutAttribute(aiItemMagicAttributePercentHigh);
	if (number(1, 100) <= iSecondPct)
	{
		PutAttribute(aiItemMagicAttributePercentLow);
	}

	if (number(1, 100) <= iThirdPct)
	{
		PutAttribute(aiItemMagicAttributePercentLow);
	}
}

void CItem::ApplyAddon(int iAddonType)
{
	CItemAddonManager::instance().ApplyAddonTo(iAddonType, this);
}

void CItem::CopySocketTo(LPITEM pItem)
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		pItem->m_alSockets[i] = m_alSockets[i];
	}
}

void CItem::AttrLog()
{
	const char* pszIP = nullptr;

	if (GetOwner() && GetOwner()->GetDesc())
		pszIP = GetOwner()->GetDesc()->GetHostName();

	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		if (m_alSockets[i])
		{
#ifdef ENABLE_NEWSTUFF
			if (g_iDbLogLevel >= LOG_LEVEL_MAX)
#endif
				LogManager::instance().ItemLog(i, m_alSockets[i], 0, GetID(), "INFO_SOCKET", "", pszIP ? pszIP : "", GetOriginalVnum());
		}
	}

	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
	{
		int	type = m_aAttr[i].bType;
		int value = m_aAttr[i].sValue;

		if (type)
		{
#ifdef ENABLE_NEWSTUFF
			if (g_iDbLogLevel >= LOG_LEVEL_MAX)
#endif
				LogManager::instance().ItemLog(i, type, value, GetID(), "INFO_ATTR", "", pszIP ? pszIP : "", GetOriginalVnum());
		}
	}
}


bool CItem::IsPolymorphItem()
{
	return GetType() == ITEM_POLYMORPH;
}

bool CItem::IsRealTimeItem()
{
	if (!GetProto())
		return false;

	for (auto aLimit : GetProto()->aLimits)
	{
		if (LIMIT_REAL_TIME == aLimit.bType)
			return true;
	}
	return false;
}

bool CItem::IsRealTimeFirstUseItem()
{
	if (GetProto()) {
		for (auto aLimit : GetProto()->aLimits)
		{
			if (LIMIT_REAL_TIME_START_FIRST_USE == aLimit.bType)
				return true;
		}
	}

	return false;
}

bool CItem::IsUnlimitedTimeUnique()
{
	if (GetProto()) {
		for (auto aLimit : GetProto()->aLimits)
		{
			if (LIMIT_UNIQUE_UNLIMITED == aLimit.bType)
				return true;
		}
	}

	return false;
}

bool CItem::IsRamadanRing()
{
	if (GetVnum() == UNIQUE_ITEM_RAMADAN_RING)
		return true;
	return false;
}

int CItem::GiveMoreTime_Per(float fPercent)
{
	if (IsDragonSoul())
	{
		uint32_t duration = DSManager::instance().GetDuration(this);
		uint32_t remain_sec = GetSocket(ITEM_SOCKET_REMAIN_SEC);
		uint32_t given_time = fPercent * duration / 100u;
		if (remain_sec == duration)
			return false;
		if ((given_time + remain_sec) >= duration)
		{
			SetSocket(ITEM_SOCKET_REMAIN_SEC, duration);
			return duration - remain_sec;
		}
		else
		{
			SetSocket(ITEM_SOCKET_REMAIN_SEC, given_time + remain_sec);
			return given_time;
		}
	}
	// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
	else
		return 0;
}

int CItem::GiveMoreTime_Fix(uint32_t dwTime)
{
	if (IsDragonSoul())
	{
		uint32_t duration = DSManager::instance().GetDuration(this);
		uint32_t remain_sec = GetSocket(ITEM_SOCKET_REMAIN_SEC);
		if (remain_sec == duration)
			return false;
		if ((dwTime + remain_sec) >= duration)
		{
			SetSocket(ITEM_SOCKET_REMAIN_SEC, duration);
			return duration - remain_sec;
		}
		else
		{
			SetSocket(ITEM_SOCKET_REMAIN_SEC, dwTime + remain_sec);
			return dwTime;
		}
	}
	// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
	else
		return 0;
}

int	CItem::GetDuration()
{
	if (!GetProto())
		return -1;

	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; i++)
	{
		if (LIMIT_REAL_TIME == GetProto()->aLimits[i].bType)
			return GetProto()->aLimits[i].lValue;
	}

	if (GetProto()->cLimitTimerBasedOnWearIndex >= 0)
	{
		uint8_t cLTBOWI = GetProto()->cLimitTimerBasedOnWearIndex;
		return GetProto()->aLimits[cLTBOWI].lValue;
	}

	return -1;
}


bool CItem::CheckHumanApply()
{
	bool bHaveHuman = false;
	TItemTable* p = ITEM_MANAGER::instance().GetTable(GetVnum());
	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
		if (p->aApplies[i].bType == APPLY_ATTBONUS_HUMAN)
			bHaveHuman = true;

	return bHaveHuman;
}

uint32_t CItem::GetRefineFromVnum()
{
	return ITEM_MANAGER::instance().GetRefineFromVnum(GetVnum());
}

int CItem::GetRefineLevel()
{
	const char* name = GetBaseName();
	char* p = const_cast<char*>(strrchr(name, '+'));

	if (!p)
		return 0;

	int	rtn = 0;
	str_to_number(rtn, p + 1);

	const char* locale_name = GetName();
	p = const_cast<char*>(strrchr(locale_name, '+'));

	if (p)
	{
		int	locale_rtn = 0;
		str_to_number(locale_rtn, p + 1);
		if (locale_rtn != rtn)
		{
			sys_err("refine_level_based_on_NAME(%d) is not equal to refine_level_based_on_LOCALE_NAME(%d).", rtn, locale_rtn);
		}
	}

	return rtn;
}

void CItem::ClearMountAttributeAndAffect()
{
	auto* ch = GetOwner();

	ch->RemoveAffect(AFFECT_MOUNT);
	ch->RemoveAffect(AFFECT_MOUNT_BONUS);

	ch->MountVnum(0);

	ch->PointChange(POINT_ST, 0);
	ch->PointChange(POINT_DX, 0);
	ch->PointChange(POINT_HT, 0);
	ch->PointChange(POINT_IQ, 0);
}

int32_t CItem::FindApplyValue(uint8_t bApplyType)
{
	if (m_pProto == nullptr)
		return 0;

	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
		if (m_pProto->aApplies[i].bType == bApplyType)
			return m_pProto->aApplies[i].lValue;
	}

	return 0;
}


void CItem::AddLockedAttr()
{
	const int iCount = GetAttributeCount();
	if (iCount <= 0)
	{
		SetLockedAttr(-1);
		return;
	}

	SetLockedAttr((short)(rand() % iCount));
}

void CItem::ChangeLockedAttr()
{
	const int iCount = GetAttributeCount();
	if (iCount <= 0)
	{
		SetLockedAttr(-1);
		return;
	}

	if (iCount == 1)
	{
		SetLockedAttr(0);
		return;
	}

	int iRand = 0;
	do
	{
		iRand = rand() % iCount;
	} while (iRand == (int)GetLockedAttr());

	SetLockedAttr((short)iRand);
}

void CItem::RemoveLockedAttr()
{
	SetLockedAttr(-1);
}

void CItem::SetLockedAttr(short sIndex)
{
	m_sLockedAttr = sIndex;
	UpdatePacket();
	Save();
}

void CItem::SetExchanging(bool bOn)
{
	m_bExchanging = bOn;
}

static const bool CanPutIntoRing(LPITEM ring, LPITEM item)
{
	//const uint32_t vnum = item->GetVnum();
	return false;
}

bool CItem::CanPutInto(LPITEM item)
{
	//if (item->GetType() == ITEM_BELT) {
	//	if (GetSubType() == USE_PUT_INTO_BELT_SOCKET && GetValue(0) != 1) {
	//		return true;
	//	}
	//	else {
	//		return false;
	//	}
	//}
	/*else*/ if (item->GetType() == ITEM_RING)
		return CanPutIntoRing(item, this);

	else if (item->GetType() != ITEM_ARMOR)
		return false;

	uint32_t vnum = item->GetVnum();

	if (GetVnum() == 50634) {
		return (vnum >= 14220 && vnum <= 14233) || (vnum >= 16220 && vnum <= 16233) || (vnum >= 17220 && vnum <= 17233) ? true : false;
	}

	if (GetVnum() == 50640) {
		return (vnum >= 14580 && vnum <= 14589) || (vnum >= 15010 && vnum <= 15013) || (vnum >= 16580 && vnum <= 16593) || (vnum >= 17570 && vnum <= 17583) ? true : false;
	}

	if (GetVnum() == 50641) //limites koho aqua
	{
		return (vnum >= 8210 && vnum <= 8223) || (vnum >= 8250 && vnum <= 8263) || (vnum >= 8270 && vnum <= 8283) ? true : false;
	}

	if (GetVnum() == 50645) //limites koho aqua
	{
		return (vnum >= 8780 && vnum <= 8789) || (vnum >= 8760 && vnum <= 8769) || (vnum >= 8790 && vnum <= 8799) ? true : false;//Fagyos aqua itemek
	}


	if (GetVnum() == 50646) //limites koho isteni
	{
		return (vnum >= 8730 && vnum <= 8739) || (vnum >= 8700 && vnum <= 8709) || (vnum >= 8780 && vnum <= 8789) ? true : false;//véres zodiák itemek
	}




	if (GetVnum() == 50643) //limites koho isteni
	{
		return (vnum >= 1740 && vnum <= 1753) || (vnum >= 1780 && vnum <= 1793) || (vnum >= 1800 && vnum <= 1813) ? true : false;
	}
	struct JewelAccessoryInfo
	{
		uint32_t jewel;
		uint32_t wrist;
		uint32_t neck;
		uint32_t ear;
	};
	const static JewelAccessoryInfo infos[] = {
		{ 50634, 14220, 16220, 17220 },
		{ 50635, 14500, 16500, 17500 },
		{ 50636, 14520, 16520, 17520 },
		{ 50637, 14540, 16540, 17540 },
		{ 50638, 14560, 16560, 17560 },
		{ 50639, 14570, 16570, 17570 },
		{ 50641, 8210, 8250, 8270 },
		{ 50645, 8780, 8760, 8790 },
		{ 50643, 1740, 1780, 1800 },
		{ 50646, 8730, 8720, 8730 },
	};

	uint32_t item_type = (item->GetVnum() / 10) * 10;
	for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++)
	{
		const JewelAccessoryInfo& info = infos[i];
		switch (item->GetSubType())
		{
		case ARMOR_WRIST:
			if (info.wrist == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		case ARMOR_NECK:
			if (info.neck == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		case ARMOR_EAR:
			if (info.ear == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		}
	}
	if (item->GetSubType() == ARMOR_WRIST)
		vnum -= 14000;
	else if (item->GetSubType() == ARMOR_NECK)
		vnum -= 16000;
	else if (item->GetSubType() == ARMOR_EAR)
		vnum -= 17000;
	else
		return false;

	uint32_t type = vnum / 20;

	if (type < 0 || type > 11)
	{
		type = (vnum - 170) / 20;

		if (50623 + type != GetVnum())
			return false;
		else
			return true;
	}
	else if (item->GetVnum() >= 16210 && item->GetVnum() <= 16219)
	{
		if (50625 != GetVnum())
			return false;
		else
			return true;
	}
	else if (item->GetVnum() >= 16230 && item->GetVnum() <= 16239)
	{
		if (50626 != GetVnum())
			return false;
		else
			return true;
	}

	return 50623 + type == GetVnum();
}

bool CItem::CanPutInto2(LPITEM item)
{
/*	if (item->GetType() == ITEM_BELT) {
		if (GetSubType() == USE_PUT_INTO_BELT_SOCKET && GetValue(0) == 1) {
			return true;
		}
		else {
			return false;
		}
	}

	else*/ if (item->GetType() == ITEM_RING)
		return CanPutIntoRing(item, this);

	else if (item->GetType() != ITEM_ARMOR)
		return false;

	uint32_t vnum = item->GetVnum();

	if (GetVnum() == 50684) {
		return (vnum >= 14220 && vnum <= 14233) || (vnum >= 16220 && vnum <= 16233) || (vnum >= 17220 && vnum <= 17233) ? true : false;
	}

	if (GetVnum() == 50690) {
		return (vnum >= 14580 && vnum <= 14589) || (vnum >= 15010 && vnum <= 15013) || (vnum >= 16580 && vnum <= 16593) || (vnum >= 17570 && vnum <= 17583) ? true : false;
	}

	if (GetVnum() == 50642) //perma koho aqua
	{
		return (vnum >= 8210 && vnum <= 8223) || (vnum >= 8250 && vnum <= 8263) || (vnum >= 8270 && vnum <= 8283) ? true : false;
	}


	if (GetVnum() == 50644) //perma koho isteni
	{
		return (vnum >= 1740 && vnum <= 1753) || (vnum >= 1780 && vnum <= 1793) || (vnum >= 1800 && vnum <= 1813) ? true : false;
	}

	struct JewelAccessoryInfo
	{
		uint32_t jewel;
		uint32_t wrist;
		uint32_t neck;
		uint32_t ear;
	};
	const static JewelAccessoryInfo infos[] = {
		{ 50684, 14220, 16220, 17220 },
		{ 50685, 14500, 16500, 17500 },
		{ 50686, 14520, 16520, 17520 },
		{ 50687, 14540, 16540, 17540 },
		{ 50688, 14560, 16560, 17560 },
		{ 50689, 14570, 16570, 17570 },
		{ 50642, 8210, 8250, 8270 },
		{ 50644, 1740, 1780, 1800 },
	};

	uint32_t item_type = (item->GetVnum() / 10) * 10;
	for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++)
	{
		const JewelAccessoryInfo& info = infos[i];
		switch (item->GetSubType())
		{
		case ARMOR_WRIST:
			if (info.wrist == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		case ARMOR_NECK:
			if (info.neck == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		case ARMOR_EAR:
			if (info.ear == item_type)
			{
				if (info.jewel == GetVnum())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			break;
		}
	}
	if (item->GetSubType() == ARMOR_WRIST)
		vnum -= 14000;
	else if (item->GetSubType() == ARMOR_NECK)
		vnum -= 16000;
	else if (item->GetSubType() == ARMOR_EAR)
		vnum -= 17000;
	else
		return false;

	uint32_t type = vnum / 20;

	if (type < 0 || type > 11)
	{
		type = (vnum - 170) / 20;

		if (50673 + type != GetVnum())
			return false;
		else
			return true;
	}
	else if (item->GetVnum() >= 16210 && item->GetVnum() <= 16219)
	{
		if (50675 != GetVnum())
			return false;
		else
			return true;
	}
	else if (item->GetVnum() >= 16230 && item->GetVnum() <= 16239)
	{
		if (50676 != GetVnum())
			return false;
		else
			return true;
	}

	return 50673 + type == GetVnum();
}

int32_t CItem::GetRuneAttrType(int c) {
	int32_t v = 0;
	uint8_t bSubType = GetSubType();
	if (bSubType == RUNE_SLOT1)
		v = c == 1 ? aApplyRuneInfo[1][0] : aApplyRuneInfo[0][0];
	else if (bSubType == RUNE_SLOT2)
		v = c == 1 ? aApplyRuneInfo[3][0] : aApplyRuneInfo[2][0];
	else if (bSubType == RUNE_SLOT3)
		v = c == 1 ? aApplyRuneInfo[5][0] : aApplyRuneInfo[4][0];
	else if (bSubType == RUNE_SLOT4)
		v = c == 1 ? aApplyRuneInfo[7][0] : aApplyRuneInfo[6][0];
	else if (bSubType == RUNE_SLOT5)
		v = c == 1 ? aApplyRuneInfo[9][0] : aApplyRuneInfo[8][0];
	else if (bSubType == RUNE_SLOT6)
		v = c == 1 ? aApplyRuneInfo[11][0] : aApplyRuneInfo[10][0];
	else if (bSubType == RUNE_SLOT7)
		v = c == 1 ? aApplyRuneInfo[13][0] : aApplyRuneInfo[12][0];

	return v;
}

int32_t CItem::GetRuneAttrValue(int c, int32_t lTime) {
	int32_t v = 0;
	int32_t t = 1;
	int32_t lMaxTime = GetValue(0);
	int32_t lOnePercent = lMaxTime / 100;
	int32_t lRemainPercent = lTime / lOnePercent;
	if (lRemainPercent >= 81)
		t = 7;
	else if (lRemainPercent >= 61)
		t = 6;
	else if (lRemainPercent >= 41)
		t = 5;
	else if (lRemainPercent >= 21)
		t = 4;
	else if (lRemainPercent >= 11)
		t = 3;
	else if (lRemainPercent >= 6)
		t = 2;
	else if (lRemainPercent >= 0)
		t = 1;

	uint8_t bSubType = GetSubType();
	if (bSubType == RUNE_SLOT1)
		v = c == 1 ? aApplyRuneInfo[1][t] : aApplyRuneInfo[0][t];
	else if (bSubType == RUNE_SLOT2)
		v = c == 1 ? aApplyRuneInfo[3][t] : aApplyRuneInfo[2][t];
	else if (bSubType == RUNE_SLOT3)
		v = c == 1 ? aApplyRuneInfo[5][t] : aApplyRuneInfo[4][t];
	else if (bSubType == RUNE_SLOT4)
		v = c == 1 ? aApplyRuneInfo[7][t] : aApplyRuneInfo[6][t];
	else if (bSubType == RUNE_SLOT5)
		v = c == 1 ? aApplyRuneInfo[9][t] : aApplyRuneInfo[8][t];
	else if (bSubType == RUNE_SLOT6)
		v = c == 1 ? aApplyRuneInfo[11][t] : aApplyRuneInfo[10][t];
	else if (bSubType == RUNE_SLOT7)
		v = c == 1 ? aApplyRuneInfo[13][t] : aApplyRuneInfo[12][t];

	return v;
}


CItem::CItem(uint32_t dwVnum)
	: m_pProto(nullptr), m_dwVnum(dwVnum), m_pOwner(nullptr), m_bWindow(0), m_dwID(0), m_bEquipped(false), m_dwVID(0),
	m_wCell(0),
	m_dwCount(0),
	m_sLockedAttr(0),
	m_ExtraProto(nullptr), m_lFlag(0), m_dwLastOwnerPID(0),
	m_bExchanging(false), m_pkDestroyEvent(nullptr), m_pkExpireEvent(nullptr),

#ifdef ENABLE_SOUL_SYSTEM
	m_pkSoulItemEvent(nullptr),
#endif

	m_pkUniqueExpireEvent(nullptr), m_pkTimerBasedOnWearExpireEvent(nullptr), m_pkRealTimeExpireEvent(nullptr),
	m_pkAccessorySocketExpireEvent(nullptr), m_pkOwnershipEvent(nullptr), m_dwOwnershipPID(0), m_bSkipSave(false),
	m_isLocked(false),
	m_dwMaskVnum(0), m_dwSIGVnum(0)
{
	memset(&m_alSockets, 0, sizeof(m_alSockets));
	memset(&m_aAttr, 0, sizeof(m_aAttr));
}

CItem::~CItem()
{
	Destroy();
}


EVENTFUNC(item_destroy_event)
{
	auto info = dynamic_cast<item_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("item_destroy_event> <Factor> Null pointer");
		return 0;
	}

	LPITEM pkItem = info->item;

	if (pkItem->GetOwner())
		sys_err("item_destroy_event: Owner exist. (item %s owner %s)", pkItem->GetName(), pkItem->GetOwner()->GetName());

	pkItem->SetDestroyEvent(nullptr);
	M2_DESTROY_ITEM(pkItem);
	return 0;
}

EVENTFUNC(ownership_event)
{
	auto info = dynamic_cast<item_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("ownership_event> <Factor> Null pointer");
		return 0;
	}

	LPITEM pkItem = info->item;

	pkItem->SetOwnershipEvent(nullptr);

	TPacketGCItemOwnership p;

	p.bHeader = HEADER_GC_ITEM_OWNERSHIP;
	p.dwVID = pkItem->GetVID();
	p.szName[0] = '\0';

	pkItem->PacketAround(&p, sizeof(p));
	return 0;
}

EVENTFUNC(unique_expire_event)
{
	auto info = dynamic_cast<item_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("unique_expire_event> <Factor> Null pointer");
		return 0;
	}

	LPITEM pkItem = info->item;

	if (pkItem->GetValue(2) == 0)
	{
		if (pkItem->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) <= 1)
		{
			sys_log(0, "UNIQUE_ITEM: expire %s %u", pkItem->GetName(), pkItem->GetID());
			pkItem->SetUniqueExpireEvent(nullptr);
			ITEM_MANAGER::instance().RemoveItem(pkItem, "UNIQUE_EXPIRE");
			return 0;
		}
		else
		{
			pkItem->SetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, pkItem->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - 1);
			return PASSES_PER_SEC(60);
		}
	}
	else
	{
		time_t cur = get_global_time();

		if (pkItem->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) <= cur)
		{
			pkItem->SetUniqueExpireEvent(nullptr);
			ITEM_MANAGER::instance().RemoveItem(pkItem, "UNIQUE_EXPIRE");
			return 0;
		}
		else
		{
			if (pkItem->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - cur < 600)
				return PASSES_PER_SEC(pkItem->GetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME) - cur);
			else
				return PASSES_PER_SEC(600);
		}
	}
}

EVENTFUNC(timer_based_on_wear_expire_event)
{
	auto info = dynamic_cast<item_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("expire_event <Factor> Null pointer");
		return 0;
	}

	LPITEM pkItem = info->item;
	int remain_time = pkItem->GetSocket(ITEM_SOCKET_REMAIN_SEC) - processing_time / passes_per_sec;
#ifdef ENABLE_RUNE_SYSTEM
	if (pkItem->IsRune()) {
		if (remain_time <= 0) {
			pkItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, 0);
			pkItem->DeactivateRune();
			return 0;
		}

		if (int32_t(remain_time / (pkItem->GetValue(0) / 100)) < 50) {
			pkItem->DeactivateRuneBonus();
		}

		if ((pkItem->GetSubType() == RUNE_SLOT7) || (pkItem->GetSocket(1) != 1))
			return PASSES_PER_SEC(MIN(60, remain_time));

		if (pkItem->GetSocket(1) == 1)
			pkItem->ChangeRuneAttr(remain_time);
	}
#endif

	if (remain_time <= 0)
	{
		sys_log(0, "ITEM EXPIRED : expired %s %u", pkItem->GetName(), pkItem->GetID());
		pkItem->SetTimerBasedOnWearExpireEvent(nullptr);
		pkItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, 0);

		if (pkItem->IsDragonSoul())
		{
			DSManager::instance().DeactivateDragonSoul(pkItem);
		}
		else
		{
			ITEM_MANAGER::instance().RemoveItem(pkItem, "TIMER_BASED_ON_WEAR_EXPIRE");
		}
		return 0;
	}

	pkItem->SetSocket(ITEM_SOCKET_REMAIN_SEC, remain_time);
	return PASSES_PER_SEC(MIN(60, remain_time));
}

EVENTFUNC(real_time_expire_event)
{
	auto info = reinterpret_cast<const item_vid_event_info*>(event->info);

	if (nullptr == info)
		return 0;

	const LPITEM item = ITEM_MANAGER::instance().FindByVID(info->item_vid);

	if (nullptr == item)
		return 0;

#ifdef ENABLE_NEW_USE_POTION
	if (info->newpotion) {
		int32_t remainSec = item->GetSocket(0);
		if (remainSec <= 0) {
			if (item->GetSocket(1) == 1) {
				auto* pkOwner = item->GetOwner();
				if (pkOwner) {
					if (pkOwner->FindAffect(item->GetValue(0))) {
						pkOwner->RemoveAffect(item->GetValue(0));
					}

#ifdef TEXTS_IMPROVEMENT
					pkOwner->ChatPacketNew(CHAT_TYPE_INFO, 27, "%s", item->GetName());
#endif
				}
			}

			ITEM_MANAGER::instance().RemoveItem(item, "REAL_TIME_EXPIRE");
			return 0;
		}

		if (item->GetSocket(1) != 1) {
			return PASSES_PER_SEC(60);
		}
		else {
			int32_t nextSec = (remainSec - 60) > 0 ? (remainSec - 60) : 0;
			item->SetSocket(0, nextSec);
			if (nextSec <= 0) {
				auto* pkOwner = item->GetOwner();
				if (pkOwner) {
					if (pkOwner->FindAffect(item->GetValue(0))) {
						pkOwner->RemoveAffect(item->GetValue(0));
					}

#ifdef TEXTS_IMPROVEMENT
					pkOwner->ChatPacketNew(CHAT_TYPE_INFO, 27, "%s", item->GetName());
#endif
				}

				ITEM_MANAGER::instance().RemoveItem(item, "REAL_TIME_EXPIRE");
				return 0;
			}
			else {
				return PASSES_PER_SEC(nextSec > 60 ? 60 : nextSec);
			}
		}
	}
#endif

	const time_t current = get_global_time();
	if (current > item->GetSocket(0))
	{
		auto* pkOwner = item->GetOwner();

		if (pkOwner && pkOwner->GetDesc() && item->GetWindow() == MOUNT_INVENTORY)
		{
			TPacketGCWhisper pack;
			char msg[CHAT_MAX_LEN + 1];

			const int len = snprintf(msg, sizeof(msg), "Mount expired in mountinventory: %s", item->GetName());

			pack.bHeader = HEADER_GC_WHISPER;
			pack.bType = WHISPER_TYPE_SYSTEM;
			pack.wSize = static_cast<uint16_t>(sizeof(TPacketGCWhisper) + len + 1);
			strlcpy(pack.szNameFrom, "[MountInventory]", sizeof(pack.szNameFrom));

			pkOwner->GetDesc()->BufferedPacket(&pack, sizeof(pack));
			pkOwner->GetDesc()->Packet(msg, len + 1);
		}

		if (item->IsNewMountItem()) {
			if (item->GetSocket(2) != 0)
				item->ClearMountAttributeAndAffect();
		}

		ITEM_MANAGER::instance().RemoveItem(item, "REAL_TIME_EXPIRE");
		return 0;
	}

	return PASSES_PER_SEC(1);
}

EVENTFUNC(accessory_socket_expire_event)
{
	item_vid_event_info* info = dynamic_cast<item_vid_event_info*>(event->info);

	if (info == nullptr)
	{
		sys_err("accessory_socket_expire_event> <Factor> Null pointer");
		return 0;
	}

	LPITEM item = ITEM_MANAGER::instance().FindByVID(info->item_vid);
	if (item->GetAccessorySocketDownGradeTime() <= 1)
	{
	degrade:
		item->SetAccessorySocketExpireEvent(nullptr);
		item->AccessorySocketDegrade();
		return 0;
	}
	else
	{
		int iTime = item->GetAccessorySocketDownGradeTime() - 60;

		if (iTime <= 1)
			goto degrade;

		item->SetAccessorySocketDownGradeTime(iTime);

		if (iTime > 60)
			return PASSES_PER_SEC(60);
		else
			return PASSES_PER_SEC(iTime);
	}
}

#ifdef ENABLE_SOUL_SYSTEM
EVENTFUNC(soul_item_event)
{
	const item_vid_event_info* pInfo = reinterpret_cast<item_vid_event_info*>(event->info);
	if (!pInfo)
		return 0;

	const LPITEM pItem = ITEM_MANAGER::instance().FindByVID(pInfo->item_vid);
	if (!pItem)
		return 0;

	int iCurrentMinutes = (pItem->GetSocket(2) / 10000);
	int iCurrentStrike = (pItem->GetSocket(2) % 10000);
	int iNextMinutes = iCurrentMinutes + 1;

	if (iNextMinutes >= pItem->GetLimitValue(1))
	{
		if (pItem->GetValue(0) != 1)
		{
			pItem->SetSocket(2, (pItem->GetLimitValue(1) * 10000 + iCurrentStrike)); // just in case
			pItem->SetSoulItemEvent(nullptr);
			return 0;
		}
	}

	pItem->SetSocket(2, (iNextMinutes * 10000 + iCurrentStrike));

	if (test_server)
		return PASSES_PER_SEC(5);

	return PASSES_PER_SEC(60);
}


#endif

