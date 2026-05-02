#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"

#include "ItemSystem.hpp"
#include "../EntityFactory.hpp"
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

			if (ecs::PlayerRuntime::IsStone(AIHelpers::EcsOf(pChar)) == true)
			{
				m_mapStone[ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(pChar))] = pChar;
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

static ecs::MainInventoryRuntimeComponent* EnsureMainInventoryRuntimeComponent(LPCHARACTER ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::MainInventoryRuntimeComponent>(e);
}

static const ecs::MainInventoryRuntimeComponent* TryGetMainInventoryRuntimeComponent(const CHARACTER* ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::MainInventoryRuntimeComponent>(e);
}

static const ecs::MainInventoryRuntimeComponent* TryGetMainInventoryRuntimeComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::MainInventoryRuntimeComponent>(e);
}

static LPITEM GetMainInventoryItem(const CHARACTER* ch, uint16_t cell)
{
    if (cell >= INVENTORY_AND_EQUIP_SLOT_MAX)
        return nullptr;

    if (const auto* comp = TryGetMainInventoryRuntimeComponent(ch))
        return comp->pItems[cell];

    return nullptr;
}

static LPITEM GetMainInventoryItem(entt::entity e, uint16_t cell)
{
    if (cell >= INVENTORY_AND_EQUIP_SLOT_MAX)
        return nullptr;

    const auto* comp = TryGetMainInventoryRuntimeComponent(e);
    return comp ? comp->pItems[cell] : nullptr;
}

static uint16_t GetMainInventoryGrid(const CHARACTER* ch, uint16_t cell)
{
    if (cell >= INVENTORY_AND_EQUIP_SLOT_MAX)
        return 0;

    if (const auto* comp = TryGetMainInventoryRuntimeComponent(ch))
        return comp->bItemGrid[cell];

    return 0;
}
#ifdef ENABLE_EXTRA_INVENTORY
static ecs::ExtraInventoryRuntimeComponent* EnsureExtraInventoryRuntimeComponent(LPCHARACTER ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::ExtraInventoryRuntimeComponent>(e);
}

static const ecs::ExtraInventoryRuntimeComponent* TryGetExtraInventoryRuntimeComponent(const CHARACTER* ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e);
}
#endif

static ecs::DragonSoulInventoryComponent* EnsureDragonSoulInventoryComponent(LPCHARACTER ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::DragonSoulInventoryComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::DragonSoulInventoryComponent>(e);
}

static const ecs::DragonSoulInventoryComponent* TryGetDragonSoulInventoryComponent(const CHARACTER* ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::DragonSoulInventoryComponent>(e);
}


static ecs::CubeWindowComponent* EnsureCubeWindowComponent(LPCHARACTER ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::CubeWindowComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::CubeWindowComponent>(e);
}

static const ecs::CubeWindowComponent* TryGetCubeWindowComponent(const CHARACTER* ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::CubeWindowComponent>(e);
}

#ifdef __ATTR_TRANSFER_SYSTEM__
static ecs::AttrTransferWindowComponent* EnsureAttrTransferWindowComponent(LPCHARACTER ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::AttrTransferWindowComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::AttrTransferWindowComponent>(e);
}

static const ecs::AttrTransferWindowComponent* TryGetAttrTransferWindowComponent(const CHARACTER* ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::AttrTransferWindowComponent>(e);
}
#endif

#ifdef ENABLE_ACCE_SYSTEM
static ecs::AcceWindowComponent* EnsureAcceWindowComponent(LPCHARACTER ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::AcceWindowComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::AcceWindowComponent>(e);
}

static const ecs::AcceWindowComponent* TryGetAcceWindowComponent(const CHARACTER* ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::AcceWindowComponent>(e);
}
#endif

#ifdef ENABLE_SWITCHBOT
static ecs::SwitchbotRuntimeComponent* EnsureSwitchbotRuntimeComponent(LPCHARACTER ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::SwitchbotRuntimeComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::SwitchbotRuntimeComponent>(e);
}

static const ecs::SwitchbotRuntimeComponent* TryGetSwitchbotRuntimeComponent(const CHARACTER* ch)
{
    if (!ch)
        return nullptr;

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::SwitchbotRuntimeComponent>(e);
}
#endif

static entt::entity ItemEntityOf(LPITEM item)
{
    if (!item || item->GetID() == 0)
        return entt::null;

    return CItemRegistry::Instance().Find(item->GetID());
}

static LPITEM LegacyItemOf(entt::entity itemEntity)
{
    if (itemEntity == entt::null || !g_registry.valid(itemEntity))
        return nullptr;

    const auto* identity = g_registry.try_get<ecs::ItemIdentity>(itemEntity);
    if (!identity || identity->id == 0)
        return nullptr;

    return ITEM_MANAGER::instance().Find(identity->id);
}

static LPITEM ResolveLegacyItemForSync(entt::entity itemEntity)
{
    return LegacyItemOf(itemEntity);
}

static LPITEM ResolveLegacyItemForLegacySideEffect(entt::entity itemEntity)
{
    return LegacyItemOf(itemEntity);
}

static LPITEM ResolveLegacyItemForDestruction(entt::entity itemEntity)
{
    return LegacyItemOf(itemEntity);
}

static bool DestroyItemEntityAndLegacy(entt::entity itemEntity, const char* reason)
{
    if (itemEntity == entt::null || !g_registry.valid(itemEntity))
        return false;

    uint32_t itemID = 0;
    if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(itemEntity))
        itemID = identity->id;

    LPITEM legacyItem = ResolveLegacyItemForDestruction(itemEntity);
    if (legacyItem) {
        EntityFactory::DestroyItemEntity(g_registry, legacyItem);
        if (reason && *reason)
            ITEM_MANAGER::instance().RemoveItem(legacyItem, reason);
        else
            ITEM_MANAGER::instance().RemoveItem(legacyItem);
        return true;
    }

    if (itemID != 0)
        CItemRegistry::Instance().Unregister(itemID);
    if (g_registry.valid(itemEntity))
        g_registry.destroy(itemEntity);
    return true;
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

entt::entity GetItem(entt::entity e, TItemPos cell)
{
    auto* ch = LegacyCharOf(e);
    LPITEM item = ch ? ch->GetItem(cell) : nullptr;
    return EntityFactory::CreateItemEntity(g_registry, item);
}

entt::entity GetInventoryItem(entt::entity e, uint16_t cell)
{
    LPITEM item = GetInventoryItemPtr(e, cell);
    return EntityFactory::CreateItemEntity(g_registry, item);
}

LPITEM GetInventoryItemPtr(entt::entity e, uint16_t cell)
{
    return GetMainInventoryItem(e, cell);
}

#ifdef ENABLE_EXTRA_INVENTORY
entt::entity GetExtraInventoryItem(entt::entity e, uint16_t cell)
{
    auto* ch = LegacyCharOf(e);
    LPITEM item = ch ? ch->GetExtraInventoryItem(cell) : nullptr;
    return EntityFactory::CreateItemEntity(g_registry, item);
}

