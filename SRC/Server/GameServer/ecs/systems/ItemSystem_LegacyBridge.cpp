#include "../../core/stdafx.h"
#include "../../item/ItemStore.hpp"
#include "../../social/exchange.h"
#include "ViewSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "AffectSystem.hpp"
#include "ActivitySystem.hpp"

#include "ItemSystem.hpp"
#include "../../item/TypedMoney.hpp"
#include "MovementSystem.hpp"
#include "SkillSystem.hpp"
#include "SocialSystem.hpp"
#include "SessionSystem.hpp"
#include "InventorySystem.hpp"
#include "CombatSystem.hpp"
#include "MountSystem.hpp"
#include "QuestSystem.hpp"
#include "PointSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "../EntityFactory.hpp"
#include "../VIDRegistry.hpp"
#include "../ResourceAdmission.hpp"

#include "../../core/utils.h"
#include "../../core/config.h"
#include "../../entity/char.h"
#include "../../entity/char_manager.h"
#include "../../item/item_manager.h"
#include "../../network/desc.h"
#include "../../network/desc_client.h"
#include "../../network/desc_manager.h"
#include "../../core/packet.h"
#include "../../core/protocol.h"
#include "../../combat/affect.h"
#include "../../combat/skill.h"
#include "../../world/start_position.h"
#include "../../entity/mob_manager.h"
#include "../../network/db.h"
#include "../../core/log.h"
#include "../../core/vector.h"
#include "../../core/buffer_manager.h"
#include "../../quest/questmanager.h"
#include "../../world/fishing.h"
#include "../../social/party.h"
#include "../../dungeon/dungeon.h"
#include "../../item/refine.h"
#include "../../item/unique_item.h"
#include "../../world/war_map.h"
#include "../../social/marriage.h"
#include "../../world/BattleArena.h"
#include "../../world/arena.h"
#include "../../core/dev_log.h"
#include <Core/Logging.hpp>
#include "../../../common/VnumHelper.h"
#include "../../item/belt_inventory_helper.h"
#include "../../item/item.h"
#include "../../item/item_manager.h"
#include "../../social/shop.h"
#include "../../item/safebox.h"
#ifdef ENABLE_SWITCHBOT
#include "../../social/new_switchbot.h"
#endif
#include "../../combat/buff_on_attributes.h"
#include "../../item/ItemUse.h"

#ifdef __NEWPET_SYSTEM__
#include "../../pet/New_PetSystem.h"
#define __NEWPET_SYSTEM_CHECK
#endif
#ifdef __PET_SYSTEM__
#endif
#ifdef ENABLE_NEWSTUFF
#include "../../combat/pvp.h"
#endif
#include "../../../common/CommonDefines.h"
#ifdef ENABLE_RUNE_SYSTEM
#include "../../../common/rune_length.h"
#endif

#include "../Registry.hpp"
#include "../SpatialHelpers.hpp"
#include "../components/identity_components.hpp"
#include "../components/inventory_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"
#include "../components/item_components.hpp"
#include "../ItemRegistry.hpp"
#include "../CharacterAccessors.hpp"
#include "../components/social_components.hpp"

bool IS_SUMMONABLE_ZONE(int map_index);
bool IS_BOTARYABLE_ZONE(int nMapIndex);
extern int stone_chance;

namespace {

const int ITEM_BROKEN_METIN_VNUM = 28960;


struct FFindStone
{
	// The stones found in range. Only their positions are ever read, so
	// there is nothing here a CHARACTER pointer answers that the entity
	// does not.
	std::map<uint32_t, entt::entity> m_mapStone;

	void operator()(entt::entity pChar)
	{
		if (ecs::IsCharacter(pChar) == true)
		{

			if (ecs::PlayerRuntime::IsStone(pChar))
			{
				m_mapStone[ecs::PlayerRuntime::GetPacketVID(pChar)] = pChar;
			}
		}
	}
};


static ecs::MainInventoryRuntimeComponent* EnsureMainInventoryRuntimeComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(character))
        return comp;

    return &g_registry.emplace<ecs::MainInventoryRuntimeComponent>(character);
}