void SyncExtraInventoryAll(entt::entity e)
{
    auto* ch = LegacyCharOf(e);
    if (!ch || !ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
        return;

    if (e == entt::null || !g_registry.valid(e))
        return;

    const auto* inventory = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e);
    if (!inventory)
        return;

    for (uint16_t cell = 0; cell < EXTRA_INVENTORY_MAX_NUM; ++cell)
    {
        LPITEM item = inventory->pItems[cell];
        const TItemPos packetCell(EXTRA_INVENTORY, cell);

        if (item)
        {
            TPacketGCItemSet packet{};
            packet.header = HEADER_GC_ITEM_SET;
            packet.Cell = packetCell;
            packet.count = item->GetCount();
#ifdef ATTR_LOCK
            packet.lockedattr = item->GetLockedAttr();
#endif
            packet.vnum = item->GetVnum();
            packet.flags = item->GetFlag();
            packet.anti_flags = item->GetAntiFlag();
            packet.highlight = false;

            memcpy(packet.alSockets, item->GetSockets(), sizeof(packet.alSockets));
            memcpy(packet.aAttr, item->GetAttributes(), sizeof(packet.aAttr));

            ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&packet, sizeof(packet));
        }
        else
        {
            TPacketGCItemDelDeprecated packet{};
            packet.header = HEADER_GC_ITEM_DEL;
            packet.Cell = packetCell;
            packet.count = 0;
#ifdef ATTR_LOCK
            packet.lockedattr = -1;
#endif
            packet.vnum = 0;

            ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(&packet, sizeof(packet));
        }
    }
}
#endif

entt::entity FindSpecifyItem(entt::entity e, uint32_t vnum
#ifdef ENABLE_EXTRA_INVENTORY
                       , bool reinforce
#endif
)
{
    auto* ch = LegacyCharOf(e);
    if (!ch)
        return entt::null;

#ifdef ENABLE_EXTRA_INVENTORY
    LPITEM item = ch->FindSpecifyItem(vnum, reinforce);
#else
    LPITEM item = ch->FindSpecifyItem(vnum);
#endif
    return EntityFactory::CreateItemEntity(g_registry, item);
}

entt::entity FindItemByID(entt::entity e, uint32_t id)
{
    auto* ch = LegacyCharOf(e);
    LPITEM item = ch ? ch->FindItemByID(id) : nullptr;
    return EntityFactory::CreateItemEntity(g_registry, item);
}

entt::entity FindItemByID(uint32_t id)
{
    if (id == 0)
        return entt::null;

    entt::entity item = CItemRegistry::Instance().Find(id);
    if (item != entt::null && g_registry.valid(item))
        return item;

    LPITEM legacyItem = ITEM_MANAGER::instance().Find(id);
    return EntityFactory::CreateItemEntity(g_registry, legacyItem);
}

entt::entity FindItemByVID(uint32_t vid)
{
    if (vid == 0)
        return entt::null;

    entt::entity item = CItemRegistry::Instance().FindByVID(vid);
    if (item != entt::null && g_registry.valid(item))
        return item;

    LPITEM legacyItem = ITEM_MANAGER::instance().FindByVID(vid);
    return EntityFactory::CreateItemEntity(g_registry, legacyItem);
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

entt::entity GetWearItem(entt::entity e, uint8_t wearPos)
{
    LPITEM item = GetWear(e, wearPos);
    return EntityFactory::CreateItemEntity(g_registry, item);
}

LPITEM GetWear(entt::entity e, uint8_t wearPos)
{
    if (wearPos >= WEAR_MAX_NUM)
        return nullptr;

    return GetMainInventoryItem(e, static_cast<uint16_t>(INVENTORY_MAX_NUM + wearPos));
}

void SetWearItem(entt::entity e, uint8_t wearPos, entt::entity item)
{
    auto* ch = LegacyCharOf(e);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (ch) {
        ch->SetWear(wearPos, legacyItem);
        SyncItemStateFromLegacy(item);
    }
}

bool UnequipItem(entt::entity e, entt::entity item)
{
    return UnequipItemEcs(e, item);
}

bool EquipItem(entt::entity e, entt::entity item, int candidateCell)
{
    return EquipItemEcs(e, item, candidateCell);
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

bool CanEquipNow(entt::entity e, entt::entity item, const TItemPos& srcCell, const TItemPos& destCell)
{
    auto* ch = LegacyCharOf(e);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    return ch && legacyItem ? ch->CanEquipNow(legacyItem, srcCell, destCell) : false;
}

bool CanUnequipNow(entt::entity e, entt::entity item, const TItemPos& srcCell, const TItemPos& destCell)
{
    auto* ch = LegacyCharOf(e);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    return ch && legacyItem ? ch->CanUnequipNow(legacyItem, srcCell, destCell) : false;
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

bool UseItemEx(entt::entity e, entt::entity item, TItemPos destCell)
{
    return UseItemEcs(e, item, destCell);
}

static bool IsSimpleEquipToggleType(uint8_t type)
{
    switch (type) {
    case ITEM_COSTUME:
    case ITEM_WEAPON:
    case ITEM_ARMOR:
    case ITEM_ROD:
    case ITEM_RING:
    case ITEM_BELT:
    case ITEM_PICK:
    case ITEM_SPECIAL_DS:
        return true;
    default:
        return false;
    }
}

bool UseItemEcs(entt::entity owner, entt::entity item, TItemPos destCell)
{
    auto* ch = LegacyCharOf(owner);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!ch || !legacyItem)
        return false;

    const uint8_t itemType = GetItemType(item);
    if (destCell == NPOS && IsSimpleEquipToggleType(itemType)) {
        return IsItemEquipped(item)
            ? UnequipItemEcs(owner, item)
            : EquipItemEcs(owner, item);
    }

    const uint32_t itemID = legacyItem->GetID();
    const bool result = ch->UseItemEx(legacyItem, destCell);
    if (result) {
        LPITEM currentItem = itemID != 0 ? ITEM_MANAGER::instance().Find(itemID) : nullptr;
        if (!currentItem) {
            DestroyItemEntityAndLegacy(item, "USE_ITEM_ECS_CONSUMED");
            return true;
        }
        SyncItemStateFromLegacy(item);
    }
    return result;
}


void RemoveTypeItem(entt::entity e, uint8_t type, int count)
{
    if (auto* ch = LegacyCharOf(e))
        ch->RemoveSpecifyTypeItem(type, count);
}

void AutoGiveItem(entt::entity e, entt::entity item, bool longOwnerShip
#ifdef __HIGHLIGHT_SYSTEM__
                  , bool isHighLight
#endif
)
{
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (auto* ch = LegacyCharOf(e))
        if (legacyItem) {
            ch->AutoGiveItem(legacyItem, longOwnerShip
#ifdef __HIGHLIGHT_SYSTEM__
                             , isHighLight
#endif
            );
            SyncItemStateFromLegacy(item);
        }
}

#ifdef ENABLE_DS_REFINE_ALL
bool AutoGiveDS(entt::entity e, entt::entity item, bool longOwnerShip)
{
    auto* ch = LegacyCharOf(e);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!ch || !legacyItem)
        return false;

    const bool result = ch->AutoGiveDS(legacyItem, longOwnerShip);
    if (result)
        SyncItemStateFromLegacy(item);
    return result;
}
#endif

namespace {

enum class StackInventoryKind {
    Main,
#ifdef ENABLE_EXTRA_INVENTORY
    Extra,
#endif
};

static bool ItemSocketsMatchProto(entt::entity item, const TItemTable& proto)
{
    const auto* sockets = g_registry.try_get<ecs::ItemSockets>(item);
    if (!sockets)
        return false;

    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i) {
        if (sockets->sockets[i] != proto.alSockets[i])
            return false;
    }

    return true;
}

static entt::entity FindMergeTarget(entt::entity owner, uint32_t itemVnum,
                                    const TItemTable& proto,
                                    StackInventoryKind kind)
{
    const auto* ownerID = g_registry.try_get<ecs::PlayerID>(owner);
    if (!ownerID || ownerID->pid == 0)
        return entt::null;

    const uint8_t targetWindow =
#ifdef ENABLE_EXTRA_INVENTORY
        kind == StackInventoryKind::Extra ? EXTRA_INVENTORY :
#endif
        INVENTORY;

    auto view = g_registry.view<ecs::ItemIdentity, ecs::ItemOwner,
                                ecs::ItemLocation, ecs::ItemCount,
                                ecs::ItemSockets>();
    for (auto item : view) {
        const auto& identity = view.get<ecs::ItemIdentity>(item);
        const auto& itemOwner = view.get<ecs::ItemOwner>(item);
        const auto& location = view.get<ecs::ItemLocation>(item);
        const auto& count = view.get<ecs::ItemCount>(item);

        if (itemOwner.ownerPID != ownerID->pid)
            continue;
        if (location.window != targetWindow)
            continue;
        if (identity.vnum != itemVnum)
            continue;
        if (count.count >= g_bItemCountLimit)
            continue;
        if (!ItemSocketsMatchProto(item, proto))
            continue;

        return item;
    }

    return entt::null;
}

static uint32_t MergeIntoStack(entt::entity target, uint32_t count)
{
    if (target == entt::null || !g_registry.valid(target) || count == 0)
        return count;

    const uint32_t current = GetItemCount(target);
    if (current >= static_cast<uint32_t>(g_bItemCountLimit))
        return count;

    const uint32_t capacity = static_cast<uint32_t>(g_bItemCountLimit) - current;
    const uint32_t merged = std::min(capacity, count);
    SetItemCount(target, current + merged);
    return count - merged;
}

static entt::entity TryMergeItemVnum(entt::entity owner, uint32_t itemVnum,
                                     uint32_t& count, const TItemTable& proto,
                                     StackInventoryKind kind)
{
    entt::entity lastMerged = entt::null;
    while (count > 0) {
        entt::entity target = FindMergeTarget(owner, itemVnum, proto, kind);
        if (target == entt::null)
            break;

        const uint32_t before = count;
        count = MergeIntoStack(target, count);
        lastMerged = target;

        if (count == before)
            break;
    }

    return count == 0 ? lastMerged : entt::null;
}

static void SendAutoGiveMessage(entt::entity owner, uint32_t count, LPITEM item)
{
    auto* ch = LegacyCharOf(owner);
    if (!ch || !item)
        return;

#ifdef TEXTS_IMPROVEMENT
    ecs::ChatSystem::SendNew(owner,
#ifdef ENABLE_NEW_CHAT
                             CHAT_TYPE_INFO_ITEM
#else
                             CHAT_TYPE_INFO
#endif
                             , 102, "%d#%s", count,
                             item->GetName(ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetLanguage() : 0));
#else
    (void)count;
#endif
}

static entt::entity CreateItemEntityFromProto(uint32_t itemVnum,
                                              uint32_t count,
                                              int rarePct)
{
    (void)rarePct;
    LPITEM item = ITEM_MANAGER::instance().CreateItem(itemVnum, count, 0, true);
    return EntityFactory::CreateItemEntity(g_registry, item);
}

static entt::entity HandleBlendItemMerge(entt::entity owner, entt::entity item)
{
    auto* ch = LegacyCharOf(owner);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!ch || !legacyItem || legacyItem->GetType() != ITEM_BLEND)
        return item;

    auto* incomingSockets = g_registry.try_get<ecs::ItemSockets>(item);
    if (!incomingSockets)
        return item;

    const uint32_t incomingVnum = GetItemVnum(item);
    auto view = g_registry.view<ecs::ItemIdentity, ecs::ItemOwner,
                                ecs::ItemLocation, ecs::ItemCount,
                                ecs::ItemPrototypeMeta, ecs::ItemSockets>();
    const auto* ownerID = g_registry.try_get<ecs::PlayerID>(owner);
    if (!ownerID)
        return item;

    for (auto candidate : view) {
        if (candidate == item)
            continue;

        const auto& identity = view.get<ecs::ItemIdentity>(candidate);
        const auto& candidateOwner = view.get<ecs::ItemOwner>(candidate);
        const auto& location = view.get<ecs::ItemLocation>(candidate);
        const auto& count = view.get<ecs::ItemCount>(candidate);
        const auto& meta = view.get<ecs::ItemPrototypeMeta>(candidate);
        const auto& sockets = view.get<ecs::ItemSockets>(candidate);

        if (candidateOwner.ownerPID != ownerID->pid)
            continue;
        if (location.window != INVENTORY)
            continue;
        if (meta.type != ITEM_BLEND || identity.vnum != incomingVnum)
            continue;
        if (sockets.sockets[0] != incomingSockets->sockets[0] ||
            sockets.sockets[1] != incomingSockets->sockets[1] ||
            sockets.sockets[2] != incomingSockets->sockets[2])
            continue;
        if (count.count >= g_bItemCountLimit)
            continue;

        SetItemCount(candidate, static_cast<uint32_t>(count.count) + GetItemCount(item));
        EntityFactory::DestroyItemEntity(g_registry, legacyItem);
        M2_DESTROY_ITEM(legacyItem);
        return candidate;
    }

    return item;
}

static entt::entity PlaceItemInInventory(entt::entity owner, entt::entity item,
                                         bool longOwnerShip,
                                         bool sendMessage,
                                         uint32_t messageCount)
{
    auto* ch = LegacyCharOf(owner);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!ch || !legacyItem)
        return entt::null;

    int cell = -1;
    if (legacyItem->IsDragonSoul()) {
        cell = ch->GetEmptyDragonSoulInventory(legacyItem);
    }
#ifdef ENABLE_EXTRA_INVENTORY
    else if (legacyItem->IsExtraItem()) {
        cell = ch->GetEmptyExtraInventory(legacyItem);
    }
#endif
    else {
        cell = ch->GetEmptyInventory(legacyItem->GetSize());
    }

    if (cell != -1) {
        if (legacyItem->IsDragonSoul()) {
            if (!PlaceItemEcs(owner, item, DRAGON_SOUL_INVENTORY, cell))
                return entt::null;
        }
#ifdef ENABLE_EXTRA_INVENTORY
        else if (legacyItem->IsExtraItem()) {
            if (!PlaceItemEcs(owner, item, EXTRA_INVENTORY, cell))
                return entt::null;
        }
#endif
        else {
            if (!PlaceItemEcs(owner, item, INVENTORY, cell))
                return entt::null;
        }

        LogManager::instance().ItemLog(ch, legacyItem, "SYSTEM", legacyItem->GetName());
        if (sendMessage)
            SendAutoGiveMessage(owner, messageCount, legacyItem);

        if (legacyItem->GetType() == ITEM_USE && legacyItem->GetSubType() == USE_POTION) {
            TQuickslot* pSlot;
            if (ch->GetQuickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE) {
                TQuickslot slot;
                slot.type = QUICKSLOT_TYPE_ITEM;
                slot.pos = cell;
                ch->SetQuickslot(0, slot);
            }
        }

        SyncItemStateFromLegacy(item);
        return item;
    }

    legacyItem->AddToGround(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), ch->GetXYZ());