#ifdef ENABLE_EXTRA_INVENTORY
static ecs::ExtraInventoryRuntimeComponent* EnsureExtraInventoryRuntimeComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(character))
        return comp;

    return &g_registry.emplace<ecs::ExtraInventoryRuntimeComponent>(character);
}

#endif

#ifdef ENABLE_SWITCHBOT
static ecs::SwitchbotRuntimeComponent* EnsureSwitchbotRuntimeComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::SwitchbotRuntimeComponent>(character))
        return comp;

    return &g_registry.emplace<ecs::SwitchbotRuntimeComponent>(character);
}

#endif

} // namespace

EVENTFUNC(unique_expire_event);
EVENTFUNC(timer_based_on_wear_expire_event);
EVENTFUNC(real_time_expire_event);
EVENTFUNC(accessory_socket_expire_event);
EVENTFUNC(soul_item_event);

// Remaining character-side item workflows. Items are native entities.

#ifdef ENABLE_EXTRA_INVENTORY


#ifdef ENABLE_ACCE_SYSTEM
#endif

#endif

namespace ecs::PlayerRuntime {

void SetWear(entt::entity e, uint8_t bCell, entt::entity item)
{
	if (bCell >= WEAR_MAX_NUM)
	{
		LOG_ERROR("SetWear: invalid item cell {}", bCell);
		return;
	}

#ifdef __HIGHLIGHT_SYSTEM__
	SetItem(e, TItemPos(EQUIPMENT, bCell), item, false);
#else
	SetItem(e, TItemPos(EQUIPMENT, bCell), item);
#endif

#ifndef ENABLE_BUG_FIXES
	if (item == entt::null && bCell == WEAR_WEAPON) {
		if (AffectSystem::IsAffectFlag(e, AFF_GWIGUM))
			AffectSystem::RemoveAffect(e, SKILL_GWIGEOM);

		if (AffectSystem::IsAffectFlag(e, AFF_GEOMGYEONG))
			AffectSystem::RemoveAffect(e, SKILL_GEOMKYUNG);
	}
#endif
}

} // namespace ecs::PlayerRuntime


// char_item.cpp slice C1 moved into ItemSystem.cpp

namespace NPartyPickupDistribute
{
	struct FCountNearMember
	{
		int		total;
		int		x, y;

		explicit FCountNearMember(entt::entity center)
			: total(0), x(ecs::PlayerRuntime::GetX(center)), y(ecs::PlayerRuntime::GetY(center))
		{
		}

		void operator () (entt::entity member)
		{
			if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(member) - x, ecs::PlayerRuntime::GetY(member) - y) <= PARTY_DEFAULT_RANGE)
				total += 1;
		}
	};

	struct FMoneyDistributor
	{
		int		total;
		entt::entity	c;
		int		x, y;
		int64_t		iMoney;

		FMoneyDistributor(entt::entity center, int64_t iMoney)
			: total(0), c(center), x(ecs::PlayerRuntime::GetX(center)), y(ecs::PlayerRuntime::GetY(center)), iMoney(iMoney)
		{
		}

		void operator ()(entt::entity member)
		{
			if (member != c)
				if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(member) - x, ecs::PlayerRuntime::GetY(member) - y) <= PARTY_DEFAULT_RANGE)
				{
					ecs::PointSystem::Change(member, POINT_GOLD, iMoney, true);

					if (iMoney > 1000)
					{
						LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(member, iMoney, "GET_GOLD", ""));
					}
				}
		}
	};
}

// Player drop transactions live with the native inventory commit engine.

// char_item.cpp slice C2a moved into ItemSystem.cpp

// char_item.cpp slice C2b moved into ItemSystem.cpp