#ifdef ENABLE_NEWSTUFF
    legacyItem->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
    legacyItem->StartDestroyEvent();
#endif
    legacyItem->SetOwnership(ch, longOwnerShip ? 300 : 60);
    LogManager::instance().ItemLog(ch, legacyItem, "SYSTEM_DROP", legacyItem->GetName());
    SyncItemStateFromLegacy(item);
    return item;
}

} // namespace

entt::entity AutoGiveItemEcs(entt::entity owner, uint32_t itemVnum,
                             uint32_t count, int rarePct,
                             bool sendMessage)
{
    auto* ch = LegacyCharOf(owner);
    if (!ch)
        return entt::null;

    TItemTable* proto = ITEM_MANAGER::instance().GetTable(itemVnum);
    if (!proto)
        return entt::null;

    const uint32_t requestedCount = count;
    DBManager::instance().SendMoneyLog(MONEY_LOG_DROP, itemVnum, count);

    entt::entity merged = entt::null;
#ifdef ENABLE_EXTRA_INVENTORY
    if ((proto->dwFlags & ITEM_FLAG_STACKABLE) &&
        ITEM_MANAGER::instance().IsExtraItem(itemVnum)) {
        if (IS_SET(proto->dwFlags, ITEM_FLAG_MAKECOUNT) &&
            count < static_cast<uint32_t>(proto->alValues[1])) {
            count = static_cast<uint32_t>(proto->alValues[1]);
        }

        merged = TryMergeItemVnum(owner, itemVnum, count, *proto, StackInventoryKind::Extra);
        if (merged != entt::null) {
            if (sendMessage)
                SendAutoGiveMessage(owner, requestedCount, ResolveLegacyItemForLegacySideEffect(merged));
            return merged;
        }
    }
    else
#endif
    if ((proto->dwFlags & ITEM_FLAG_STACKABLE) && proto->bType != ITEM_BLEND) {
        if (IS_SET(proto->dwFlags, ITEM_FLAG_MAKECOUNT) &&
            count < static_cast<uint32_t>(proto->alValues[1])) {
            count = static_cast<uint32_t>(proto->alValues[1]);
        }

        merged = TryMergeItemVnum(owner, itemVnum, count, *proto, StackInventoryKind::Main);
        if (merged != entt::null) {
            if (sendMessage)
                SendAutoGiveMessage(owner, requestedCount, ResolveLegacyItemForLegacySideEffect(merged));
            return merged;
        }
    }

    entt::entity created = CreateItemEntityFromProto(itemVnum, count, rarePct);
    if (created == entt::null)
        return entt::null;

    entt::entity mergedBlend = HandleBlendItemMerge(owner, created);
    if (mergedBlend != created)
        return mergedBlend;

    return PlaceItemInInventory(owner, created, false, sendMessage, count);
}

bool IsValidItem(entt::entity item)
{
    return item != entt::null && g_registry.valid(item) &&
           g_registry.any_of<ecs::ItemIdentity>(item);
}

uint32_t GetItemID(entt::entity item)
{
    if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(item))
        return identity->id;

    return 0;
}

uint32_t GetItemVID(entt::entity item)
{
    if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(item))
        return identity->vid;

    return 0;
}

uint32_t GetItemVnum(entt::entity item)
{
    if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(item))
        return identity->vnum;

    return 0;
}

uint32_t GetItemOriginalVnum(entt::entity item)
{
    return GetItemVnum(item);
}

uint8_t GetItemType(entt::entity item)
{
    if (const auto* meta = g_registry.try_get<ecs::ItemPrototypeMeta>(item))
        return meta->type;

    return 0;
}

uint8_t GetItemSubType(entt::entity item)
{
    if (const auto* meta = g_registry.try_get<ecs::ItemPrototypeMeta>(item))
        return meta->subType;

    return 0;
}

uint32_t GetItemCount(entt::entity item)
{
    if (const auto* count = g_registry.try_get<ecs::ItemCount>(item))
        return static_cast<uint32_t>(count->count);

    return 0;
}

int32_t GetItemValue(entt::entity item, uint32_t index)
{
    if (index >= ITEM_VALUES_MAX_NUM)
        return 0;

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetValue(index);

    return 0;
}

const char* GetItemName(entt::entity item)
{
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetName();

    return "";
}

uint8_t GetItemSize(entt::entity item)
{
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetSize();

    return 0;
}

uint32_t GetItemRefineVnum(entt::entity item)
{
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetRefinedVnum();

    return 0;
}

int GetItemRefineLevel(entt::entity item)
{
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetRefineLevel();

    return 0;
}

int GetItemLevelLimit(entt::entity item)
{
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetLevelLimit();

    return 0;
}

int GetItemLimitTimerBasedOnWearIndex(entt::entity item)
{
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetProto() ? legacyItem->GetProto()->cLimitTimerBasedOnWearIndex : -1;

    return -1;
}

int32_t GetItemFlags(entt::entity item)
{
    if (const auto* flags = g_registry.try_get<ecs::ItemFlags>(item))
        return flags->flags;

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetFlag();

    return 0;
}

uint32_t GetItemWearFlags(entt::entity item)
{
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetWearFlag();

    return 0;
}

uint32_t GetItemWearFlag(entt::entity item)
{
    return GetItemWearFlags(item);
}

uint32_t GetItemAntiFlags(entt::entity item)
{
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetAntiFlag();

    return 0;
}

uint32_t GetItemAntiFlag(entt::entity item)
{
    return GetItemAntiFlags(item);
}

uint32_t GetItemImmuneFlags(entt::entity item)
{
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetImmuneFlag();

    return 0;
}

const TItemTable* GetItemProto(entt::entity item)
{
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetProto();

    return nullptr;
}

static void SetItemCountComponentOnly(entt::entity item, uint32_t count)
{
    if (item != entt::null && g_registry.valid(item))
        g_registry.emplace_or_replace<ecs::ItemCount>(item, ecs::ItemCount{static_cast<int>(count)});
}

static void MirrorItemCountToLegacyNonDestroy(entt::entity item, uint32_t count)
{
    if (count == 0)
        return;
    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        legacyItem->SetCount(count);
}

void SetItemCount(entt::entity item, uint32_t count)
{
    if (item == entt::null || !g_registry.valid(item))
        return;

    if (count == 0) {
        DestroyItemEntityAndLegacy(item, "SET_ITEM_COUNT_ZERO");
        return;
    }

    SetItemCountComponentOnly(item, count);
    MirrorItemCountToLegacyNonDestroy(item, count);
}

bool SetItemCountEcs(entt::entity item, uint32_t count)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    if (count == 0)
        return DestroyItemEntityAndLegacy(item, "SET_ITEM_COUNT_ECS_ZERO");

    SetItemCount(item, count);
    return true;
}

bool AddItemCountEcs(entt::entity item, int delta)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    const int current = static_cast<int>(GetItemCount(item));
    const int next = current + delta;
    if (next <= 0)
        return DestroyItemEntityAndLegacy(item, "ADD_ITEM_COUNT_ECS_ZERO");

    return SetItemCountEcs(item, static_cast<uint32_t>(next));
}

bool ConsumeItem(entt::entity item, uint32_t amount)
{
    if (item == entt::null || !g_registry.valid(item) || amount == 0)
        return false;

    const uint32_t count = GetItemCount(item);
    if (count > amount) {
        SetItemCount(item, count - amount);
        return true;
    }

    return DestroyItemEntityAndLegacy(item, "CONSUME_ITEM");
}

bool ConsumeItemEcs(entt::entity item, uint32_t amount)
{
    return ConsumeItem(item, amount);
}

bool DestroyItemEntityEcs(entt::entity item, const char* reason)
{
    return DestroyItemEntityAndLegacy(item, reason ? reason : "DESTROY_ITEM_ENTITY_ECS");
}

entt::entity GetItemOwner(entt::entity item)
{
    if (const auto* owner = g_registry.try_get<ecs::ItemOwner>(item)) {
        if (owner->ownerPID != 0) {
            if (auto* ch = CHARACTER_MANAGER::instance().FindByPID(owner->ownerPID))
                return AIHelpers::EcsOf(ch);
        }
    }

    if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(item)) {
        if (LPITEM legacyItem = ITEM_MANAGER::instance().Find(identity->id)) {
            if (auto* ch = legacyItem->GetOwner())
                return AIHelpers::EcsOf(ch);
        }
    }

    return entt::null;
}

entt::entity GetItemOwnerEntity(entt::entity item)
{
    return GetItemOwner(item);
}

uint32_t GetItemLastOwnerPID(entt::entity item)
{
    if (const auto* owner = g_registry.try_get<ecs::ItemOwner>(item))
        return owner->lastOwnerPID;

    return 0;
}

uint32_t GetItemSocket(entt::entity item, int index)
{
    if (index < 0 || index >= ITEM_SOCKET_MAX_NUM)
        return 0;

    if (const auto* sockets = g_registry.try_get<ecs::ItemSockets>(item))
        return static_cast<uint32_t>(sockets->sockets[index]);

    return 0;
}

bool HasItemSocket(entt::entity item, int index)
{
    return index >= 0 && index < ITEM_SOCKET_MAX_NUM &&
           g_registry.try_get<ecs::ItemSockets>(item) != nullptr;
}

TPlayerItemAttribute GetItemAttribute(entt::entity item, int index)
{
    if (index < 0 || index >= ITEM_ATTRIBUTE_MAX_NUM)
        return {};

    if (const auto* attrs = g_registry.try_get<ecs::ItemAttributes>(item))
        return attrs->attrs[index];

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetAttribute(index);

    return {};
}

int GetItemAttributeType(entt::entity item, int index)
{
    if (index < 0 || index >= ITEM_ATTRIBUTE_MAX_NUM)
        return 0;

    if (const auto* attrs = g_registry.try_get<ecs::ItemAttributes>(item))
        return attrs->attrs[index].bType;

    return 0;
}

int GetItemAttributeValue(entt::entity item, int index)
{
    if (index < 0 || index >= ITEM_ATTRIBUTE_MAX_NUM)
        return 0;

    if (const auto* attrs = g_registry.try_get<ecs::ItemAttributes>(item))
        return attrs->attrs[index].sValue;

    return 0;
}

bool SetItemSocket(entt::entity item, int index, uint32_t value)
{
    if (item == entt::null || !g_registry.valid(item) ||
        index < 0 || index >= ITEM_SOCKET_MAX_NUM)
        return false;

    auto& sockets = g_registry.get_or_emplace<ecs::ItemSockets>(item);
    sockets.sockets[index] = static_cast<int32_t>(value);

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        legacyItem->SetSocket(index, static_cast<int32_t>(value));

    return true;
}

bool SetItemSocketEcs(entt::entity item, int index, uint32_t value)
{
    return SetItemSocket(item, index, value);
}

bool SyncItemSocketsFromLegacy(entt::entity item)
{
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!legacyItem || item == entt::null || !g_registry.valid(item))
        return false;

    ecs::ItemSockets sockets{};
    const int32_t* values = legacyItem->GetSockets();
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        sockets.sockets[i] = values[i];

    g_registry.emplace_or_replace<ecs::ItemSockets>(item, sockets);
    return true;
}

bool SyncLegacySocketsFromEcs(entt::entity item)
{
    auto* sockets = g_registry.try_get<ecs::ItemSockets>(item);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!sockets || !legacyItem)
        return false;

    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        legacyItem->SetSocket(i, sockets->sockets[i]);

    return true;
}

bool SetItemAttribute(entt::entity item, int index, int type, int value)
{
    if (item == entt::null || !g_registry.valid(item) ||
        index < 0 || index >= ITEM_ATTRIBUTE_MAX_NUM)
        return false;

    auto& attrs = g_registry.get_or_emplace<ecs::ItemAttributes>(item);
    attrs.attrs[index].bType = static_cast<uint8_t>(type);
    attrs.attrs[index].sValue = static_cast<short>(value);

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        legacyItem->SetAttributes(attrs.attrs.data());

    return true;
}

bool ClearItemAttribute(entt::entity item, int index)
{
    return SetItemAttribute(item, index, APPLY_NONE, 0);
}

bool SetItemForceAttributeEcs(entt::entity item, int index, uint8_t type, int16_t value)
{
    return SetItemAttribute(item, index, type, value);
}

bool AddItemAttributeEcs(entt::entity item)
{
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!legacyItem)
        return false;

    legacyItem->AddAttribute();
    return SyncItemAttributesFromLegacy(item);
}

bool ChangeItemAttributeEcs(entt::entity item)
{
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!legacyItem)
        return false;

    legacyItem->ChangeAttribute();
    return SyncItemAttributesFromLegacy(item);
}

bool ClearItemAttributesEcs(entt::entity item)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& attrs = g_registry.get_or_emplace<ecs::ItemAttributes>(item);
    for (auto& attr : attrs.attrs) {
        attr.bType = APPLY_NONE;
        attr.sValue = 0;
    }

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        legacyItem->SetAttributes(attrs.attrs.data());

    return true;
}

bool SyncItemAttributesFromLegacy(entt::entity item)
{
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!legacyItem || item == entt::null || !g_registry.valid(item))
        return false;

    ecs::ItemAttributes attrs{};
    const TPlayerItemAttribute* values = legacyItem->GetAttributes();
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        attrs.attrs[i] = values[i];

    g_registry.emplace_or_replace<ecs::ItemAttributes>(item, attrs);
    return true;
}

bool SyncLegacyAttributesFromEcs(entt::entity item)
{
    auto* attrs = g_registry.try_get<ecs::ItemAttributes>(item);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!attrs || !legacyItem)
        return false;

    legacyItem->SetAttributes(attrs->attrs.data());
    return true;
}