EVENTFUNC(kill_campfire_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>(event->info);

	if (info == nullptr)
	{
		LOG_ERROR("kill_campfire_event> <Factor> Null pointer");
		return 0;
	}

	if (!ecs::IsCharacter(info->ch)) { // <Factor>
		return 0;
	}
	ecs::PlayerRuntime::SetCharEvent(
		info->ch, ecs::PlayerRuntime::CharEvent::Mining, nullptr);
	CHARACTER_MANAGER::instance().DestroyCharacter(info->ch);
	return 0;
}

int g_nPortalLimitTime = 10;

const uint8_t g_aBuffOnAttrPoints[] = { POINT_ENERGY, POINT_COSTUME_ATTR_BONUS };

#ifdef ENABLE_PVP_ADVANCED
#endif




bool IS_SUMMONABLE_ZONE(int map_index)
{
	switch (map_index)
	{
	case 66:
	case 71:
	case 72:
	case 73:
	case 193:
	case 216:
	case 217:
	case 208:

	case 113:
		return false;
	}

	if (CBattleArena::IsBattleArenaMap(map_index)) return false;

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




/////////////////////////////////////////////////////////////////////////////
// ITEM HANDLING
/////////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_EXTRA_INVENTORY
#endif

namespace ecs::PlayerRuntime {

#ifdef __HIGHLIGHT_SYSTEM__
void SetItem(entt::entity e, TItemPos Cell, entt::entity itemEntity, bool isHighLight)
#else
void SetItem(entt::entity e, TItemPos Cell, entt::entity itemEntity)
#endif
{
	uint16_t wCell = Cell.cell;
	uint8_t window_type = Cell.window_type;
	// The 0xff / 0xffffffff pointer sentinel check is gone with the pointer.
	// An entt::entity cannot hold a scribbled pointer value, and an invalid
	// one is caught by IsValidItem below rather than by core_dump().
	const bool hasItem = itemEntity != entt::null && ItemSystem::IsValidItem(itemEntity);
	if (itemEntity != entt::null && !hasItem)
	{
		LOG_ERROR("PlayerRuntime::SetItem: item entity {} is not a valid item (char: {} cell: {})",
			static_cast<uint32_t>(itemEntity), ecs::PlayerRuntime::GetName(e), wCell);
		return;
	}

	if (hasItem && ItemSystem::GetItemOwner(itemEntity) != entt::null)
	{
		assert(!"GetOwner exist");
		return;
	}
	switch (window_type)
	{
	case INVENTORY:
	{
		const uint16_t storageCell = wCell;
		if (!Cell.IsValidItemPosition())
		{
			LOG_ERROR("PlayerRuntime::SetItem: invalid item cell {}", storageCell);
			return;
		}

		auto* pMainInventory = EnsureMainInventoryRuntimeComponent(e);
		if (!pMainInventory)
		{
			LOG_ERROR("PlayerRuntime::SetItem: missing MainInventoryRuntimeComponent");
			return;
		}

		const entt::entity pOld = pMainInventory->items[storageCell];

		if (pOld != entt::null)
		{
			if (storageCell < INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < ItemSystem::GetItemSize(pOld); ++i)
				{
					int p = storageCell + (i * 5);

					if (p >= INVENTORY_MAX_NUM)
						continue;

					if (pMainInventory->items[p] != entt::null && pMainInventory->items[p] != pOld)
						continue;

					pMainInventory->itemGrid[p] = 0;
				}
			}
			else
				pMainInventory->itemGrid[storageCell] = 0;
		}

		if (hasItem)
		{
			if (storageCell < INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < ItemSystem::GetItemSize(itemEntity); ++i)
				{
					int p = storageCell + (i * 5);

					if (p >= INVENTORY_MAX_NUM)
						continue;

					pMainInventory->itemGrid[p] = storageCell + 1;
				}
			}
			else
				pMainInventory->itemGrid[storageCell] = storageCell + 1;
		}

		pMainInventory->items[storageCell] = itemEntity;
	}
	break;
	case EQUIPMENT:
	{
		const uint16_t storageCell = static_cast<uint16_t>(INVENTORY_MAX_NUM + wCell);
		if (storageCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
		{
			LOG_ERROR("PlayerRuntime::SetItem: invalid equipment item cell {}", wCell);
			return;
		}

		auto* pMainInventory = EnsureMainInventoryRuntimeComponent(e);
		if (!pMainInventory)
		{
			LOG_ERROR("PlayerRuntime::SetItem: missing MainInventoryRuntimeComponent");
			return;
		}

		const entt::entity pOld = pMainInventory->items[storageCell];

		if (pOld != entt::null)
			pMainInventory->itemGrid[storageCell] = 0;

		if (hasItem)
			pMainInventory->itemGrid[storageCell] = storageCell + 1;

		pMainInventory->items[storageCell] = itemEntity;
	}
	break;
#ifdef ENABLE_EXTRA_INVENTORY
	case EXTRA_INVENTORY:
	{
		if (wCell >= EXTRA_INVENTORY_MAX_NUM)
		{
			LOG_ERROR("PlayerRuntime::SetItem: invalid EXTRA item cell {}", wCell);
			return;
		}

		auto* pExtraInventory = EnsureExtraInventoryRuntimeComponent(e);
		if (!pExtraInventory)
		{
			LOG_ERROR("PlayerRuntime::SetItem: missing ExtraInventoryRuntimeComponent");
			return;
		}

		const entt::entity pOld = pExtraInventory->items[wCell];

		if (pOld != entt::null)
		{

			if (wCell < EXTRA_INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < ItemSystem::GetItemSize(pOld); ++i)
				{
					int p = wCell + (i * EXTRA_INVENTORY_PAGE_COLUMN);

					if (p >= EXTRA_INVENTORY_MAX_NUM)
						continue;

					if (pExtraInventory->items[p] != entt::null && pExtraInventory->items[p] != pOld)
						continue;

					pExtraInventory->itemGrid[p] = 0;
				}
			}
			else
				pExtraInventory->itemGrid[wCell] = 0;
		}

		if (hasItem)
		{
			if (wCell < EXTRA_INVENTORY_MAX_NUM)
			{
				for (int i = 0; i < ItemSystem::GetItemSize(itemEntity); ++i)
				{
					int p = wCell + (i * EXTRA_INVENTORY_PAGE_COLUMN);

					if (p >= EXTRA_INVENTORY_MAX_NUM)
						continue;

					pExtraInventory->itemGrid[p] = wCell + 1;
				}
			}
			else
				pExtraInventory->itemGrid[wCell] = wCell + 1;
		}

		pExtraInventory->items[wCell] = itemEntity;
	}
	break;
#endif

#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
	{
		const entt::entity oldItem = ItemSystem::GetItem(e, TItemPos(SWITCHBOT, wCell));
		if (hasItem && oldItem != entt::null)
		{
			return;
		}

		if (wCell >= SWITCHBOT_SLOT_COUNT)
		{
			LOG_ERROR("PlayerRuntime::SetItem: invalid switchbot item cell {}", wCell);
			return;
		}

		if (hasItem)
		{
			CSwitchbotManager::Instance().RegisterItem(ecs::PlayerRuntime::GetPlayerID(e), ItemSystem::GetItemID(itemEntity), wCell);
		}
		else
		{
			CSwitchbotManager::Instance().UnregisterItem(ecs::PlayerRuntime::GetPlayerID(e), wCell);
		}

		if (auto* switchbot = EnsureSwitchbotRuntimeComponent(e))
			switchbot->items[wCell] = itemEntity;
	}
	break;
#endif
	default:
		LOG_ERROR("Invalid Inventory type {}", window_type);
		return;
	}

	TItemPos packetCell = Cell;
	if (window_type == EQUIPMENT)
		packetCell = TItemPos(EQUIPMENT, static_cast<uint16_t>(INVENTORY_MAX_NUM + wCell));

	if (ecs::PlayerRuntime::GetDesc(e))
	{
		if (hasItem)
		{
			TPacketGCItemSet pack;
			pack.header = HEADER_GC_ITEM_SET;
			pack.Cell = packetCell;

			pack.count = ItemSystem::GetItemCount(itemEntity);
#ifdef ATTR_LOCK
			pack.lockedattr = ItemSystem::GetItemLockedAttributeIndex(itemEntity);
#endif
			pack.vnum = ItemSystem::GetItemVnum(itemEntity);
			pack.flags = ItemSystem::GetItemFlags(itemEntity);
			pack.anti_flags = ItemSystem::GetItemAntiFlag(itemEntity);
#ifdef __HIGHLIGHT_SYSTEM__
			pack.highlight = isHighLight;
#else
			pack.highlight = false;
#endif

			// Per index rather than memcpy: the components are the source now, and
			// they are not laid out as one block behind a pointer.
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				pack.alSockets[i] = ItemSystem::GetItemSocket(itemEntity, i);
			for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				pack.aAttr[i] = ItemSystem::GetItemAttribute(itemEntity, i);

			ecs::PlayerRuntime::GetDesc(e)->Packet(&pack, sizeof(TPacketGCItemSet));
		}
		else
		{
			TPacketGCItemDelDeprecated pack;
			pack.header = HEADER_GC_ITEM_DEL;
			pack.Cell = packetCell;
			pack.count = 0;
#ifdef ATTR_LOCK
			pack.lockedattr = -1;
#endif
			pack.vnum = 0;
			memset(pack.alSockets, 0, sizeof(pack.alSockets));
			memset(pack.aAttr, 0, sizeof(pack.aAttr));

			ecs::PlayerRuntime::GetDesc(e)->Packet(&pack, sizeof(TPacketGCItemDelDeprecated));
		}
	}

	if (hasItem)
	{
		const uint16_t storageCell = (window_type == EQUIPMENT)
			? static_cast<uint16_t>(INVENTORY_MAX_NUM + wCell)
			: wCell;
		ItemSystem::SetItemCell(itemEntity, e, storageCell);
		switch (window_type)
		{
		case INVENTORY:
			if (wCell >= BELT_INVENTORY_SLOT_START && wCell < BELT_INVENTORY_SLOT_END)
			{
				if (CBeltInventoryHelper::CanMoveIntoBeltInventory(itemEntity))
					ItemSystem::SetItemWindow(itemEntity, INVENTORY);
				else
					ItemSystem::SetItemWindow(itemEntity, EQUIPMENT); // vagy return is lehet, ha nem engedelyezett
			}
			else if (wCell < INVENTORY_MAX_NUM)
			{
				ItemSystem::SetItemWindow(itemEntity, INVENTORY);
			}
			else
			{
				ItemSystem::SetItemWindow(itemEntity, EQUIPMENT);
			}

			break;
		case EQUIPMENT:
			ItemSystem::SetItemWindow(itemEntity, EQUIPMENT);
			break;
#ifdef ENABLE_EXTRA_INVENTORY
		case EXTRA_INVENTORY:
			ItemSystem::SetItemWindow(itemEntity, EXTRA_INVENTORY);
			break;
#endif
#ifdef ENABLE_SWITCHBOT
		case SWITCHBOT:
			ItemSystem::SetItemWindow(itemEntity, SWITCHBOT);
			break;
#endif
		}
	}
}


} // namespace ecs::PlayerRuntime
void InventorySystem::ClearItem(entt::entity e)
{
	int		i;
	entt::entity item;

	for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
	{
		if (ItemSystem::IsValidItem(item = ItemSystem::GetInventoryItem(e, i)))
		{
			ItemSystem::SetItemSkipSave(item, true);

			InventorySystem::RemoveFromCharacter(item);
			ItemSystem::DestroyItemEntityEcs(
				item,
				"CLEAR_ITEM_INVENTORY");

			InventorySystem::SyncQuickslot(e, QUICKSLOT_TYPE_ITEM, i, 255);
		}
	}
#ifdef ENABLE_EXTRA_INVENTORY
	for (i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
	{
		if (ItemSystem::IsValidItem(item = ItemSystem::GetExtraInventoryItem(e, i)))
		{
			ItemSystem::SetItemSkipSave(item, true);

			InventorySystem::RemoveFromCharacter(item);
			ItemSystem::DestroyItemEntityEcs(
				item,
				"CLEAR_ITEM_EXTRA_INVENTORY");

			InventorySystem::SyncQuickslot(e, QUICKSLOT_TYPE_ITEM_EXTRA, i, 255);
		}
	}
#endif

#ifdef ENABLE_SWITCHBOT
	for (i = 0; i < SWITCHBOT_SLOT_COUNT; ++i)
	{
		if (ItemSystem::IsValidItem(item = ItemSystem::GetItem(e, TItemPos(SWITCHBOT, i))))
		{
			ItemSystem::SetItemSkipSave(item, true);

			InventorySystem::RemoveFromCharacter(item);
			ItemSystem::DestroyItemEntityEcs(
				item,
				"CLEAR_ITEM_SWITCHBOT");
		}
	}
#endif
}


#ifdef ENABLE_RECALL
void AffectSystem::AutoRecallProcess(entt::entity e)
{
	const auto* descriptor = ecs::PlayerRuntime::GetDesc(e);
	if (!descriptor || !descriptor->IsPhase(PHASE_GAME) || !ecs::PlayerRuntime::GetSectree(e)
		|| !InventorySystem::IsItemLoaded(e))
		return;

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	// ENABLE_01092021 disables the old LoadAffect mount-restore branch.
	// Restore only after inventory, placement and the GAME phase are ready.
	if (ecs::PlayerRuntime::GetMapIndex(e) != 113 &&
		!CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)))
		MountSystem::CheckMount(e);