bool SetItemExchanging(entt::entity item, bool flag)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& flags = g_registry.get_or_emplace<ecs::ItemFlags>(item);
    flags.exchanging = flag;

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        legacyItem->SetExchanging(flag);

    return true;
}

bool LockItem(entt::entity item, bool locked)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& flags = g_registry.get_or_emplace<ecs::ItemFlags>(item);
    flags.isLocked = locked;

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        legacyItem->Lock(locked);

    return true;
}

bool UnlockItem(entt::entity item)
{
    return LockItem(item, false);
}

bool SetItemSkipSave(entt::entity item, bool flag)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& flags = g_registry.get_or_emplace<ecs::ItemFlags>(item);
    flags.skipSave = flag;

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        legacyItem->SetSkipSave(flag);

    return true;
}

bool SetItemWindow(entt::entity item, uint8_t window)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& location = g_registry.get_or_emplace<ecs::ItemLocation>(item);
    location.window = window;

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        legacyItem->SetWindow(window);

    return true;
}

bool SetItemCell(entt::entity item, entt::entity owner, uint16_t cell)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& location = g_registry.get_or_emplace<ecs::ItemLocation>(item);
    location.cell = cell;

    LPCHARACTER legacyOwner = LegacyCharOf(owner);
    if (!legacyOwner)
        legacyOwner = ResolveLegacyItemForLegacySideEffect(item)
            ? ResolveLegacyItemForLegacySideEffect(item)->GetOwner()
            : nullptr;

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        legacyItem->SetCell(legacyOwner, cell);

    return true;
}

bool AlterItemToMagicItem(entt::entity item)
{
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!legacyItem)
        return false;

    legacyItem->AlterToMagicItem();
    return SyncItemStateFromLegacy(item);
}

uint8_t GetItemWindow(entt::entity item)
{
    if (const auto* location = g_registry.try_get<ecs::ItemLocation>(item))
        return location->window;

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetWindow();

    return 0;
}

uint16_t GetItemCell(entt::entity item)
{
    if (const auto* location = g_registry.try_get<ecs::ItemLocation>(item))
        return location->cell;

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->GetCell();

    return 0;
}

bool IsItemEquipped(entt::entity item)
{
    if (const auto* equipped = g_registry.try_get<ecs::ItemEquipped>(item))
        return equipped->equipped;

    if (LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item))
        return legacyItem->IsEquipped();

    return false;
}

bool IsItemInInventory(entt::entity item)
{
    return GetItemWindow(item) == INVENTORY;
}

bool IsItemInExtraInventory(entt::entity item)
{
#ifdef ENABLE_EXTRA_INVENTORY
    return GetItemWindow(item) == EXTRA_INVENTORY;
#else
    (void)item;
    return false;
#endif
}

bool IsItemInDragonSoulInventory(entt::entity item)
{
    return GetItemWindow(item) == DRAGON_SOUL_INVENTORY;
}

static uint32_t EntityPlayerID(entt::entity e);

static bool IsItemLocationInBounds(uint8_t window, uint16_t cell)
{
    switch (window) {
    case INVENTORY:
        return cell < INVENTORY_AND_EQUIP_SLOT_MAX;
    case DRAGON_SOUL_INVENTORY:
        return cell < DRAGON_SOUL_INVENTORY_MAX_NUM;
#ifdef ENABLE_EXTRA_INVENTORY
    case EXTRA_INVENTORY:
        return cell < EXTRA_INVENTORY_MAX_NUM;
#endif
    default:
        return false;
    }
}

static bool IsItemLocationOccupied(entt::entity owner, entt::entity item,
                                   uint8_t window, uint16_t cell)
{
    const uint32_t ownerPID = EntityPlayerID(owner);
    if (ownerPID == 0)
        return false;

    auto view = g_registry.view<ecs::ItemOwner, ecs::ItemLocation>();
    for (auto other : view) {
        if (other == item)
            continue;

        const auto& otherOwner = view.get<ecs::ItemOwner>(other);
        const auto& otherLocation = view.get<ecs::ItemLocation>(other);
        if (otherOwner.ownerPID == ownerPID &&
            otherLocation.window == window &&
            otherLocation.cell == cell) {
            return true;
        }
    }

    return false;
}

bool PlaceItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell)
{
    auto* ch = LegacyCharOf(owner);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!ch || !legacyItem)
        return false;
    if (!IsItemLocationInBounds(window, cell))
        return false;
    if (IsItemLocationOccupied(owner, item, window, cell))
        return false;

    const bool hadLocation = g_registry.all_of<ecs::ItemLocation>(item);
    const bool hadOwner = g_registry.all_of<ecs::ItemOwner>(item);
    const bool hadEquipped = g_registry.all_of<ecs::ItemEquipped>(item);
    const ecs::ItemLocation oldLocation =
        hadLocation ? g_registry.get<ecs::ItemLocation>(item) : ecs::ItemLocation {};
    const ecs::ItemOwner oldOwner =
        hadOwner ? g_registry.get<ecs::ItemOwner>(item) : ecs::ItemOwner {};
    const ecs::ItemEquipped oldEquipped =
        hadEquipped ? g_registry.get<ecs::ItemEquipped>(item) : ecs::ItemEquipped {};

    if (!TransferItemOwnership(item, entt::null, owner))
        return false;

    g_registry.emplace_or_replace<ecs::ItemLocation>(item, ecs::ItemLocation{window, cell});
    g_registry.emplace_or_replace<ecs::ItemEquipped>(item, ecs::ItemEquipped{false});

    if (!legacyItem->AddToCharacter(ch, TItemPos(window, cell))) {
        if (hadLocation)
            g_registry.emplace_or_replace<ecs::ItemLocation>(item, oldLocation);
        else
            g_registry.remove<ecs::ItemLocation>(item);

        if (hadOwner)
            g_registry.emplace_or_replace<ecs::ItemOwner>(item, oldOwner);
        else
            g_registry.remove<ecs::ItemOwner>(item);

        if (hadEquipped)
            g_registry.emplace_or_replace<ecs::ItemEquipped>(item, oldEquipped);
        else
            g_registry.remove<ecs::ItemEquipped>(item);
        return false;
    }

    SyncItemStateFromLegacy(item);
    return true;
}

bool RemoveItemEcs(entt::entity item)
{
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!legacyItem)
        return false;

    g_registry.emplace_or_replace<ecs::ItemLocation>(
        item, ecs::ItemLocation{RESERVED_WINDOW, 0});
    g_registry.emplace_or_replace<ecs::ItemOwner>(
        item, ecs::ItemOwner{0, legacyItem->GetLastOwnerPID(), 0});
    g_registry.remove<ecs::ItemEquipped>(item);

    if (legacyItem->GetOwner()) {
        legacyItem->RemoveFromCharacter();
        SyncItemStateFromLegacy(item);
    }

    return true;
}

bool EquipItemEcs(entt::entity owner, entt::entity item, int candidateCell)
{
    auto* ch = LegacyCharOf(owner);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!ch || !legacyItem)
        return false;

    const int wearCell = legacyItem->FindEquipCell(ch, candidateCell);
    if (wearCell < 0)
        return false;

    const bool hadLocation = g_registry.all_of<ecs::ItemLocation>(item);
    const bool hadOwner = g_registry.all_of<ecs::ItemOwner>(item);
    const bool hadEquipped = g_registry.all_of<ecs::ItemEquipped>(item);
    const ecs::ItemLocation oldLocation =
        hadLocation ? g_registry.get<ecs::ItemLocation>(item) : ecs::ItemLocation {};
    const ecs::ItemOwner oldOwner =
        hadOwner ? g_registry.get<ecs::ItemOwner>(item) : ecs::ItemOwner {};
    const ecs::ItemEquipped oldEquipped =
        hadEquipped ? g_registry.get<ecs::ItemEquipped>(item) : ecs::ItemEquipped {};

    TransferItemOwnership(item, entt::null, owner);
    g_registry.emplace_or_replace<ecs::ItemLocation>(
        item, ecs::ItemLocation{EQUIPMENT, static_cast<uint16_t>(INVENTORY_MAX_NUM + wearCell)});
    g_registry.emplace_or_replace<ecs::ItemEquipped>(
        item, ecs::ItemEquipped{true, static_cast<uint8_t>(wearCell)});

    const bool result = ch->EquipItem(legacyItem, candidateCell);
    if (!result) {
        if (hadLocation)
            g_registry.emplace_or_replace<ecs::ItemLocation>(item, oldLocation);
        else
            g_registry.remove<ecs::ItemLocation>(item);

        if (hadOwner)
            g_registry.emplace_or_replace<ecs::ItemOwner>(item, oldOwner);
        else
            g_registry.remove<ecs::ItemOwner>(item);

        if (hadEquipped)
            g_registry.emplace_or_replace<ecs::ItemEquipped>(item, oldEquipped);
        else
            g_registry.remove<ecs::ItemEquipped>(item);
        return false;
    }

    SyncItemStateFromLegacy(item);
    return true;
}

bool UnequipItemEcs(entt::entity owner, entt::entity item)
{
    auto* ch = LegacyCharOf(owner);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!ch || !legacyItem)
        return false;

    const int targetCell = legacyItem->IsDragonSoul()
        ? ch->GetEmptyDragonSoulInventory(legacyItem)
        : ch->GetEmptyInventory(legacyItem->GetSize());
    if (targetCell < 0)
        return false;

    const uint8_t targetWindow = legacyItem->IsDragonSoul()
        ? DRAGON_SOUL_INVENTORY
        : INVENTORY;

    const bool hadLocation = g_registry.all_of<ecs::ItemLocation>(item);
    const bool hadOwner = g_registry.all_of<ecs::ItemOwner>(item);
    const bool hadEquipped = g_registry.all_of<ecs::ItemEquipped>(item);
    const ecs::ItemLocation oldLocation =
        hadLocation ? g_registry.get<ecs::ItemLocation>(item) : ecs::ItemLocation {};
    const ecs::ItemOwner oldOwner =
        hadOwner ? g_registry.get<ecs::ItemOwner>(item) : ecs::ItemOwner {};
    const ecs::ItemEquipped oldEquipped =
        hadEquipped ? g_registry.get<ecs::ItemEquipped>(item) : ecs::ItemEquipped {};

    TransferItemOwnership(item, entt::null, owner);
    g_registry.emplace_or_replace<ecs::ItemLocation>(
        item, ecs::ItemLocation{targetWindow, static_cast<uint16_t>(targetCell)});
    g_registry.emplace_or_replace<ecs::ItemEquipped>(
        item, ecs::ItemEquipped{false, 0});

    const bool result = ch->UnequipItem(legacyItem);
    if (!result) {
        if (hadLocation)
            g_registry.emplace_or_replace<ecs::ItemLocation>(item, oldLocation);
        else
            g_registry.remove<ecs::ItemLocation>(item);

        if (hadOwner)
            g_registry.emplace_or_replace<ecs::ItemOwner>(item, oldOwner);
        else
            g_registry.remove<ecs::ItemOwner>(item);

        if (hadEquipped)
            g_registry.emplace_or_replace<ecs::ItemEquipped>(item, oldEquipped);
        else
            g_registry.remove<ecs::ItemEquipped>(item);
        return false;
    }

    SyncItemStateFromLegacy(item);
    return true;
}

bool SyncItemLocationFromLegacy(entt::entity item)
{
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!legacyItem || item == entt::null || !g_registry.valid(item))
        return false;

    g_registry.emplace_or_replace<ecs::ItemLocation>(
        item, ecs::ItemLocation{legacyItem->GetWindow(), legacyItem->GetCell()});
    uint8_t slot = 0;
    if (legacyItem->IsEquipped() && legacyItem->GetCell() >= INVENTORY_MAX_NUM)
        slot = static_cast<uint8_t>(legacyItem->GetCell() - INVENTORY_MAX_NUM);
    g_registry.emplace_or_replace<ecs::ItemEquipped>(
        item, ecs::ItemEquipped{legacyItem->IsEquipped(), slot});
    return true;
}

bool SyncItemOwnerFromLegacy(entt::entity item)
{
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!legacyItem || item == entt::null || !g_registry.valid(item))
        return false;

    uint32_t ownerPID = 0;
    if (const auto* owner = legacyItem->GetOwner())
        ownerPID = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(owner));

    g_registry.emplace_or_replace<ecs::ItemOwner>(
        item, ecs::ItemOwner{ownerPID, legacyItem->GetLastOwnerPID(), ownerPID});
    return true;
}

bool SyncItemStateFromLegacy(entt::entity item)
{
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!legacyItem || item == entt::null || !g_registry.valid(item))
        return false;

    g_registry.emplace_or_replace<ecs::ItemIdentity>(
        item, ecs::ItemIdentity{
                  legacyItem->GetID(),
                  legacyItem->GetOriginalVnum(),
                  legacyItem->GetVID(),
                  legacyItem->GetMaskVnum(),
              });
    g_registry.emplace_or_replace<ecs::ItemCount>(
        item, ecs::ItemCount{legacyItem->GetCount()});
    g_registry.emplace_or_replace<ecs::ItemPrototypeMeta>(
        item, ecs::ItemPrototypeMeta{legacyItem->GetType(), legacyItem->GetSubType()});
    g_registry.emplace_or_replace<ecs::ItemFlags>(
        item, ecs::ItemFlags{
                  legacyItem->GetFlag(),
                  legacyItem->IsExchanging(),
                  legacyItem->GetSkipSave(),
                  legacyItem->isLocked(),
              });

    SyncItemLocationFromLegacy(item);
    SyncItemOwnerFromLegacy(item);
    SyncItemSocketsFromLegacy(item);
    SyncItemAttributesFromLegacy(item);
    return true;
}