#endif

#ifdef __NEWPET_SYSTEM__
	{
		const CAffect* pAffect = AffectSystem::FindAffect(e, AFFECT_RECALL2);
		if (pAffect) {
			const entt::entity pItem = ItemSystem::FindItemByID(e, pAffect->dwFlag);
			if (ItemSystem::IsValidItem(pItem)) {
				// The saved growth-pet active marker is not a live actor handle.
				if (NewPetSystem::CountSummoned(e) == 0)
					NewPetSystem::Summon(e, ItemSystem::GetItemValue(pItem, 0), pItem, "", false);
			}
			else
				AffectSystem::RemoveAffect(e, const_cast<CAffect*>(pAffect));
		}
	}
#endif
}
#endif

#

EVENTFUNC(unique_expire_event)
{
	auto info = dynamic_cast<item_event_info*>(event->info);

	if (info == nullptr)
	{
		LOG_ERROR("unique_expire_event> <Factor> Null pointer");
		return 0;
	}

	const entt::entity itemEntity = info->item;
	if (!ItemSystem::IsValidItem(itemEntity))
		return 0;

	if (ItemSystem::GetItemValue(itemEntity, 2) == 0)
	{
		if (ItemSystem::GetItemSocket(itemEntity, ITEM_SOCKET_UNIQUE_REMAIN_TIME) <= 1)
		{
			LOG_INFO("UNIQUE_ITEM: expire {} {}", ItemSystem::GetItemName(itemEntity),
				ItemSystem::GetItemID(itemEntity));
			ItemSystem::GetItemEvents(itemEntity).uniqueExpire = nullptr;
			ITEM_MANAGER::instance().RemoveItem(itemEntity, "UNIQUE_EXPIRE");
			return 0;
		}
		else
		{
			ItemSystem::SetItemSocket(itemEntity, ITEM_SOCKET_UNIQUE_REMAIN_TIME,
				ItemSystem::GetItemSocket(itemEntity, ITEM_SOCKET_UNIQUE_REMAIN_TIME) - 1);
			return PASSES_PER_SEC(60);
		}
	}
	else
	{
		time_t cur = get_global_time();

		if (ItemSystem::GetItemSocket(itemEntity, ITEM_SOCKET_UNIQUE_REMAIN_TIME) <= cur)
		{
			ItemSystem::GetItemEvents(itemEntity).uniqueExpire = nullptr;
			ITEM_MANAGER::instance().RemoveItem(itemEntity, "UNIQUE_EXPIRE");
			return 0;
		}
		else
		{
			const time_t remaining =
				static_cast<time_t>(ItemSystem::GetItemSocket(itemEntity, ITEM_SOCKET_UNIQUE_REMAIN_TIME)) - cur;
			if (remaining < 600)
				return PASSES_PER_SEC(remaining);
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
		LOG_ERROR("expire_event <Factor> Null pointer");
		return 0;
	}

	const entt::entity itemEntity = info->item;
	if (!ItemSystem::IsValidItem(itemEntity))
		return 0;

#ifdef ENABLE_RUNE_SYSTEM
	if (ItemSystem::IsRuneItem(itemEntity)) {
		const int delay = ItemSystem::UpdateRuneWearTime(itemEntity, processing_time / passes_per_sec);
		if (delay == 0 && ItemSystem::IsValidItem(itemEntity)) {
			if (auto* events = g_registry.try_get<ecs::ItemEvents>(itemEntity);
				events && events->timerBasedOnWearExpire == event)
				events->timerBasedOnWearExpire = nullptr;
		}
		return PASSES_PER_SEC(delay);
	}
#endif
	int remain_time = static_cast<int>(ItemSystem::GetItemSocket(itemEntity, ITEM_SOCKET_REMAIN_SEC))
		- processing_time / passes_per_sec;

	if (remain_time <= 0)
	{
		LOG_INFO("ITEM EXPIRED : expired {} {}", ItemSystem::GetItemName(itemEntity),
			ItemSystem::GetItemID(itemEntity));
		ItemSystem::GetItemEvents(itemEntity).timerBasedOnWearExpire = nullptr;
		ItemSystem::SetItemSocket(itemEntity, ITEM_SOCKET_REMAIN_SEC, 0);

		{
			ITEM_MANAGER::instance().RemoveItem(itemEntity, "TIMER_BASED_ON_WEAR_EXPIRE");
		}
		return 0;
	}

	ItemSystem::SetItemSocket(itemEntity, ITEM_SOCKET_REMAIN_SEC, remain_time);
	return PASSES_PER_SEC(MIN(60, remain_time));
}

EVENTFUNC(real_time_expire_event)
{
	auto info = reinterpret_cast<const item_vid_event_info*>(event->info);

	if (nullptr == info)
		return 0;

	const entt::entity item = info->item;
	if (!ItemSystem::IsValidItem(item))
		return 0;

#ifdef ENABLE_NEW_USE_POTION
	if (info->newpotion) {
		int32_t remainSec = static_cast<int32_t>(ItemSystem::GetItemSocket(item, 0));
		if (remainSec <= 0) {
			if (ItemSystem::GetItemSocket(item, 1) == 1) {
				const entt::entity pkOwner = ItemSystem::GetItemOwnerEntity(item);
				if (pkOwner != entt::null) {
					if (AffectSystem::FindAffect(pkOwner, ItemSystem::GetItemValue(item, 0))) {
						AffectSystem::RemoveAffect(pkOwner, ItemSystem::GetItemValue(item, 0));
					}

#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(pkOwner, CHAT_TYPE_INFO, 27, "%s", ItemSystem::GetItemName(item));
#endif
				}
			}

			ITEM_MANAGER::instance().RemoveItem(item, "REAL_TIME_EXPIRE");
			return 0;
		}

		if (ItemSystem::GetItemSocket(item, 1) != 1) {
			return PASSES_PER_SEC(60);
		}
		else {
			int32_t nextSec = (remainSec - 60) > 0 ? (remainSec - 60) : 0;
			ItemSystem::SetItemSocket(item, 0, nextSec);
			if (nextSec <= 0) {
				const entt::entity pkOwner = ItemSystem::GetItemOwnerEntity(item);
				if (pkOwner != entt::null) {
					if (AffectSystem::FindAffect(pkOwner, ItemSystem::GetItemValue(item, 0))) {
						AffectSystem::RemoveAffect(pkOwner, ItemSystem::GetItemValue(item, 0));
					}

#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(pkOwner, CHAT_TYPE_INFO, 27, "%s", ItemSystem::GetItemName(item));
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
	if (current > ItemSystem::GetItemSocket(item, 0))
	{
		if (ItemSystem::IsNewMountItem(item)) {
			if (ItemSystem::GetItemSocket(item, 2) != 0)
				ItemSystem::ClearMountAttributeAndAffect(item);
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
		LOG_ERROR("accessory_socket_expire_event> <Factor> Null pointer");
		return 0;
	}

	if (!ItemSystem::IsValidItem(info->item))
		return 0;
	if (ItemSystem::GetItemAccessorySocketDownGradeTime(info->item) <= 1)
	{
	degrade:
		ItemSystem::GetItemEvents(info->item).accessorySocketExpire = nullptr;
		ItemSystem::AccessorySocketDegrade(info->item);
		return 0;
	}
	else
	{
		int iTime = ItemSystem::GetItemAccessorySocketDownGradeTime(info->item) - 60;

		if (iTime <= 1)
			goto degrade;

		ItemSystem::SetItemAccessorySocketDownGradeTime(info->item, iTime);

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

	const entt::entity item = pInfo->item;
	if (!ItemSystem::IsValidItem(item))
		return 0;

	const int iCurrentMinutes = static_cast<int>(ItemSystem::GetItemSocket(item, 2) / 10000);
	const int iCurrentStrike = static_cast<int>(ItemSystem::GetItemSocket(item, 2) % 10000);
	int iNextMinutes = iCurrentMinutes + 1;

	if (iNextMinutes >= ItemSystem::GetItemLimitValue(item, 1))
	{
		if (ItemSystem::GetItemValue(item, 0) != 1)
		{
			ItemSystem::SetItemSocket(item, 2, ItemSystem::GetItemLimitValue(item, 1) * 10000 + iCurrentStrike);
			ItemSystem::GetItemEvents(item).soulItem = nullptr;
			return 0;
		}
	}

	ItemSystem::SetItemSocket(item, 2, iNextMinutes * 10000 + iCurrentStrike);

	if (test_server)
		return PASSES_PER_SEC(5);

	return PASSES_PER_SEC(60);
}


#endif

namespace InventorySystem {

// The first inventory cell an item of this size fits in, or -1. The belt
// is a special inventory and is deliberately not searched.
int GetEmptyInventory(entt::entity e, uint8_t size)
{
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	const int inventoryLimit = std::min(GetInventorySize(e), (int)INVENTORY_MAX_NUM);
	for (int i = 0; i < inventoryLimit; ++i)
#else
	for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
#endif
		if (IsEmptyItemGrid(e, TItemPos(INVENTORY, i), size))
			return i;
	return -1;
}

} // namespace InventorySystem

namespace InventorySystem {

// Whether a window and cell pair names a slot this character has. Only
// the safebox and the mall arms need the character at all; the rest is a
// range check that never touched CHARACTER state.
bool IsValidItemPosition(entt::entity owner, TItemPos Pos)
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

#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
		return cell < SWITCHBOT_SLOT_COUNT;
#endif
	case SAFEBOX:
		if (const auto storage = SafeboxSystem::Get(owner, SAFEBOX); storage != entt::null)
			return SafeboxSystem::IsValidPosition(storage, cell);
		else
			return false;

	case MALL:
		if (const auto storage = SafeboxSystem::Get(owner, MALL); storage != entt::null)
			return SafeboxSystem::IsValidPosition(storage, cell);
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

} // namespace InventorySystem

namespace ItemSystem {

// Handing gold to a character, and crediting the farm-yang mission.
void GiveGold(entt::entity e, int64_t amount)
{
    if (amount > 0 && g_registry.valid(e) && g_registry.all_of<ecs::TypedItems>(e))
        QueueTypedGold(e, static_cast<std::uint64_t>(amount));
}

} // namespace ItemSystem