bool DestroyLoadedDuplicateItem(entt::entity item)
{
    LPITEM legacyItem = ResolveLegacyItemForDestruction(item);
    if (!legacyItem) {
        LOG_ERROR("DUP_ITEM_DESTROY_RESOLVE_FAIL entity={}", static_cast<uint32_t>(item));
        return false;
    }

    const uint32_t itemID = legacyItem->GetID();
    const uint32_t itemVID = legacyItem->GetVID();
    const uint32_t itemVnum = legacyItem->GetVnum();
    const uint32_t lastOwnerPID = legacyItem->GetLastOwnerPID();
    const uint8_t itemWindow = legacyItem->GetWindow();
    const uint16_t itemCell = legacyItem->GetCell();
    LOG_ERROR("DUP_ITEM_DESTROY_BEGIN entity={} item={} id={} vid={} vnum={} last_owner_pid={} window={} cell={}",
        static_cast<uint32_t>(item), static_cast<const void*>(legacyItem), itemID, itemVID, itemVnum,
        lastOwnerPID, static_cast<int>(itemWindow), itemCell);

    legacyItem->SetSkipSave(true);

    const auto* ownerState = g_registry.try_get<ecs::ItemOwner>(item);
    const uint32_t ownerPID = ownerState ? ownerState->ownerPID : 0;
    LPCHARACTER legacyOwner = legacyItem->GetOwner();
    LPCHARACTER liveOwner = ownerPID != 0 ? CHARACTER_MANAGER::instance().FindByPID(ownerPID) : nullptr;
    LOG_ERROR("DUP_ITEM_DESTROY_OWNER entity={} id={} owner_pid={} legacy_owner={} live_owner={}",
        static_cast<uint32_t>(item), itemID, ownerPID, static_cast<const void*>(legacyOwner),
        static_cast<const void*>(liveOwner));

    if (legacyOwner) {
        LOG_ERROR("DUP_ITEM_DESTROY_REMOVE_FROM_CHARACTER_BEGIN entity={} id={} stale_owner={}",
            static_cast<uint32_t>(item), itemID, liveOwner != legacyOwner);
        legacyItem->RemoveFromCharacter();
        LOG_ERROR("DUP_ITEM_DESTROY_REMOVE_FROM_CHARACTER_END entity={} id={} owner_after={}",
            static_cast<uint32_t>(item), itemID, static_cast<const void*>(legacyItem->GetOwner()));
    }

    LOG_ERROR("DUP_ITEM_DESTROY_LEGACY_BEGIN item={} id={}", static_cast<const void*>(legacyItem), itemID);
    M2_DESTROY_ITEM(legacyItem);
    LOG_ERROR("DUP_ITEM_DESTROY_LEGACY_END item={} id={}", static_cast<const void*>(legacyItem), itemID);
    return true;
}

bool GiveItem(entt::entity from, entt::entity victim, TItemPos cell)
{
    auto* fromCh = LegacyCharOf(from);
    auto* victimCh = LegacyCharOf(victim);
    return (fromCh && victimCh) ? fromCh->GiveItem(victimCh, cell) : false;
}

bool CanReceiveItem(entt::entity receiver, entt::entity from, entt::entity item)
{
    auto* receiverCh = LegacyCharOf(receiver);
    auto* fromCh = LegacyCharOf(from);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    return (receiverCh && fromCh && legacyItem) ? receiverCh->CanReceiveItem(fromCh, legacyItem) : false;
}

void ReceiveItem(entt::entity receiver, entt::entity from, entt::entity item)
{
    ReceiveItemEcs(receiver, from, item);
}

static uint32_t EntityPlayerID(entt::entity e)
{
    if (const auto* playerID = g_registry.try_get<ecs::PlayerID>(e))
        return playerID->pid;

    if (const auto* ch = LegacyCharOf(e))
        return ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch));

    return 0;
}

bool TransferItemOwnership(entt::entity item, entt::entity from, entt::entity to)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    const uint32_t toPID = EntityPlayerID(to);
    if (toPID == 0)
        return false;

    const uint32_t fromPID = EntityPlayerID(from);
    auto& owner = g_registry.get_or_emplace<ecs::ItemOwner>(item);
    owner.lastOwnerPID = fromPID != 0 ? fromPID : owner.ownerPID;
    owner.ownerPID = toPID;
    owner.ownershipPID = toPID;
    return true;
}

bool ReceiveItemEcs(entt::entity receiver, entt::entity from, entt::entity item)
{
    auto* receiverCh = LegacyCharOf(receiver);
    auto* fromCh = LegacyCharOf(from);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!receiverCh || !fromCh || !legacyItem)
        return false;

    if (!receiverCh->CanReceiveItem(fromCh, legacyItem))
        return false;

    const uint32_t itemID = legacyItem->GetID();
    receiverCh->ReceiveItem(fromCh, legacyItem);

    LPITEM currentItem = itemID != 0 ? ITEM_MANAGER::instance().Find(itemID) : nullptr;
    if (currentItem) {
        SyncItemStateFromLegacy(item);
        return true;
    }

    return DestroyItemEntityAndLegacy(item, "RECEIVE_ITEM_ECS_CONSUMED");
}

SpecialItemGroupResult GiveItemFromSpecialItemGroup(entt::entity e, uint32_t groupNum)
{
    SpecialItemGroupResult result;
    auto* ch = LegacyCharOf(e);
    if (!ch)
        return result;

    std::vector<LPITEM> itemGets;
    if (!ch->GiveItemFromSpecialItemGroup(groupNum, result.itemVnums, result.itemCounts, itemGets, result.count))
        return result;

    result.itemEntities.reserve(itemGets.size());
    for (LPITEM item : itemGets) {
        result.itemEntities.push_back(item ? EntityFactory::CreateItemEntity(g_registry, item) : entt::null);
    }

    return result;
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

bool DoRefine(entt::entity e, entt::entity item, bool moneyOnly)
{
    auto* ch = LegacyCharOf(e);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!ch || !legacyItem)
        return false;

    const bool result = ch->DoRefine(legacyItem, moneyOnly);
    if (result)
        SyncItemStateFromLegacy(item);
    return result;
}

bool DoRefineWithScroll(entt::entity e, entt::entity item)
{
    auto* ch = LegacyCharOf(e);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!ch || !legacyItem)
        return false;

    const bool result = ch->DoRefineWithScroll(legacyItem);
    if (result)
        SyncItemStateFromLegacy(item);
    return result;
}

bool DoRefineItemSoul(entt::entity e, entt::entity item)
{
    auto* ch = LegacyCharOf(e);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    if (!ch || !legacyItem)
        return false;

    const bool result = ch->DoRefineItemSoul(legacyItem);
    if (result)
        SyncItemStateFromLegacy(item);
    return result;
}

bool RefineInformation(entt::entity e, uint8_t cell, uint8_t type, int additionalCell)
{
    auto* ch = LegacyCharOf(e);
    return ch ? ch->RefineInformation(cell, type, additionalCell) : false;
}

bool RefineItem(entt::entity e, entt::entity item, entt::entity target)
{
    auto* ch = LegacyCharOf(e);
    LPITEM legacyItem = ResolveLegacyItemForLegacySideEffect(item);
    LPITEM legacyTarget = ResolveLegacyItemForLegacySideEffect(target);
    if (!ch || !legacyItem || !legacyTarget)
        return false;

    const bool result = ch->RefineItem(legacyItem, legacyTarget);
    if (result) {
        SyncItemStateFromLegacy(item);
        SyncItemStateFromLegacy(target);
    }
    return result;
}

RefineResult RefineItemEcs(entt::entity e, const RefineInput& input, entt::entity target)
{
    RefineResult result;
    result.success = RefineItem(e, input.item, target);
    result.resultItem = result.success ? input.item : entt::null;
    return result;
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
