#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"
#include "QuestSystem.hpp"
#include "SocialSystem.hpp"
#include "InventorySystem.hpp"

#include "ItemSystem.hpp"
#include "MountSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "PointSystem.hpp"
#include "AffectSystem.hpp"
#include "../EntityFactory.hpp"
#include "../ItemInvariants.hpp"
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
#include "../SpatialHelpers.hpp"
#include "../components/identity_components.hpp"
#include "../components/inventory_components.hpp"
#include "../events.hpp"
#include "../EventDispatcher.hpp"
#include "../components/item_components.hpp"
#include "../components/visibility_components.hpp"
#include "../components/item_proto_components.hpp"
#include "../components/session_components.hpp"
#include "../components/social_components.hpp"
#include "../components/transform_components.hpp"
#include "../components/vital_components.hpp"
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
			const entt::entity character = pChar->GetEntityHandle();

			if (ecs::PlayerRuntime::IsStone(character))
			{
				m_mapStone[ecs::PlayerRuntime::GetPacketVID(character)] = pChar;
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

static ecs::MainInventoryRuntimeComponent* EnsureMainInventoryRuntimeComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::MainInventoryRuntimeComponent>(e);
}

static const ecs::MainInventoryRuntimeComponent* TryGetMainInventoryRuntimeComponent(entt::entity e);
static entt::entity GetMainInventoryItem(entt::entity e, uint16_t cell);

static const ecs::MainInventoryRuntimeComponent* TryGetMainInventoryRuntimeComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    return g_registry.try_get<ecs::MainInventoryRuntimeComponent>(e);
}

static int GetMainInventoryLimit(entt::entity e)
{
    int limit = INVENTORY_MAX_NUM;
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
    if (const auto* points = g_registry.try_get<ecs::CharacterPoints>(e))
        limit = std::clamp(90 + (5 * points->base.envanter), 0,
                           static_cast<int>(INVENTORY_MAX_NUM));
#endif
    return limit;
}

static entt::entity GetMainInventoryItem(entt::entity e, uint16_t cell)
{
    if (cell >= INVENTORY_AND_EQUIP_SLOT_MAX)
        return entt::null;

    const auto* comp = TryGetMainInventoryRuntimeComponent(e);
    return comp ? comp->items[cell] : entt::null;
}

#ifdef ENABLE_EXTRA_INVENTORY
static ecs::ExtraInventoryRuntimeComponent* EnsureExtraInventoryRuntimeComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::ExtraInventoryRuntimeComponent>(e);
}

#endif

static ecs::DragonSoulInventoryComponent* EnsureDragonSoulInventoryComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::DragonSoulInventoryComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::DragonSoulInventoryComponent>(e);
}

static ecs::CubeWindowComponent* EnsureCubeWindowComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::CubeWindowComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::CubeWindowComponent>(e);
}


#ifdef ENABLE_ACCE_SYSTEM
static ecs::AcceWindowComponent* EnsureAcceWindowComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::AcceWindowComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::AcceWindowComponent>(e);
}

#endif

#ifdef ENABLE_SWITCHBOT
static ecs::SwitchbotRuntimeComponent* EnsureSwitchbotRuntimeComponent(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::SwitchbotRuntimeComponent>(e))
        return comp;

    return &g_registry.emplace<ecs::SwitchbotRuntimeComponent>(e);
}

#endif

static LPITEM LegacyItemBoundary(entt::entity itemEntity)
{
    if (itemEntity == entt::null || !g_registry.valid(itemEntity))
        return nullptr;

    const auto* legacy = g_registry.try_get<ecs::LegacyItemPtr>(itemEntity);
    return legacy ? legacy->ptr : nullptr;
}

static bool DestroyItemEntityAndLegacy(entt::entity itemEntity, const char* reason)
{
    if (!ItemSystem::IsValidItem(itemEntity))
        return false;

    const auto* legacy = g_registry.try_get<ecs::LegacyItemPtr>(itemEntity);
    if (legacy && legacy->ptr)
        ITEM_MANAGER::instance().RemoveItem(itemEntity, reason);
    else
        M2_DESTROY_ITEM(itemEntity);

    // A reentrant/busy or otherwise rejected manager cleanup is not success.
    // Never bypass it with a second raw factory destruction.
    return !g_registry.valid(itemEntity);
}

static void SyncItemCountComponent(LPITEM item, int count)
{
	entt::entity e = item ? item->GetEntityHandle() : entt::null;
    if (e == entt::null)
        return;

    g_registry.emplace_or_replace<ecs::ItemCount>(e, ecs::ItemCount{count});
}

static void SyncItemFlagsComponent(LPITEM item)
{
	entt::entity e = item ? item->GetEntityHandle() : entt::null;
    if (e == entt::null)
        return;

    ecs::ItemFlags flags{};
    flags.flags = item->GetFlag();
    flags.exchanging = item->IsExchanging();
    flags.skipSave = ItemSystem::GetItemSkipSave(e);
    flags.isLocked = item->isLocked();
    g_registry.emplace_or_replace<ecs::ItemFlags>(e, flags);
}

#ifndef ENABLE_SWITCHBOT
const int MAX_NORM_ATTR_NUM = ITEM_MANAGER::MAX_NORM_ATTR_NUM;
const int MAX_RARE_ATTR_NUM = ITEM_MANAGER::MAX_RARE_ATTR_NUM;
#endif

static void SyncItemAttributesComponent(LPITEM item)
{
	entt::entity e = item ? item->GetEntityHandle() : entt::null;
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
	entt::entity e = item ? item->GetEntityHandle() : entt::null;
    if (e == entt::null)
        return;

    ecs::ItemSockets sockets{};
    const int32_t* values = item->GetSockets();
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        sockets.sockets[i] = values[i];

    g_registry.emplace_or_replace<ecs::ItemSockets>(e, sockets);
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


// item socket º¹»ç -- by mhh

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
    if (e == entt::null || !g_registry.valid(e))
        return entt::null;

    switch (cell.window_type)
    {
    case INVENTORY:
        return GetMainInventoryItem(e, cell.cell);
    case EQUIPMENT:
        return GetMainInventoryItem(e, static_cast<uint16_t>(INVENTORY_MAX_NUM + cell.cell));
    case DRAGON_SOUL_INVENTORY:
        if (cell.cell < DRAGON_SOUL_INVENTORY_MAX_NUM)
            if (const auto* inventory = g_registry.try_get<ecs::DragonSoulInventoryComponent>(e))
                return inventory->items[cell.cell];
        return entt::null;
#ifdef ENABLE_EXTRA_INVENTORY
    case EXTRA_INVENTORY:
        if (cell.cell < EXTRA_INVENTORY_MAX_NUM)
            if (const auto* inventory = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e))
                return inventory->items[cell.cell];
        return entt::null;
#endif
#ifdef ENABLE_SWITCHBOT
    case SWITCHBOT:
        if (cell.cell < SWITCHBOT_SLOT_COUNT)
            if (const auto* switchbot = g_registry.try_get<ecs::SwitchbotRuntimeComponent>(e))
                return switchbot->items[cell.cell];
        return entt::null;
#endif
    default:
        return entt::null;
    }
}

entt::entity GetInventoryItem(entt::entity e, uint16_t cell)
{
    return GetMainInventoryItem(e, cell);
}

#ifdef ENABLE_EXTRA_INVENTORY
entt::entity GetExtraInventoryItem(entt::entity e, uint16_t cell)
{
    if (e == entt::null || !g_registry.valid(e) || cell >= EXTRA_INVENTORY_MAX_NUM)
        return entt::null;

    const auto* inventory = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e);
    return inventory ? inventory->items[cell] : entt::null;
}

void SyncExtraInventoryAll(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e))
        return;

    const auto* session = g_registry.try_get<ecs::NetworkSession>(e);
    if (!session || !session->desc)
        return;

    const auto* inventory = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e);
    if (!inventory)
        return;

    for (uint16_t cell = 0; cell < EXTRA_INVENTORY_MAX_NUM; ++cell)
    {
        const entt::entity item = inventory->items[cell];
        const TItemPos packetCell(EXTRA_INVENTORY, cell);

        if (IsValidItem(item))
        {
            TPacketGCItemSet packet{};
            packet.header = HEADER_GC_ITEM_SET;
            packet.Cell = packetCell;
            packet.count = GetItemCount(item);
#ifdef ATTR_LOCK
            if (const auto* locked = g_registry.try_get<ecs::ItemLockedAttribute>(item))
                packet.lockedattr = locked->index;
            else
                packet.lockedattr = -1;
#endif
            packet.vnum = GetItemVnum(item);
            packet.flags = GetItemFlags(item);
            packet.anti_flags = GetItemAntiFlags(item);
            packet.highlight = false;

            for (int index = 0; index < ITEM_SOCKET_MAX_NUM; ++index)
                packet.alSockets[index] = GetItemSocket(item, index);
            for (int index = 0; index < ITEM_ATTRIBUTE_MAX_NUM; ++index)
                packet.aAttr[index] = GetItemAttribute(item, index);

            session->desc->Packet(&packet, sizeof(packet));
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

            session->desc->Packet(&packet, sizeof(packet));
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
    if (e == entt::null || !g_registry.valid(e) || vnum == 0)
        return entt::null;

#ifdef ENABLE_EXTRA_INVENTORY
    if (reinforce)
    {
        const auto* inventory = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e);
        if (!inventory)
            return entt::null;
        for (const entt::entity item : inventory->items)
            if (IsValidItem(item) && GetItemVnum(item) == vnum)
                return item;
        return entt::null;
    }
#endif

    const auto* inventory = TryGetMainInventoryRuntimeComponent(e);
    if (!inventory)
        return entt::null;
    const int limit = GetMainInventoryLimit(e);
    for (int cell = 0; cell < limit; ++cell)
    {
        const entt::entity item = inventory->items[cell];
        if (IsValidItem(item) && GetItemVnum(item) == vnum)
            return item;
    }
    return entt::null;
}

entt::entity FindItemByID(entt::entity e, uint32_t id)
{
    if (e == entt::null || !g_registry.valid(e) || id == 0)
        return entt::null;

    const auto matches = [id](entt::entity item) {
        return IsValidItem(item) && GetItemID(item) == id;
    };

    if (const auto* inventory = TryGetMainInventoryRuntimeComponent(e))
    {
        const int limit = GetMainInventoryLimit(e);
        for (int cell = 0; cell < limit; ++cell)
            if (matches(inventory->items[cell]))
                return inventory->items[cell];
        for (int cell = BELT_INVENTORY_SLOT_START; cell < BELT_INVENTORY_SLOT_END; ++cell)
            if (matches(inventory->items[cell]))
                return inventory->items[cell];
    }

#ifdef ENABLE_EXTRA_INVENTORY
    if (const auto* extra = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e))
        for (const entt::entity item : extra->items)
            if (matches(item))
                return item;
#endif

    return entt::null;
}

entt::entity FindItemByID(uint32_t id)
{
    if (id == 0)
        return entt::null;

    const entt::entity item = CItemRegistry::Instance().Find(id);
    return item != entt::null && g_registry.valid(item) ? item : entt::null;
}

entt::entity FindItemByVID(uint32_t vid)
{
    if (vid == 0)
        return entt::null;

    const entt::entity item = CItemRegistry::Instance().FindByVID(vid);
    return item != entt::null && g_registry.valid(item) ? item : entt::null;
}

int CountItemRenewal(entt::entity e, uint32_t vnum)
{
    if (e == entt::null || !g_registry.valid(e) || vnum == 0)
        return 0;

    const auto* shop = g_registry.try_get<ecs::ShopState>(e);
    const auto countItem = [vnum, shop](entt::entity item) -> int {
        if (!IsValidItem(item) || GetItemVnum(item) != vnum ||
            GetItemLockedAttributeIndex(item) != -1)
            return 0;
        if (shop && shop->myShop && shop->myShop->IsSellingItem(GetItemID(item)))
            return 0;
        return static_cast<int>(GetItemCount(item));
    };

    int count = 0;
#ifdef ENABLE_EXTRA_INVENTORY
    if (ITEM_MANAGER::instance().IsExtraItem(vnum))
    {
        if (const auto* extra = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e))
            for (const entt::entity item : extra->items)
                count += countItem(item);
        return count;
    }
#endif

    if (const auto* inventory = TryGetMainInventoryRuntimeComponent(e))
        for (int cell = 0, limit = GetMainInventoryLimit(e); cell < limit; ++cell)
            count += countItem(inventory->items[cell]);
    return count;
}

int CountItem(entt::entity e, uint32_t vnum)
{
    if (e == entt::null || !g_registry.valid(e) || vnum == 0)
        return 0;

    const auto* shop = g_registry.try_get<ecs::ShopState>(e);
    const auto countItem = [vnum, shop](entt::entity item) -> int {
        if (!IsValidItem(item) || GetItemVnum(item) != vnum)
            return 0;
        if (shop && shop->myShop && shop->myShop->IsSellingItem(GetItemID(item)))
            return 0;
        return static_cast<int>(GetItemCount(item));
    };

    int count = 0;
#ifdef ENABLE_EXTRA_INVENTORY
    if (ITEM_MANAGER::instance().IsExtraItem(vnum))
    {
        if (const auto* extra = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e))
            for (const entt::entity item : extra->items)
                count += countItem(item);
        return count;
    }
#endif

    if (const auto* inventory = TryGetMainInventoryRuntimeComponent(e))
        for (int cell = 0, limit = GetMainInventoryLimit(e); cell < limit; ++cell)
            count += countItem(inventory->items[cell]);
    return count;
}

int CountTypeItem(entt::entity e, uint8_t type)
{
    if (e == entt::null || !g_registry.valid(e))
        return 0;

    int count = 0;
    if (const auto* inventory = TryGetMainInventoryRuntimeComponent(e))
    {
        for (int cell = 0, limit = GetMainInventoryLimit(e); cell < limit; ++cell)
        {
            const entt::entity item = inventory->items[cell];
            if (IsValidItem(item) && GetItemType(item) == type)
                count += static_cast<int>(GetItemCount(item));
        }
    }
    return count;
}

bool HasItem(entt::entity e, uint32_t vnum, uint32_t count)
{
    return CountItem(e, vnum) >= static_cast<int>(count);
}

bool RemoveSpecifyItemEcs(entt::entity e, uint32_t vnum, uint32_t count,
                          bool cubeRenewal)
{
    if (e == entt::null || !g_registry.valid(e) || count == 0)
        return false;

    const int available = cubeRenewal ? CountItemRenewal(e, vnum) : CountItem(e, vnum);
    if (available < static_cast<int>(count))
        return false;

    const auto* shop = g_registry.try_get<ecs::ShopState>(e);
    auto consumeMatching = [&](entt::entity item) {
        if (count == 0 || !IsValidItem(item) || GetItemVnum(item) != vnum)
            return;
        if (cubeRenewal && GetItemLockedAttributeIndex(item) != -1)
            return;
        if (shop && shop->myShop && shop->myShop->IsSellingItem(GetItemID(item)))
            return;

        if (vnum >= 80003 && vnum <= 80007)
            LogManager::instance().GoldBarLog(
                ecs::PlayerRuntime::GetPlayerID(e), GetItemID(item), QUEST,
                "RemoveSpecifyItemEcs");

        const uint32_t amount = std::min(count, GetItemCount(item));
        if (ConsumeItemEcs(item, amount))
            count -= amount;
    };

#ifdef ENABLE_EXTRA_INVENTORY
    if (ITEM_MANAGER::instance().IsExtraItem(vnum))
    {
        if (const auto* extra = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e))
            for (int cell = 0; cell < EXTRA_INVENTORY_MAX_NUM && count > 0; ++cell)
                consumeMatching(extra->items[cell]);
        return count == 0;
    }
#endif

    if (const auto* inventory = TryGetMainInventoryRuntimeComponent(e))
        for (int cell = 0, limit = GetMainInventoryLimit(e); cell < limit && count > 0; ++cell)
            consumeMatching(inventory->items[cell]);
    return count == 0;
}

entt::entity GetWearItem(entt::entity e, uint8_t wearPos)
{
    if (wearPos >= WEAR_MAX_NUM)
        return entt::null;

    return GetMainInventoryItem(e, static_cast<uint16_t>(INVENTORY_MAX_NUM + wearPos));
}

bool IsEquipUniqueItem(entt::entity e, uint32_t itemVnum)
{
    for (const uint8_t wearSlot : { WEAR_UNIQUE1, WEAR_UNIQUE2, WEAR_COSTUME_MOUNT })
    {
        const entt::entity item = GetWearItem(e, wearSlot);
        if (IsValidItem(item) && GetItemVnum(item) == itemVnum)
            return true;
    }

    return itemVnum == UNIQUE_ITEM_RING_OF_LANGUAGE &&
        IsEquipUniqueItem(e, UNIQUE_ITEM_RING_OF_LANGUAGE_SAMPLE);
}

bool IsEquipUniqueGroup(entt::entity e, uint32_t groupVnum)
{
    for (const uint8_t wearSlot : { WEAR_UNIQUE1, WEAR_UNIQUE2, WEAR_COSTUME_MOUNT })
    {
        const entt::entity item = GetWearItem(e, wearSlot);
        if (IsValidItem(item) &&
            GetItemSpecialGroup(item) == static_cast<int32_t>(groupVnum))
            return true;
    }
    return false;
}

bool UnEquipSpecialRideUniqueItem(entt::entity e)
{
    for (const uint8_t wearSlot : { WEAR_UNIQUE1, WEAR_UNIQUE2, WEAR_COSTUME_MOUNT })
    {
        const entt::entity item = GetWearItem(e, wearSlot);
        if (IsValidItem(item) && GetItemSpecialGroup(item) == UNIQUE_GROUP_SPECIAL_RIDE)
            return UnequipItemEcs(e, item);
    }
    return true;
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

static bool UseNonEquipItemLegacyBoundary(entt::entity owner,
                                          entt::entity item,
                                          TItemPos destCell)
{
    LPCHARACTER legacyOwner = LegacyCharOf(owner);
    LPITEM legacyItem = LegacyItemBoundary(item);
    if (!legacyOwner || !IsValidItem(item))
        return false;

    const bool result = legacyOwner->UseItemEx(legacyItem, destCell);
    if (!result)
        return false;

    if (g_registry.valid(item) && IsValidItem(item)) {
        SyncItemStateFromLegacy(item);
        return true;
    }

    return true;
}

bool UseItemEcs(entt::entity owner, entt::entity item, TItemPos destCell)
{
    if (owner == entt::null || !g_registry.valid(owner) || !IsValidItem(item))
        return false;

    const uint8_t itemType = GetItemType(item);
    if (destCell == NPOS && IsSimpleEquipToggleType(itemType)) {
        return IsItemEquipped(item)
            ? UnequipItemEcs(owner, item)
            : EquipItemEcs(owner, item);
    }

    return UseNonEquipItemLegacyBoundary(owner, item, destCell);
}

namespace {
entt::entity GiveExistingItemEcs(entt::entity owner, entt::entity item,
                                 bool longOwnerShip);
}


void AutoGiveItem(entt::entity e, entt::entity item, bool longOwnerShip
#ifdef __HIGHLIGHT_SYSTEM__
                  , bool isHighLight
#endif
)
{
#ifdef __HIGHLIGHT_SYSTEM__
    (void)isHighLight;
#endif
    GiveExistingItemEcs(e, item, longOwnerShip);
}

#ifdef ENABLE_DS_REFINE_ALL
bool AutoGiveDS(entt::entity e, entt::entity item, bool longOwnerShip)
{
    if (!IsDragonSoulItem(item))
        return false;

    return GiveExistingItemEcs(e, item, longOwnerShip) != entt::null;
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

static void SendAutoGiveMessage(entt::entity owner, uint32_t count,
                                entt::entity item)
{
    if (owner == entt::null || !g_registry.valid(owner) || !IsValidItem(item))
        return;

#ifdef TEXTS_IMPROVEMENT
    const LPDESC desc = ecs::PlayerRuntime::GetDesc(owner);
    const uint8_t language = desc ? desc->GetLanguage() : 0;
    const TItemTable* proto = GetItemProto(item);
    const char* itemName = proto ? proto->szLocaleName[language] : GetItemName(item);
    ecs::ChatSystem::SendNew(owner,
#ifdef ENABLE_NEW_CHAT
                             CHAT_TYPE_INFO_ITEM
#else
                             CHAT_TYPE_INFO
#endif
                             , 102, "%d#%s", count, itemName);
#else
    (void)count;
    (void)item;
#endif
}

static entt::entity HandleBlendItemMerge(entt::entity owner, entt::entity item)
{
    if (owner == entt::null || !g_registry.valid(owner) ||
        !IsValidItem(item) || GetItemType(item) != ITEM_BLEND)
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
        DestroyItemEntityEcs(item, "AUTOGIVE_BLEND_MERGE");
        return candidate;
    }

    return item;
}

static entt::entity PlaceItemInInventory(entt::entity owner, entt::entity item,
                                         bool longOwnerShip,
                                         bool sendMessage,
                                         uint32_t messageCount)
{
    if (owner == entt::null || !g_registry.valid(owner) ||
        !g_registry.any_of<ecs::PlayerID>(owner) || !IsValidItem(item))
        return entt::null;

    const int cell = GetEmptyInventoryPositionEcs(owner, item);
    uint8_t targetWindow = INVENTORY;
    if (IsDragonSoulItem(item))
        targetWindow = DRAGON_SOUL_INVENTORY;
#ifdef ENABLE_EXTRA_INVENTORY
    else if (IsExtraItem(item))
        targetWindow = EXTRA_INVENTORY;
#endif

    if (cell != -1) {
        if (!PlaceItemEcs(owner, item, targetWindow, static_cast<uint16_t>(cell)))
            return entt::null;

        LogManager::instance().ItemLogEntity(owner, item, "SYSTEM", GetItemName(item));
        if (sendMessage)
            SendAutoGiveMessage(owner, messageCount, item);

        if (GetItemType(item) == ITEM_USE && GetItemSubType(item) == USE_POTION) {
            TQuickslot currentSlot {};
            if (InventorySystem::GetQuickslot(owner, 0, currentSlot) &&
                currentSlot.type == QUICKSLOT_TYPE_NONE) {
                TQuickslot slot {};
                slot.type = QUICKSLOT_TYPE_ITEM;
                slot.pos = static_cast<uint8_t>(cell);
                InventorySystem::SetQuickslot(owner, 0, slot);
                if (LPDESC desc = ecs::PlayerRuntime::GetDesc(owner)) {
                    packet_quickslot_add packet {};
                    packet.header = HEADER_GC_QUICKSLOT_ADD;
                    packet.pos = 0;
                    packet.slot = slot;
                    desc->Packet(&packet, sizeof(packet));
                }
            }
        }

        return item;
    }

    PIXEL_POSITION position {
        ecs::PlayerRuntime::GetX(owner), ecs::PlayerRuntime::GetY(owner), 0
    };
    if (const auto* spatialPosition = g_registry.try_get<ecs::Position>(owner))
        position.z = spatialPosition->z;
#ifdef ENABLE_NEWSTUFF
    const int destroySeconds = g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE];
#else
    const int destroySeconds = 300;
#endif
    if (!PlaceItemOnGroundLegacyBoundary(
            item, ecs::PlayerRuntime::GetMapIndex(owner), position, destroySeconds))
        return entt::null;
    SetGroundOwnership(item, owner, longOwnerShip ? 300 : 60);
    LogManager::instance().ItemLogEntity(
        owner, item, "SYSTEM_DROP", GetItemName(item));
    return item;
}

static entt::entity MergeExistingStackIntoInventory(entt::entity owner,
                                                    entt::entity item)
{
    if (!IsValidItem(item) ||
        !IS_SET(GetItemFlags(item), ITEM_FLAG_STACKABLE) ||
        IS_SET(GetItemAntiFlags(item), ITEM_ANTIFLAG_STACK) ||
        IsDragonSoulItem(item))
        return item;

    const auto* ownerID = g_registry.try_get<ecs::PlayerID>(owner);
    const auto* incomingSockets = g_registry.try_get<ecs::ItemSockets>(item);
    if (!ownerID || ownerID->pid == 0 || !incomingSockets)
        return item;

    const uint8_t targetWindow =
#ifdef ENABLE_EXTRA_INVENTORY
        IsExtraItem(item) ? EXTRA_INVENTORY :
#endif
        INVENTORY;
    const uint32_t itemVnum = GetItemVnum(item);
    uint32_t remaining = GetItemCount(item);
    entt::entity lastMerged = entt::null;

    auto view = g_registry.view<ecs::ItemIdentity, ecs::ItemOwner,
                                ecs::ItemLocation, ecs::ItemCount,
                                ecs::ItemSockets>();
    for (auto candidate : view) {
        if (candidate == item || remaining == 0)
            continue;

        const auto& identity = view.get<ecs::ItemIdentity>(candidate);
        const auto& candidateOwner = view.get<ecs::ItemOwner>(candidate);
        const auto& location = view.get<ecs::ItemLocation>(candidate);
        const auto& count = view.get<ecs::ItemCount>(candidate);
        const auto& sockets = view.get<ecs::ItemSockets>(candidate);
        if (candidateOwner.ownerPID != ownerID->pid ||
            location.window != targetWindow || identity.vnum != itemVnum ||
            sockets.sockets != incomingSockets->sockets ||
            count.count >= g_bItemCountLimit)
            continue;

        const uint32_t capacity = static_cast<uint32_t>(g_bItemCountLimit - count.count);
        const uint32_t merged = std::min(capacity, remaining);
        SetItemCount(candidate, static_cast<uint32_t>(count.count) + merged);
        remaining -= merged;
        lastMerged = candidate;
    }

    if (remaining == 0) {
        DestroyItemEntityEcs(item, "AUTOGIVE_STACK_MERGE");
        return lastMerged;
    }

    if (remaining != GetItemCount(item))
        SetItemCount(item, remaining);
    return item;
}

entt::entity GiveExistingItemEcs(entt::entity owner, entt::entity item,
                                 bool longOwnerShip)
{
    if (owner == entt::null || !g_registry.valid(owner) ||
        !g_registry.any_of<ecs::PlayerID>(owner) || !IsValidItem(item))
        return entt::null;

    if (const auto* itemOwner = g_registry.try_get<ecs::ItemOwner>(item);
        itemOwner && itemOwner->ownerPID != 0)
        return entt::null;

    const entt::entity merged = MergeExistingStackIntoInventory(owner, item);
    if (merged != item)
        return merged;

    return PlaceItemInInventory(owner, item, longOwnerShip, false,
                                GetItemCount(item));
}

} // namespace

entt::entity AutoGiveItemEcs(entt::entity owner, uint32_t itemVnum,
                             uint32_t count, int rarePct,
                             bool sendMessage)
{
    if (owner == entt::null || !g_registry.valid(owner) ||
        !g_registry.any_of<ecs::PlayerID>(owner))
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
                SendAutoGiveMessage(owner, requestedCount, merged);
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
                SendAutoGiveMessage(owner, requestedCount, merged);
            return merged;
        }
    }

    entt::entity created = ITEM_MANAGER::instance().CreateItem(itemVnum, count, 0, true, rarePct);
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

bool IsDragonSoulItem(entt::entity item)
{
    return IsValidItem(item) && GetItemType(item) == ITEM_DS;
}

bool IsExtraItem(entt::entity item)
{
#ifdef ENABLE_EXTRA_INVENTORY
    return IsValidItem(item) && ITEM_MANAGER::instance().IsExtraItem(GetItemVnum(item));
#else
    (void)item;
    return false;
#endif
}

bool IsRideItem(entt::entity item)
{
    if (!IsValidItem(item))
        return false;
    const uint8_t type = GetItemType(item);
    const uint8_t subType = GetItemSubType(item);
    if (type == ITEM_UNIQUE &&
        (subType == UNIQUE_SPECIAL_RIDE || subType == UNIQUE_SPECIAL_MOUNT_RIDE))
        return true;
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    return type == ITEM_COSTUME && subType == COSTUME_MOUNT;
#else
    return false;
#endif
}

bool IsMountItem(entt::entity item)
{
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
    return IsValidItem(item) && GetItemType(item) == ITEM_COSTUME &&
        GetItemSubType(item) == COSTUME_MOUNT;
#else
    (void)item;
    return false;
#endif
}

bool IsNewMountItem(entt::entity item)
{
    const uint32_t vnum = GetItemVnum(item);
    return vnum >= 76000 && vnum <= 76014;
}

#ifdef ENABLE_RUNE_SYSTEM
bool IsRuneItem(entt::entity item)
{
    return IsValidItem(item) && GetItemType(item) == ITEM_COSTUME &&
        GetItemSubType(item) >= RUNE_SLOT1 && GetItemSubType(item) <= RUNE_SLOT7;
}

bool ActivateRuneLegacyBoundary(entt::entity item)
{
    LPITEM legacyItem = LegacyItemBoundary(item);
    if (!legacyItem)
        return false;
    legacyItem->ActivateRune();
    return SyncItemStateFromLegacy(item);
}

bool DeactivateRuneLegacyBoundary(entt::entity item)
{
    LPITEM legacyItem = LegacyItemBoundary(item);
    if (!legacyItem)
        return false;
    legacyItem->DeactivateRune();
    return SyncItemStateFromLegacy(item);
}

bool ChangeRuneAttributesLegacyBoundary(entt::entity item, int32_t time)
{
    LPITEM legacyItem = LegacyItemBoundary(item);
    if (!legacyItem)
        return false;
    legacyItem->ChangeRuneAttr(time);
    return SyncItemStateFromLegacy(item);
}

bool ActivateRuneBonusLegacyBoundary(entt::entity item)
{
    LPITEM legacyItem = LegacyItemBoundary(item);
    if (!legacyItem)
        return false;
    legacyItem->ActivateRuneBonus();
    return SyncItemStateFromLegacy(item);
}

bool DeactivateRuneBonusLegacyBoundary(entt::entity item)
{
    LPITEM legacyItem = LegacyItemBoundary(item);
    if (!legacyItem)
        return false;
    legacyItem->DeactivateRuneBonus();
    return SyncItemStateFromLegacy(item);
}
#endif

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
    if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(item))
        return identity->originalVnum;

    return 0;
}

TItemExtraProto* GetItemExtraProto(entt::entity item)
{
    if (item == entt::null || !g_registry.valid(item))
        return nullptr;

    const auto* ref = g_registry.try_get<ecs::ItemExtraProtoRef>(item);
    return ref ? ref->proto : nullptr;
}

void SetItemExtraProto(entt::entity item, TItemExtraProto* proto)
{
    if (item == entt::null || !g_registry.valid(item))
        return;

    g_registry.get_or_emplace<ecs::ItemExtraProtoRef>(item).proto = proto;
}

uint32_t GetItemSIGVnum(entt::entity item)
{
    if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(item))
        return identity->sigVnum;

    return 0;
}

int32_t GetItemSpecialGroup(entt::entity item)
{
    if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(item))
        return identity->specialGroup;

    return 0;
}

uint32_t GetItemTransmutationVnum(entt::entity item)
{
    if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(item))
        return identity->transmutationVnum;

    return 0;
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
        return count->count > 0 ? static_cast<uint32_t>(count->count) : 0;

    return 0;
}

int32_t GetItemValue(entt::entity item, uint32_t index)
{
    if (index >= ITEM_VALUES_MAX_NUM)
        return 0;

    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef && protoRef->proto)
        return protoRef->proto->alValues[index];

    return 0;
}

int64_t GetItemShopBuyPrice(entt::entity item)
{
    const TItemTable* proto = GetItemProto(item);
    return proto ? static_cast<int64_t>(proto->dwShopBuyPrice) : 0;
}

const char* GetItemName(entt::entity item)
{
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef)
        return protoRef->name;

    return "";
}

const char* GetItemNameByVnum(uint32_t vnum)
{
    const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
    if (!proto)
        return "";

#ifdef ENABLE_MULTI_NAMES
    return proto->szLocaleName[DEFAULT_LANGUAGE];
#else
    return proto->szLocaleName;
#endif
}

uint8_t GetItemSize(entt::entity item)
{
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef)
        return protoRef->size;

    return 0;
}

uint8_t GetItemExtraCategory(entt::entity item)
{
#ifdef ENABLE_EXTRA_INVENTORY
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    return protoRef ? protoRef->extra_category : 0;
#else
    (void)item;
    return 0;
#endif
}

uint32_t GetItemRefineVnum(entt::entity item)
{
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef)
        return protoRef->refined_vnum;

    return 0;
}

int GetItemRefineLevel(entt::entity item)
{
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef)
        return protoRef->refine_level;

    return 0;
}

int GetItemLevelLimit(entt::entity item)
{
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef)
        return protoRef->level_limit;

    return 0;
}

int GetItemLimitTimerBasedOnWearIndex(entt::entity item)
{
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef)
        return protoRef->limit_timer_wear_index;

    return -1;
}

int GetItemDuration(entt::entity item)
{
    const TItemTable* proto = GetItemProto(item);
    if (!proto)
        return -1;

    for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
        if (proto->aLimits[i].bType == LIMIT_REAL_TIME)
            return proto->aLimits[i].lValue;
    }

    if (proto->cLimitTimerBasedOnWearIndex >= 0)
        return proto->aLimits[proto->cLimitTimerBasedOnWearIndex].lValue;

    return -1;
}

uint8_t GetItemLimitType(entt::entity item, uint32_t index)
{
    const TItemTable* proto = GetItemProto(item);
    return proto && index < ITEM_LIMIT_MAX_NUM ? proto->aLimits[index].bType : 0;
}

int32_t GetItemLimitValue(entt::entity item, uint32_t index)
{
    const TItemTable* proto = GetItemProto(item);
    return proto && index < ITEM_LIMIT_MAX_NUM ? proto->aLimits[index].lValue : 0;
}

int32_t GetItemFlags(entt::entity item)
{
    if (const auto* flags = g_registry.try_get<ecs::ItemFlags>(item))
        return flags->flags;

    return 0;
}

uint32_t GetItemWearFlags(entt::entity item)
{
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef)
        return protoRef->wear_flags;

    return 0;
}

uint32_t GetItemWearFlag(entt::entity item)
{
    return GetItemWearFlags(item);
}

uint32_t GetItemAntiFlags(entt::entity item)
{
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef)
        return protoRef->anti_flags;

    return 0;
}

uint32_t GetItemAntiFlag(entt::entity item)
{
    return GetItemAntiFlags(item);
}

uint32_t GetItemImmuneFlags(entt::entity item)
{
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef)
        return protoRef->immune_flags;

    return 0;
}

const TItemTable* GetItemProto(entt::entity item)
{
    const auto* protoRef = g_registry.try_get<ecs::ItemProtoRef>(item);
    if (protoRef)
        return protoRef->proto;

    return nullptr;
}

static void SetItemCountComponentOnly(entt::entity item, uint32_t count)
{
    if (item != entt::null && g_registry.valid(item))
        g_registry.emplace_or_replace<ecs::ItemCount>(item, ecs::ItemCount{static_cast<int>(count)});
}


void SetItemCount(entt::entity item, uint32_t count)
{
    if (item == entt::null || !g_registry.valid(item) || IsItemConsumptionPending(item))
        return;

    if (count == 0) {
        DestroyItemEntityAndLegacy(item, "SET_ITEM_COUNT_ZERO");
        return;
    }

    const auto limit = GetItemType(item) == ITEM_ELK ? INT_MAX : std::max(0, g_bItemCountLimit);
    count = std::min(count, static_cast<uint32_t>(limit));
    SetItemCountComponentOnly(item, count);
    PublishItemCount(item);
}

bool SetItemCountEcs(entt::entity item, uint32_t count)
{
    if (item == entt::null || !g_registry.valid(item) || IsItemConsumptionPending(item))
        return false;

    if (count == 0)
        return DestroyItemEntityAndLegacy(item, "SET_ITEM_COUNT_ECS_ZERO");

    SetItemCount(item, count);
    return true;
}

bool AddItemCountEcs(entt::entity item, int delta)
{
    if (item == entt::null || !g_registry.valid(item) || IsItemConsumptionPending(item))
        return false;

    const int current = static_cast<int>(GetItemCount(item));
    const int64_t next = int64_t(current) + delta;
    if (next <= 0)
        return DestroyItemEntityAndLegacy(item, "ADD_ITEM_COUNT_ECS_ZERO");

    return SetItemCountEcs(item, static_cast<uint32_t>(next));
}

bool ConsumeItem(entt::entity item, uint32_t amount)
{
    if (item == entt::null || !g_registry.valid(item) || amount == 0 || IsItemConsumptionPending(item))
        return false;

    const uint32_t count = GetItemCount(item);
    if (amount > count)
        return false;
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

bool FlushDelayedSaveEcs(entt::entity item)
{
    if (!IsValidItem(item))
        return false;

    ITEM_MANAGER::instance().FlushDelayedSave(item);
    return true;
}

// Handed out by reference so callers can event_cancel(&events.field) the way
// they used to take the address of the member.
// The three limit-table predicates and the socket-accessory test are pure
// proto reads; they were on CItem only because GetProto() was.
static bool HasLimitType(entt::entity item, uint8_t limitType)
{
    const TItemTable* proto = GetItemProto(item);
    if (!proto)
        return false;

    for (const auto& limit : proto->aLimits) {
        if (limitType == limit.bType)
            return true;
    }

    return false;
}

int GetItemAccessorySocketMaxGrade(entt::entity item)
{
    return MINMAX(0, GetItemSocket(item, 1), ITEM_ACCESSORY_SOCKET_MAX_NUM);
}

int GetItemAccessorySocketGrade(entt::entity item)
{
    return MINMAX(0, GetItemSocket(item, 0), GetItemAccessorySocketMaxGrade(item));
}

uint16_t GetItemRefineSet(entt::entity item)
{
    const TItemTable* proto = GetItemProto(item);
    return proto ? proto->wRefineSet : 0;
}

bool IsItemStackable(entt::entity item)
{
    return (GetItemFlags(item) & ITEM_FLAG_STACKABLE) != 0;
}

uint32_t GetItemRefinedVnum(entt::entity item)
{
    const TItemTable* proto = GetItemProto(item);
    return proto ? proto->dwRefinedVnum : 0;
}

bool IsRealTimeItem(entt::entity item)
{
    return HasLimitType(item, LIMIT_REAL_TIME);
}

bool IsRealTimeFirstUseItem(entt::entity item)
{
    return HasLimitType(item, LIMIT_REAL_TIME_START_FIRST_USE);
}

bool IsUnlimitedTimeUnique(entt::entity item)
{
    return HasLimitType(item, LIMIT_UNIQUE_UNLIMITED);
}

bool IsAccessoryForSocket(entt::entity item)
{
    const TItemTable* proto = GetItemProto(item);
    if (!proto)
        return false;

    return (proto->bType == ITEM_ARMOR
            && (proto->bSubType == ARMOR_WRIST || proto->bSubType == ARMOR_NECK
                || proto->bSubType == ARMOR_EAR))
        || (proto->bType == ITEM_BELT);
}

void AccessorySocketDegrade(entt::entity item)
{
	if (GetItemAccessorySocketGrade(item) > 0)
	{
		const entt::entity owner = GetItemOwner(item);
#ifdef TEXTS_IMPROVEMENT
		if (owner != entt::null) {
			ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 117, "%s", GetItemName(item));
		}
#endif

		ModifyPoints(item, false);
		SetItemAccessorySocketGrade(item, GetItemAccessorySocketGrade(item) - 1);
		ModifyPoints(item, true);

		int iDownTime = aiAccessorySocketDegradeTime[GetItemAccessorySocketGrade(item)];

		if (test_server)
			iDownTime /= 60;

		SetItemAccessorySocketDownGradeTime(item, iDownTime);

		if (iDownTime)
			StartAccessorySocketExpireEvent(item);
	}
}

void SetItemAccessorySocketGrade(entt::entity item, int iGrade
#ifdef ENABLE_INFINITE_RAFINES
    , bool infinite
#endif
)
{
    SetItemSocket(item, 0, MINMAX(0, iGrade, GetItemAccessorySocketMaxGrade(item)));

    const int iDownTime =
#ifdef ENABLE_INFINITE_RAFINES
        infinite == true ? 86410 : aiAccessorySocketDegradeTime[GetItemAccessorySocketGrade(item)];
#else
        aiAccessorySocketDegradeTime[GetItemAccessorySocketGrade(item)]
#endif
        ;

    SetItemAccessorySocketDownGradeTime(item, iDownTime);
}

void SetItemAccessorySocketMaxGrade(entt::entity item, int iMaxGrade)
{
    SetItemSocket(item, 1, MINMAX(0, iMaxGrade, ITEM_ACCESSORY_SOCKET_MAX_NUM));
}

int GetItemAccessorySocketDownGradeTime(entt::entity item)
{
#ifdef ENABLE_INFINITE_RAFINES
    return GetItemSocket(item, 2);
#else
    return MINMAX(0, GetItemSocket(item, 2),
        aiAccessorySocketDegradeTime[GetItemAccessorySocketGrade(item)]);
#endif
}

void SetItemAccessorySocketDownGradeTime(entt::entity item, uint32_t time)
{
    SetItemSocket(item, 2, time);
}

void StartDestroyEvent(entt::entity item, int iSec)
{
	auto& events = GetItemEvents(item);
	if (events.destroy)
		return;

	item_event_info* info = AllocEventInfo<item_event_info>();
	info->item = item;

	events.destroy = event_create(item_destroy_event, info, PASSES_PER_SEC(iSec));
}

void StartUniqueExpireEvent(entt::entity item)
{
	auto& events = GetItemEvents(item);
	if (GetItemType(item) != ITEM_UNIQUE)
		return;

	if (events.uniqueExpire)
		return;

	if (IsRealTimeItem(item) || IsRealTimeFirstUseItem(item) || IsUnlimitedTimeUnique(item))
		return;

	// HARD CODING
	/*if (GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		m_pOwner->ShowAlignment(false);*/

	int iSec = GetItemSocket(item, ITEM_SOCKET_UNIQUE_SAVE_TIME);

	if (iSec == 0)
		iSec = 60;
	else
		iSec = MIN(iSec, 60);

	SetItemSocket(item, ITEM_SOCKET_UNIQUE_SAVE_TIME, 0);

	item_event_info* info = AllocEventInfo<item_event_info>();
	info->item = item;

	events.uniqueExpire = event_create(unique_expire_event, info, PASSES_PER_SEC(iSec));

	const entt::entity e = item;
	if (e != entt::null)
		g_dispatcher.trigger(ecs::EvItemExpired { e, GetItemID(item) });
}

void StopUniqueExpireEvent(entt::entity item)
{
	auto& events = GetItemEvents(item);
	if (!events.uniqueExpire)
		return;

	if (GetItemValue(item, 2) != 0)
		return;

	// HARD CODING
	/*if (GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		m_pOwner->ShowAlignment(true);*/

	SetItemSocket(item, ITEM_SOCKET_UNIQUE_SAVE_TIME, event_time(events.uniqueExpire) / passes_per_sec);
	event_cancel(&events.uniqueExpire);

	ITEM_MANAGER::instance().FlushDelayedSave(item);
}

void StartTimerBasedOnWearExpireEvent(entt::entity item)
{
	auto& events = GetItemEvents(item);
	if (events.timerBasedOnWearExpire)
		return;

	if (IsRealTimeItem(item))
		return;

	if (-1 == GetItemProto(item)->cLimitTimerBasedOnWearIndex)
		return;

	int iSec = GetItemSocket(item, 0);

	if (0 != iSec)
	{
		iSec %= 60;
		if (0 == iSec)
			iSec = 60;
	}

	item_event_info* info = AllocEventInfo<item_event_info>();
	info->item = item;

	events.timerBasedOnWearExpire = event_create(timer_based_on_wear_expire_event, info, PASSES_PER_SEC(iSec));

	const entt::entity e = item;
	if (e != entt::null)
		g_dispatcher.trigger(ecs::EvItemExpired { e, GetItemID(item) });
}

void StopTimerBasedOnWearExpireEvent(entt::entity item)
{
	auto& events = GetItemEvents(item);
	if (!events.timerBasedOnWearExpire)
		return;

	int remain_time = GetItemSocket(item, ITEM_SOCKET_REMAIN_SEC) - event_processing_time(events.timerBasedOnWearExpire) / passes_per_sec;

	SetItemSocket(item, ITEM_SOCKET_REMAIN_SEC, remain_time);
	event_cancel(&events.timerBasedOnWearExpire);

	ITEM_MANAGER::instance().FlushDelayedSave(item);
}

void StartAccessorySocketExpireEvent(entt::entity item)
{
	auto& events = GetItemEvents(item);
	if (!IsAccessoryForSocket(item))
		return;

	if (events.accessorySocketExpire)
		return;

	if (GetItemAccessorySocketMaxGrade(item) == 0)
		return;

	if (GetItemAccessorySocketGrade(item) == 0)
		return;

	int iSec = GetItemAccessorySocketDownGradeTime(item);
#ifdef ENABLE_INFINITE_RAFINES
	if (iSec > 86400) {
		return;
	}
#endif
	events.accessorySocketExpire = nullptr;

	if (iSec <= 1)
		iSec = 5;
	else
		iSec = MIN(iSec, 60);

	item_vid_event_info* info = AllocEventInfo<item_vid_event_info>();
	info->item = item;

	events.accessorySocketExpire = event_create(accessory_socket_expire_event, info, PASSES_PER_SEC(iSec));

	const entt::entity e = item;
	if (e != entt::null)
		g_dispatcher.trigger(ecs::EvItemExpired { e, GetItemID(item) });
}

void StopAccessorySocketExpireEvent(entt::entity item)
{
	auto& events = GetItemEvents(item);
	if (!events.accessorySocketExpire)
		return;

	if (!IsAccessoryForSocket(item))
		return;

	int new_time = GetItemAccessorySocketDownGradeTime(item) - (60 - event_time(events.accessorySocketExpire) / passes_per_sec);

	event_cancel(&events.accessorySocketExpire);

	if (new_time <= 1)
	{
		AccessorySocketDegrade(item);
	}
	else
	{
		SetItemAccessorySocketDownGradeTime(item, new_time);
	}
}

ecs::ItemEvents& GetItemEvents(entt::entity item)
{
    static ecs::ItemEvents detached;
    if (item == entt::null || !g_registry.valid(item)) {
        detached = ecs::ItemEvents {};
        return detached;
    }

    return g_registry.get_or_emplace<ecs::ItemEvents>(item);
}

void PrepareItemDestruction(entt::entity item)
{
    if (item == entt::null || !g_registry.valid(item))
        return;

    if (auto* events = g_registry.try_get<ecs::ItemEvents>(item)) {
        event_cancel(&events->destroy);
        event_cancel(&events->expire);
        event_cancel(&events->ownership);
        event_cancel(&events->uniqueExpire);
#ifdef ENABLE_SOUL_SYSTEM
        event_cancel(&events->soulItem);
#endif
        event_cancel(&events->timerBasedOnWearExpire);
        event_cancel(&events->realTimeExpire);
        event_cancel(&events->accessorySocketExpire);
    }

    g_dispatcher.trigger(ecs::EvItemDestroyed { item, GetItemID(item) });
}

bool SaveItemEcs(entt::entity item, bool flush)
{
    if (!IsValidItem(item))
        return false;

    SaveItem(item);
    if (flush)
        ITEM_MANAGER::instance().FlushDelayedSave(item);
    return true;
}

entt::entity GetItemOwner(entt::entity item)
{
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
    if (!owner)
        return entt::null;

    // Was FindByPlayerID(ownerPID), which answered null for every owner with no
    // PID yet - and for any character that never has one.
    return (owner->owner != entt::null && g_registry.valid(owner->owner))
        ? owner->owner
        : entt::null;
}

entt::entity GetItemOwnerEntity(entt::entity item)
{
    return GetItemOwner(item);
}

entt::entity RollPartyDropOwnership(entt::entity item, entt::entity initialOwner)
{
    if (!IsValidItem(item) || initialOwner == entt::null || !g_registry.valid(initialOwner))
        return entt::null;

    LPPARTY party = ecs::SocialSystem::GetParty(initialOwner);
    if (!party || party->GetNearMemberCount() <= 1)
    {
        SetGroundOwnership(item, initialOwner);
        return initialOwner;
    }

#ifdef TEXTS_IMPROVEMENT
    party->ChatPacketToAllMemberNew(CHAT_TYPE_DICE_INFO, 542, "%s", GetItemName(item));
#endif

    entt::entity selected = initialOwner;
    int lastNumber = 0;
    auto roll = [&](LPCHARACTER member)
    {
        const entt::entity memberEntity = member
            ? member->GetEntityHandle()
            : entt::null;
        if (memberEntity == entt::null || !g_registry.valid(memberEntity))
            return;

        int pickedNumber = 0;
        do
        {
            pickedNumber = number(10000, 99999);
        } while (pickedNumber == lastNumber);

        if (pickedNumber > lastNumber)
        {
            lastNumber = pickedNumber;
            selected = memberEntity;
        }
#ifdef TEXTS_IMPROVEMENT
        party->ChatPacketToAllMemberNew(CHAT_TYPE_DICE_INFO, 543, "%s#%d",
            ecs::PlayerRuntime::GetName(memberEntity).data(), pickedNumber);
#endif
    };
    party->ForEachNearMember(roll);

    SetGroundOwnership(item, selected);
#ifdef TEXTS_IMPROVEMENT
    party->ChatPacketToAllMemberNew(CHAT_TYPE_DICE_INFO, 903, "%s#%s",
        ecs::PlayerRuntime::GetName(selected).data(), GetItemName(item));
#endif
    return selected;
}

void SetItemOwnerEntity(entt::entity item, entt::entity owner)
{
    if (item == entt::null || !g_registry.valid(item))
        return;

    auto& itemOwner = g_registry.get_or_emplace<ecs::ItemOwner>(item);
    itemOwner.owner = owner;
    itemOwner.ownerPID = ecs::PlayerRuntime::GetPlayerID(owner);
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

    return {};
}

void SetItemSockets(entt::entity item, const int32_t* sockets)
{
    if (!IsValidItem(item) || !sockets)
        return;

    auto& component = g_registry.get_or_emplace<ecs::ItemSockets>(item);
    std::copy_n(sockets, ITEM_SOCKET_MAX_NUM, component.sockets.begin());
    SaveItem(item);
}

void SetItemAttributes(entt::entity item, const TPlayerItemAttribute* attributes)
{
    if (!IsValidItem(item) || !attributes)
        return;

    auto& component = g_registry.get_or_emplace<ecs::ItemAttributes>(item);
    std::copy_n(attributes, ITEM_ATTRIBUTE_MAX_NUM, component.attrs.begin());
    SaveItem(item);
}

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
void SetItemTransmutation(entt::entity item, uint32_t vnum)
{
    if (!IsValidItem(item))
        return;

    g_registry.get_or_emplace<ecs::ItemIdentity>(item).transmutationVnum = vnum;
    ecs::ItemNetworkSystem::SendItemUpdate(g_registry, item);
    SaveItem(item);
}
#endif

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

bool SetItemSocket(entt::entity item, int index, uint32_t value, bool log)
{
    if (item == entt::null || !g_registry.valid(item) ||
        index < 0 || index >= ITEM_SOCKET_MAX_NUM)
        return false;

    auto& sockets = g_registry.get_or_emplace<ecs::ItemSockets>(item);
    sockets.sockets[index] = static_cast<int32_t>(value);

    ecs::ItemNetworkSystem::SendItemUpdate(g_registry, item);
    SaveItem(item);

    if (log) {
#ifdef ENABLE_NEWSTUFF
        if (g_iDbLogLevel >= LOG_LEVEL_MAX)
#endif
            LogManager::instance().ItemLog(
                index, static_cast<int32_t>(value), 0, GetItemID(item),
                "SET_SOCKET", "", "", GetItemOriginalVnum(item));
    }

    return true;
}

bool SetItemSocketEcs(entt::entity item, int index, uint32_t value)
{
    return SetItemSocket(item, index, value);
}

bool SetItemAttribute(entt::entity item, int index, int type, int value)
{
    if (item == entt::null || !g_registry.valid(item) ||
        index < 0 || index >= ITEM_ATTRIBUTE_MAX_NUM)
        return false;

    auto& attrs = g_registry.get_or_emplace<ecs::ItemAttributes>(item);
    attrs.attrs[index].bType = static_cast<uint8_t>(type);
    attrs.attrs[index].sValue = static_cast<short>(value);

    SaveItem(item);

    return true;
}

bool ClearItemAttribute(entt::entity item, int index)
{
    return SetItemAttribute(item, index, APPLY_NONE, 0);
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

    SaveItem(item);

    return true;
}

bool SetItemExchanging(entt::entity item, bool flag)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& flags = g_registry.get_or_emplace<ecs::ItemFlags>(item);
    flags.exchanging = flag;

    return true;
}

bool CopyItemAttributesEcs(entt::entity source, entt::entity target)
{
    if (!IsValidItem(source) || !IsValidItem(target))
        return false;

    const auto* sourceAttributes = g_registry.try_get<ecs::ItemAttributes>(source);
    if (!sourceAttributes)
        return false;

    g_registry.emplace_or_replace<ecs::ItemAttributes>(target, *sourceAttributes);

    // CItem::SetAttributes, which CopyAttributeTo called, ended with Save().
    SaveItem(target);
    return true;
}

bool CopyItemSocketsEcs(entt::entity source, entt::entity target)
{
    if (!IsValidItem(source) || !IsValidItem(target))
        return false;

    const auto* sourceSockets = g_registry.try_get<ecs::ItemSockets>(source);
    if (!sourceSockets)
        return false;

    g_registry.emplace_or_replace<ecs::ItemSockets>(target, *sourceSockets);
    // SetSocket, which the per-index copy used to call, ended with an item
    // update packet and a save. The component write does neither.
    ecs::ItemNetworkSystem::SendItemUpdate(g_registry, target);
    SaveItem(target);
    return true;
}

static bool CanPutIntoRing(entt::entity ring, entt::entity item)
{
	//const uint32_t vnum = item->GetVnum();
	return false;
}


bool IsSameSpecialGroup(entt::entity item, entt::entity other)
{
    // Answers false for an absent item rather than crashing: the three call
    // sites are wear slots, which are routinely empty. The method version was
    // guarded by a GetWear null test at each one.
    if (!IsValidItem(item) || !IsValidItem(other))
        return false;

    if (GetItemVnum(item) == GetItemVnum(other))
        return true;

    const int group = GetItemSpecialGroup(item);
    return group != 0 && GetItemSpecialGroup(other) == group;
}

bool DistanceValid(entt::entity itemEntity, entt::entity character)
{
	if (!ecs::PlayerRuntime::GetSectree(itemEntity))
		return false;

	int iDist = DISTANCE_APPROX(
		ecs::PlayerRuntime::GetX(itemEntity) - ecs::PlayerRuntime::GetX(character),
		ecs::PlayerRuntime::GetY(itemEntity) - ecs::PlayerRuntime::GetY(character));
	if (iDist > 2400)
		return false;

	return true;
}

bool CanUsedBy(entt::entity itemEntity, entt::entity character)
{
	// Anti flag check
	switch (ecs::PlayerRuntime::GetJob(character))
	{
	case JOB_WARRIOR:
		if (ItemSystem::GetItemAntiFlag(itemEntity) & ITEM_ANTIFLAG_WARRIOR)
			return false;
		break;

	case JOB_ASSASSIN:
		if (ItemSystem::GetItemAntiFlag(itemEntity) & ITEM_ANTIFLAG_ASSASSIN)
			return false;
		break;

	case JOB_SHAMAN:
		if (ItemSystem::GetItemAntiFlag(itemEntity) & ITEM_ANTIFLAG_SHAMAN)
			return false;
		break;

	case JOB_SURA:
		if (ItemSystem::GetItemAntiFlag(itemEntity) & ITEM_ANTIFLAG_SURA)
			return false;
		break;
#ifdef ENABLE_WOLFMAN_CHARACTER
	case JOB_WOLFMAN:
		if (ItemSystem::GetItemAntiFlag(itemEntity) & ITEM_ANTIFLAG_WOLFMAN)
			return false;
		break;
#endif
	}

	return true;
}

bool IsOwnership(entt::entity itemEntity, entt::entity character)
{
	if (!ItemSystem::GetItemEvents(itemEntity).ownership)
		return true;

	return ItemSystem::GetItemOwnershipPID(itemEntity) == ecs::PlayerRuntime::GetPlayerID(character);
}

bool CanPutInto(entt::entity item, entt::entity container)
{
	//if (GetItemType(container) == ITEM_BELT) {
	//	if (GetItemSubType(item) == USE_PUT_INTO_BELT_SOCKET && GetItemValue(item, 0) != 1) {
	//		return true;
	//	}
	//	else {
	//		return false;
	//	}
	//}
	/*else*/ if (GetItemType(container) == ITEM_RING)
		return CanPutIntoRing(container, item);

	else if (GetItemType(container) != ITEM_ARMOR)
		return false;

	uint32_t vnum = GetItemVnum(container);

	if (GetItemVnum(item) == 50634) {
		return (vnum >= 14220 && vnum <= 14233) || (vnum >= 16220 && vnum <= 16233) || (vnum >= 17220 && vnum <= 17233) ? true : false;
	}

	if (GetItemVnum(item) == 50640) {
		return (vnum >= 14580 && vnum <= 14589) || (vnum >= 15010 && vnum <= 15013) || (vnum >= 16580 && vnum <= 16593) || (vnum >= 17570 && vnum <= 17583) ? true : false;
	}

	if (GetItemVnum(item) == 50641) //limites koho aqua
	{
		return (vnum >= 8210 && vnum <= 8223) || (vnum >= 8250 && vnum <= 8263) || (vnum >= 8270 && vnum <= 8283) ? true : false;
	}

	if (GetItemVnum(item) == 50645) //limites koho aqua
	{
		return (vnum >= 8780 && vnum <= 8789) || (vnum >= 8760 && vnum <= 8769) || (vnum >= 8790 && vnum <= 8799) ? true : false;//Fagyos aqua itemek
	}


	if (GetItemVnum(item) == 50646) //limites koho isteni
	{
		return (vnum >= 8730 && vnum <= 8739) || (vnum >= 8700 && vnum <= 8709) || (vnum >= 8780 && vnum <= 8789) ? true : false;//véres zodiák itemek
	}




	if (GetItemVnum(item) == 50643) //limites koho isteni
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

	uint32_t item_type = (GetItemVnum(container) / 10) * 10;
	for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++)
	{
		const JewelAccessoryInfo& info = infos[i];
		switch (GetItemSubType(container))
		{
		case ARMOR_WRIST:
			if (info.wrist == item_type)
			{
				if (info.jewel == GetItemVnum(item))
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
				if (info.jewel == GetItemVnum(item))
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
				if (info.jewel == GetItemVnum(item))
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
	if (GetItemSubType(container) == ARMOR_WRIST)
		vnum -= 14000;
	else if (GetItemSubType(container) == ARMOR_NECK)
		vnum -= 16000;
	else if (GetItemSubType(container) == ARMOR_EAR)
		vnum -= 17000;
	else
		return false;

	uint32_t type = vnum / 20;

	if (type < 0 || type > 11)
	{
		type = (vnum - 170) / 20;

		if (50623 + type != GetItemVnum(item))
			return false;
		else
			return true;
	}
	else if (GetItemVnum(container) >= 16210 && GetItemVnum(container) <= 16219)
	{
		if (50625 != GetItemVnum(item))
			return false;
		else
			return true;
	}
	else if (GetItemVnum(container) >= 16230 && GetItemVnum(container) <= 16239)
	{
		if (50626 != GetItemVnum(item))
			return false;
		else
			return true;
	}

	return 50623 + type == GetItemVnum(item);
}

bool CanPutInto2(entt::entity item, entt::entity container)
{
/*	if (GetItemType(container) == ITEM_BELT) {
		if (GetItemSubType(item) == USE_PUT_INTO_BELT_SOCKET && GetItemValue(item, 0) == 1) {
			return true;
		}
		else {
			return false;
		}
	}

	else*/ if (GetItemType(container) == ITEM_RING)
		return CanPutIntoRing(container, item);

	else if (GetItemType(container) != ITEM_ARMOR)
		return false;

	uint32_t vnum = GetItemVnum(container);

	if (GetItemVnum(item) == 50684) {
		return (vnum >= 14220 && vnum <= 14233) || (vnum >= 16220 && vnum <= 16233) || (vnum >= 17220 && vnum <= 17233) ? true : false;
	}

	if (GetItemVnum(item) == 50690) {
		return (vnum >= 14580 && vnum <= 14589) || (vnum >= 15010 && vnum <= 15013) || (vnum >= 16580 && vnum <= 16593) || (vnum >= 17570 && vnum <= 17583) ? true : false;
	}

	if (GetItemVnum(item) == 50642) //perma koho aqua
	{
		return (vnum >= 8210 && vnum <= 8223) || (vnum >= 8250 && vnum <= 8263) || (vnum >= 8270 && vnum <= 8283) ? true : false;
	}


	if (GetItemVnum(item) == 50644) //perma koho isteni
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

	uint32_t item_type = (GetItemVnum(container) / 10) * 10;
	for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++)
	{
		const JewelAccessoryInfo& info = infos[i];
		switch (GetItemSubType(container))
		{
		case ARMOR_WRIST:
			if (info.wrist == item_type)
			{
				if (info.jewel == GetItemVnum(item))
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
				if (info.jewel == GetItemVnum(item))
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
				if (info.jewel == GetItemVnum(item))
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
	if (GetItemSubType(container) == ARMOR_WRIST)
		vnum -= 14000;
	else if (GetItemSubType(container) == ARMOR_NECK)
		vnum -= 16000;
	else if (GetItemSubType(container) == ARMOR_EAR)
		vnum -= 17000;
	else
		return false;

	uint32_t type = vnum / 20;

	if (type < 0 || type > 11)
	{
		type = (vnum - 170) / 20;

		if (50673 + type != GetItemVnum(item))
			return false;
		else
			return true;
	}
	else if (GetItemVnum(container) >= 16210 && GetItemVnum(container) <= 16219)
	{
		if (50675 != GetItemVnum(item))
			return false;
		else
			return true;
	}
	else if (GetItemVnum(container) >= 16230 && GetItemVnum(container) <= 16239)
	{
		if (50676 != GetItemVnum(item))
			return false;
		else
			return true;
	}

	return 50673 + type == GetItemVnum(item);
}

bool CopyAllAttrToEcs(entt::entity source, entt::entity target)
{
    if (!IsValidItem(source) || !IsValidItem(target))
        return false;

    // Writes go through SetItemSocket rather than straight into the component,
    // because the legacy CopyAllAttrTo called CItem::SetSocket - which also
    // does UpdatePacket and Save. Touching the component alone would drop both.
    if (IsAccessoryForSocket(source))
    {
        for (int index = 0; index < ITEM_SOCKET_MAX_NUM; ++index)
            SetItemSocket(target, index, GetItemSocket(source, index));
    }
    else
    {
        for (int index = 0; index < ITEM_SOCKET_MAX_NUM; ++index)
        {
            if (GetItemSocket(source, index) == 0)
                break;
            SetItemSocket(target, index, 1);
        }

        constexpr int32_t brokenMetinVnum = 28960;
        int targetSlot = 0;
        for (int index = 0;
             index < ITEM_SOCKET_MAX_NUM && targetSlot < ITEM_SOCKET_MAX_NUM;
             ++index)
        {
            const int32_t socket = GetItemSocket(source, index);
            if (socket > 2 && socket != brokenMetinVnum)
                SetItemSocket(target, targetSlot++, socket);
        }
    }

    return CopyItemAttributesEcs(source, target);
}

int GetItemAttributeCount(entt::entity item)
{
    const auto* attributes = g_registry.try_get<ecs::ItemAttributes>(item);
    if (!attributes)
        return 0;

    int count = 0;
    while (count < ITEM_ATTRIBUTE_NORM_NUM && attributes->attrs[count].bType != 0)
        ++count;
    return count;
}

int GetItemRareAttributeCount(entt::entity item)
{
    const auto* attributes = g_registry.try_get<ecs::ItemAttributes>(item);
    if (!attributes)
        return 0;

    int count = 0;
    for (int index = ITEM_ATTRIBUTE_RARE_START; index < ITEM_ATTRIBUTE_RARE_END; ++index) {
        if (attributes->attrs[index].bType != 0)
            ++count;
    }
    return count;
}

bool IsItemExchanging(entt::entity item)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    const auto* flags = g_registry.try_get<ecs::ItemFlags>(item);
    return flags && flags->exchanging;
}

bool IsItemLocked(entt::entity item)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;
    if (IsItemConsumptionPending(item))
        return true;

    const auto* flags = g_registry.try_get<ecs::ItemFlags>(item);
    return flags && flags->isLocked;
}

bool IsItemBound(entt::entity item)
{
#ifdef __SOULBINDING_SYSTEM__
    if (LPITEM legacyItem = LegacyItemBoundary(item))
        return legacyItem->IsBind() || legacyItem->IsUntilBind();
#else
    (void)item;
#endif
    return false;
}

int16_t GetItemLockedAttributeIndex(entt::entity item)
{
    if (item == entt::null || !g_registry.valid(item))
        return -1;

    const auto* locked = g_registry.try_get<ecs::ItemLockedAttribute>(item);
    return locked ? locked->index : -1;
}

bool LockItem(entt::entity item, bool locked)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& flags = g_registry.get_or_emplace<ecs::ItemFlags>(item);
    flags.isLocked = locked;

    return true;
}

bool UnlockItem(entt::entity item)
{
    return LockItem(item, false);
}

void ClearMountAttributeAndAffect(entt::entity item)
{
	const entt::entity chEntity = GetItemOwner(item);


	AffectSystem::RemoveAffect(chEntity, AFFECT_MOUNT);
	AffectSystem::RemoveAffect(chEntity, AFFECT_MOUNT_BONUS);

	::MountSystem::ForceClearRidingState(chEntity);

	ecs::PointSystem::Change(chEntity, POINT_ST, 0);
	ecs::PointSystem::Change(chEntity, POINT_DX, 0);
	ecs::PointSystem::Change(chEntity, POINT_HT, 0);
	ecs::PointSystem::Change(chEntity, POINT_IQ, 0);
}

void SaveItem(entt::entity item)
{
    if (GetItemSkipSave(item))
        return;

    ITEM_MANAGER::instance().DelayedSave(item);
}

bool GetItemSkipSave(entt::entity item)
{
    const auto* flags = g_registry.try_get<ecs::ItemFlags>(item);
    return flags && flags->skipSave;
}

void SetItemLastOwnerPID(entt::entity item, uint32_t pid)
{
    if (item == entt::null || !g_registry.valid(item))
        return;

    g_registry.get_or_emplace<ecs::ItemOwner>(item).lastOwnerPID = pid;
}

uint32_t GetItemOwnershipPID(entt::entity item)
{
    const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
    return owner ? owner->ownershipPID : 0;
}

void SetItemOwnershipPID(entt::entity item, uint32_t pid)
{
    if (item == entt::null || !g_registry.valid(item))
        return;

    g_registry.get_or_emplace<ecs::ItemOwner>(item).ownershipPID = pid;
}

bool SetItemSkipSave(entt::entity item, bool flag)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& flags = g_registry.get_or_emplace<ecs::ItemFlags>(item);
    flags.skipSave = flag;
    return true;
}

bool SetItemWindow(entt::entity item, uint8_t window)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& location = g_registry.get_or_emplace<ecs::ItemLocation>(item);
    location.window = window;

    return true;
}

bool SetItemCell(entt::entity item, entt::entity owner, uint16_t cell)
{
    if (item == entt::null || !g_registry.valid(item))
        return false;

    auto& location = g_registry.get_or_emplace<ecs::ItemLocation>(item);
    location.cell = cell;

    auto& itemOwner = g_registry.get_or_emplace<ecs::ItemOwner>(item);
    itemOwner.owner = owner;
    itemOwner.ownerPID = ecs::PlayerRuntime::GetPlayerID(owner);

    // The mirror that used to follow called CItem::SetCell, which since m_wCell
    // and m_pOwner went away writes these same two fields - and derived its
    // owner from LegacyCharOf(owner), so an owner entity carrying no
    // LegacyCharPtr would have had the assignment above undone one line later.
    return true;
}

uint8_t GetItemWindow(entt::entity item)
{
    if (const auto* location = g_registry.try_get<ecs::ItemLocation>(item))
        return location->window;

    return 0;
}

uint16_t GetItemCell(entt::entity item)
{
    if (const auto* location = g_registry.try_get<ecs::ItemLocation>(item))
        return location->cell;

    return 0;
}

bool IsItemEquipped(entt::entity item)
{
    if (const auto* equipped = g_registry.try_get<ecs::ItemEquipped>(item))
        return equipped->equipped;

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

static bool AddItemToCharacterLegacyBoundary(entt::entity owner,
                                             entt::entity item,
                                             uint8_t window,
                                             uint16_t cell)
{
    return IsValidItem(item) && ecs::PlayerRuntime::IsValid(owner) &&
        InventorySystem::AddToCharacter(item, owner, TItemPos(window, cell));
}

bool PlaceItemEcs(entt::entity owner, entt::entity item, uint8_t window, uint16_t cell)
{
    if (owner == entt::null || !g_registry.valid(owner) ||
        !g_registry.any_of<ecs::PlayerID>(owner) || !IsValidItem(item))
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

    if (!AddItemToCharacterLegacyBoundary(owner, item, window, cell)) {
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
    if (!IsValidItem(item))
        return false;

    // Both are read before the components are cleared: the owner decides
    // whether the character still has to be told, and the PID is the one
    // thing about the old owner that survives the removal.
    const entt::entity previousOwner = GetItemOwnerEntity(item);
    const uint32_t lastOwnerPID = GetItemLastOwnerPID(item);

    g_registry.emplace_or_replace<ecs::ItemLocation>(
        item, ecs::ItemLocation{RESERVED_WINDOW, 0});
    g_registry.emplace_or_replace<ecs::ItemOwner>(
        item, ecs::ItemOwner{entt::null, 0, lastOwnerPID, 0});
    g_registry.remove<ecs::ItemEquipped>(item);

    if (previousOwner != entt::null) {
        InventorySystem::RemoveFromCharacter(item);
        SyncItemStateFromLegacy(item);
    }

    return true;
}

bool RemoveItemFromCharacterLegacyBoundary(entt::entity item)
{
    if (!IsValidItem(item) || GetItemOwnerEntity(item) == entt::null)
        return false;

    InventorySystem::RemoveFromCharacter(item);
    return SyncItemStateFromLegacy(item);
}

static int FindEmptyMainInventoryPosition(entt::entity owner, uint8_t itemSize)
{
    if (owner == entt::null || !g_registry.valid(owner) || itemSize == 0)
        return -1;

    const auto* inventory = TryGetMainInventoryRuntimeComponent(owner);
    if (!inventory)
        return -1;

    const int inventoryLimit = GetMainInventoryLimit(owner);

    constexpr int pageSize = INVENTORY_PAGE_SIZE;
    for (int cell = 0; cell < inventoryLimit; ++cell)
    {
        if (inventory->itemGrid[cell] != 0)
            continue;

        const int page = cell / pageSize;
        bool fits = true;
        for (int row = 1; row < itemSize; ++row)
        {
            const int occupiedCell = cell + (INVENTORY_PAGE_COLUMN * row);
            if (occupiedCell >= inventoryLimit || occupiedCell / pageSize != page ||
                inventory->itemGrid[occupiedCell] != 0)
            {
                fits = false;
                break;
            }
        }
        if (fits)
            return cell;
    }
    return -1;
}

#ifdef ENABLE_EXTRA_INVENTORY
static int FindEmptyExtraInventoryPosition(entt::entity owner, uint8_t itemSize,
                                           uint8_t category)
{
    if (owner == entt::null || !g_registry.valid(owner) || itemSize == 0 || category >= 6)
        return -1;

    const auto* inventory = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(owner);
    if (!inventory)
        return -1;

    const int begin = EXTRA_INVENTORY_CATEGORY_MAX_NUM * category;
    int end = EXTRA_INVENTORY_CATEGORY_MAX_NUM * (category + 1);
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
    static constexpr std::array<std::string_view, 6> unlockFlags {
        "lock_extra.cat1", "lock_extra.cat2", "lock_extra.cat3",
        "lock_extra.cat4", "lock_extra.cat5", "lock_extra.cat6"
    };
    constexpr int freeSlots = (EXTRA_INVENTORY_PAGE_SIZE * 2) + 20;
    constexpr int maxUnlockSlots = 25 + EXTRA_INVENTORY_PAGE_SIZE;
    const int unlockedSlots = std::clamp(
        ecs::QuestSystem::GetFlag(owner, unlockFlags[category]) * 5,
        0, maxUnlockSlots);
    end = std::min(begin + freeSlots + unlockedSlots,
                   static_cast<int>(EXTRA_INVENTORY_MAX_NUM));
#endif

    for (int cell = begin; cell < end; ++cell)
    {
        if (inventory->itemGrid[cell] != 0)
            continue;

        const int page = cell / EXTRA_INVENTORY_PAGE_SIZE;
        bool fits = true;
        for (int row = 1; row < itemSize; ++row)
        {
            const int occupiedCell = cell + (EXTRA_INVENTORY_PAGE_COLUMN * row);
            if (occupiedCell >= end ||
                occupiedCell / EXTRA_INVENTORY_PAGE_SIZE != page ||
                inventory->itemGrid[occupiedCell] != 0)
            {
                fits = false;
                break;
            }
        }

        if (fits)
            return cell;
    }

    return -1;
}
#endif

int GetEmptyInventoryPositionEcs(entt::entity owner, entt::entity item)
{
    if (owner == entt::null || !g_registry.valid(owner) || !IsValidItem(item))
        return -1;

    if (IsDragonSoulItem(item))
        return GetEmptyDragonSoulInventory(owner, item);
#ifdef ENABLE_EXTRA_INVENTORY
    if (IsExtraItem(item))
        return FindEmptyExtraInventoryPosition(
            owner, GetItemSize(item), GetItemExtraCategory(item));
#endif
    return FindEmptyMainInventoryPosition(owner, GetItemSize(item));
}

bool HasMainInventorySpaceEcs(entt::entity owner, uint8_t itemSize)
{
    return FindEmptyMainInventoryPosition(owner, itemSize) != -1;
}

bool HasInventorySpaceForItemVnum(entt::entity owner, uint32_t itemVnum)
{
    const TItemTable* proto = ITEM_MANAGER::instance().GetTable(itemVnum);
    if (!proto)
        return false;

    return FindEmptyMainInventoryPosition(owner, proto->bSize) != -1;
}

bool PlaceItemOnGroundLegacyBoundary(entt::entity item, int32_t mapIndex,
                                     const PIXEL_POSITION& position, int destroySeconds)
{
    LPITEM legacyItem = LegacyItemBoundary(item);
    if (!legacyItem || destroySeconds <= 0)
        return false;

    if (!legacyItem->AddToGround(mapIndex, position))
        return false;

    ItemSystem::StartDestroyEvent(item, destroySeconds);
    return SyncItemStateFromLegacy(item);
}

int GetEmptyDragonSoulInventory(entt::entity owner, entt::entity item)
{
    if (owner == entt::null || !g_registry.valid(owner) || !IsDragonSoulItem(item))
        return -1;

    const auto* inventory = g_registry.try_get<ecs::DragonSoulInventoryComponent>(owner);
    if (!inventory)
        return -1;

    const uint8_t itemSize = GetItemSize(item);
    const uint16_t baseCell = DSManager::instance().GetBasePosition(item);
    if (itemSize == 0 || baseCell == WORD_MAX ||
        baseCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
        return -1;

    const int boxEnd = std::min<int>(baseCell + DRAGON_SOUL_BOX_SIZE,
                                     DRAGON_SOUL_INVENTORY_MAX_NUM);
    for (int cell = baseCell; cell < boxEnd; ++cell)
    {
        if (inventory->itemGrid[cell] != 0)
            continue;

        bool fits = true;
        for (int row = 1; row < itemSize; ++row)
        {
            const int occupiedCell = cell + (DRAGON_SOUL_BOX_COLUMN_NUM * row);
            if (occupiedCell >= boxEnd || inventory->itemGrid[occupiedCell] != 0)
            {
                fits = false;
                break;
            }
        }

        if (fits)
            return cell;
    }

    return -1;
}

bool IsItemVnumStackable(uint32_t vnum)
{
    const TItemTable* proto = ITEM_MANAGER::instance().GetTable(vnum);
    return proto && IS_SET(proto->dwFlags, ITEM_FLAG_STACKABLE) &&
        !IS_SET(proto->dwAntiFlags, ITEM_ANTIFLAG_STACK);
}

bool ModifyItemPointsEcs(entt::entity item, bool add)
{
    if (!IsValidItem(item))
        return false;

    ItemSystem::ModifyPoints(item, add);
    return true;
}

bool StartTimerBasedOnWearExpireEventEcs(entt::entity item)
{
    if (!IsValidItem(item))
        return false;

    ItemSystem::StartTimerBasedOnWearExpireEvent(item);
    return true;
}

bool StopTimerBasedOnWearExpireEventEcs(entt::entity item)
{
    if (!IsValidItem(item))
        return false;

    ItemSystem::StopTimerBasedOnWearExpireEvent(item);
    return true;
}

bool StartRealTimeExpireEventEcs(entt::entity item)
{
    if (!IsValidItem(item))
        return false;

    auto& events = GetItemEvents(item);
    if (events.realTimeExpire)
        return true;

    const TItemTable* proto = GetItemProto(item);
    if (!proto)
        return false;

    for (const auto& limit : proto->aLimits)
    {
        if (limit.bType != LIMIT_REAL_TIME && limit.bType != LIMIT_REAL_TIME_START_FIRST_USE)
            continue;

#ifdef ENABLE_NEW_USE_POTION
        const bool isNewPotion = GetItemType(item) == ITEM_USE && GetItemSubType(item) == USE_NEW_POTIION;
        const int32_t remainSec = isNewPotion ? static_cast<int32_t>(GetItemSocket(item, 0)) : 0;
        if (isNewPotion && remainSec <= 0)
        {
            if (GetItemSocket(item, 1) == 1)
            {
                const entt::entity owner = GetItemOwnerEntity(item);
                if (owner != entt::null)
                {
                    if (AffectSystem::FindAffect(owner, GetItemValue(item, 0)))
                        AffectSystem::RemoveAffect(owner, GetItemValue(item, 0));
#ifdef TEXTS_IMPROVEMENT
                    ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 27, "%s", GetItemName(item));
#endif
                }
            }

            ITEM_MANAGER::instance().RemoveItem(item, "REAL_TIME_EXPIRE");
            return true;
        }
#endif

        item_vid_event_info* info = AllocEventInfo<item_vid_event_info>();
        info->item = item;
#ifdef ENABLE_NEW_USE_POTION
        if (isNewPotion)
        {
            info->newpotion = true;
            events.realTimeExpire = event_create(
                real_time_expire_event, info, PASSES_PER_SEC(remainSec > 60 ? 60 : remainSec));
        }
        else
        {
            info->newpotion = false;
            events.realTimeExpire = event_create(real_time_expire_event, info, PASSES_PER_SEC(1));
        }
#else
        events.realTimeExpire = event_create(real_time_expire_event, info, PASSES_PER_SEC(1));
#endif

        g_dispatcher.trigger(ecs::EvItemExpired { item, GetItemID(item) });
        LOG_INFO("REAL_TIME_EXPIRE: StartRealTimeExpireEvent");
        return true;
    }

    return false;
}

#ifdef ENABLE_SOUL_SYSTEM
bool StartSoulItemEventEcs(entt::entity item)
{
    if (!IsValidItem(item) || GetItemType(item) != ITEM_SOUL)
        return false;

    auto& events = GetItemEvents(item);
    if (events.soulItem)
        return true;

    const TItemTable* proto = GetItemProto(item);
    if (!proto)
        return false;

    const int minutes = static_cast<int>(GetItemSocket(item, 2) / 10000);
    if (minutes >= proto->aLimits[1].lValue)
        return false;

    item_vid_event_info* info = AllocEventInfo<item_vid_event_info>();
    info->item = item;
    events.soulItem = event_create(
        soul_item_event, info, PASSES_PER_SEC(test_server ? 5 : 60));
    g_dispatcher.trigger(ecs::EvItemExpired { item, GetItemID(item) });
    return true;
}
#endif

int FindEquipCell(entt::entity ownerEntity, entt::entity item, int iCandidateCell)
{
	const auto hasWearItem = [ownerEntity](uint8_t wearCell) {
		return ItemSystem::IsValidItem(ItemSystem::GetWearItem(ownerEntity, wearCell));
	};

	if ((0 == GetItemWearFlag(item) || ITEM_TOTEM == GetItemType(item)) && ITEM_COSTUME != GetItemType(item) && ITEM_DS != GetItemType(item) && ITEM_SPECIAL_DS != GetItemType(item) && ITEM_RING != GetItemType(item) && ITEM_BELT != GetItemType(item))
		return -1;

	if (GetItemType(item) == ITEM_DS || GetItemType(item) == ITEM_SPECIAL_DS)
	{
		if (iCandidateCell < 0)
		{
			return WEAR_MAX_NUM + GetItemSubType(item);
		}
		else
		{
			for (int i = 0; i < DRAGON_SOUL_DECK_MAX_NUM; i++)
			{
				if (WEAR_MAX_NUM + i * DS_SLOT_MAX + GetItemSubType(item) == iCandidateCell)
				{
					return iCandidateCell;
				}
			}
			return -1;
		}
	}
	else if (GetItemType(item) == ITEM_COSTUME)
	{
		if (GetItemSubType(item) == COSTUME_BODY)
			return WEAR_COSTUME_BODY;
		else if (GetItemSubType(item) == COSTUME_HAIR)
			return WEAR_COSTUME_HAIR;
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		else if (GetItemSubType(item) == COSTUME_MOUNT)
			return WEAR_COSTUME_MOUNT;
#endif
#ifdef ENABLE_ACCE_SYSTEM
		else if (GetItemSubType(item) == COSTUME_ACCE)
			return WEAR_COSTUME_ACCE_SLOT;
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		else if (GetItemSubType(item) == COSTUME_WEAPON)
			return WEAR_COSTUME_WEAPON;
#endif
#ifdef ENABLE_STOLE_COSTUME
		else if (GetItemSubType(item) == COSTUME_STOLE)
			return WEAR_COSTUME_ACCE;
#endif
#ifdef ENABLE_COSTUME_PET
		else if (GetItemSubType(item) == COSTUME_PET_SKIN)
			return WEAR_COSTUME_PET_SKIN;
#endif
#ifdef ENABLE_COSTUME_MOUNT
		else if (GetItemSubType(item) == COSTUME_MOUNT_SKIN)
			return WEAR_COSTUME_MOUNT_SKIN;
#endif
#ifdef ENABLE_COSTUME_EFFECT
		else if (GetItemSubType(item) == COSTUME_EFFECT_BODY)
			return WEAR_COSTUME_EFFECT_BODY;
		else if (GetItemSubType(item) == COSTUME_EFFECT_WEAPON)
			return WEAR_COSTUME_EFFECT_WEAPON;
#endif
#ifdef ENABLE_RUNE_SYSTEM
		else if (GetItemSubType(item) == RUNE_SLOT1)
			return WEAR_RUNE1;
		else if (GetItemSubType(item) == RUNE_SLOT2)
			return WEAR_RUNE2;
		else if (GetItemSubType(item) == RUNE_SLOT3)
			return WEAR_RUNE3;
		else if (GetItemSubType(item) == RUNE_SLOT4)
			return WEAR_RUNE4;
		else if (GetItemSubType(item) == RUNE_SLOT5)
			return WEAR_RUNE5;
		else if (GetItemSubType(item) == RUNE_SLOT6)
			return WEAR_RUNE6;
		else if (GetItemSubType(item) == RUNE_SLOT7)
			return WEAR_RUNE7;
#endif
	}
#if !defined(ENABLE_MOUNT_COSTUME_SYSTEM) && !defined(ENABLE_ACCE_SYSTEM)
	else if (GetItemType(item) == ITEM_RING)
	{
		if (hasWearItem(WEAR_RING1))
			return WEAR_RING2;
		else
			return WEAR_RING1;
	}
#endif
	else if (GetItemType(item) == ITEM_BELT)
		return WEAR_BELT;
	else if (GetItemWearFlag(item) & WEARABLE_BODY)
		return WEAR_BODY;
	else if (GetItemWearFlag(item) & WEARABLE_HEAD)
		return WEAR_HEAD;
	else if (GetItemWearFlag(item) & WEARABLE_FOOTS)
		return WEAR_FOOTS;
	else if (GetItemWearFlag(item) & WEARABLE_WRIST)
		return WEAR_WRIST;
	else if (GetItemWearFlag(item) & WEARABLE_WEAPON)
		return WEAR_WEAPON;
	else if (GetItemWearFlag(item) & WEARABLE_SHIELD)
		return WEAR_SHIELD;
	else if (GetItemWearFlag(item) & WEARABLE_NECK)
		return WEAR_NECK;
	else if (GetItemWearFlag(item) & WEARABLE_EAR)
		return WEAR_EAR;
	else if (GetItemWearFlag(item) & WEARABLE_ARROW)
		return WEAR_ARROW;
	else if (GetItemWearFlag(item) & WEARABLE_UNIQUE)
	{
#ifdef ENABLE_NEW_UNIQUE_WEAR_LIMITED
		if (GetItemSubType(item) == UNIQUE_PVM || GetItemSubType(item) == UNIQUE_PVP || GetItemSubType(item) == UNIQUE_NONE)
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
	else if (GetItemSubType(item) == ARMOR_PENDANT || GetItemWearFlag(item) & WEARABLE_PENDANT)
		return WEAR_PENDANT;
#endif

	else if (GetItemWearFlag(item) & WEARABLE_ABILITY)
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

static bool EquipItemLegacyBoundary(entt::entity owner, entt::entity item,
                                    int candidateCell)
{
    LPCHARACTER legacyOwner = LegacyCharOf(owner);
    LPITEM legacyItem = LegacyItemBoundary(item);
    return legacyOwner && IsValidItem(item) &&
        legacyOwner->EquipItem(legacyItem, candidateCell);
}

static bool UnequipItemLegacyBoundary(entt::entity owner, entt::entity item)
{
    LPCHARACTER legacyOwner = LegacyCharOf(owner);
    LPITEM legacyItem = LegacyItemBoundary(item);
    return legacyOwner && IsValidItem(item) && legacyOwner->UnequipItem(legacyItem);
}

bool EquipItemEcs(entt::entity owner, entt::entity item, int candidateCell)
{
    if (owner == entt::null || !g_registry.valid(owner) || !IsValidItem(item))
        return false;

    const int wearCell = FindEquipCell(owner, item, candidateCell);
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

    const bool result = EquipItemLegacyBoundary(owner, item, candidateCell);
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
    if (owner == entt::null || !g_registry.valid(owner) || !IsValidItem(item))
        return false;

    const int targetCell = GetEmptyInventoryPositionEcs(owner, item);
    if (targetCell < 0)
        return false;

    const uint8_t targetWindow = IsDragonSoulItem(item)
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

    const bool result = UnequipItemLegacyBoundary(owner, item);
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

// ItemEquipped::slot is derived from the cell, and several writers set the
// two independently. This recomputes the slot from the location component.
bool RefreshItemEquippedSlot(entt::entity item)
{
    if (!IsValidItem(item))
        return false;

    const uint16_t cell = GetItemCell(item);
    const bool equipped = IsItemEquipped(item);
    uint8_t slot = 0;
    if (equipped && cell >= INVENTORY_MAX_NUM)
        slot = static_cast<uint8_t>(cell - INVENTORY_MAX_NUM);

    g_registry.emplace_or_replace<ecs::ItemEquipped>(
        item, ecs::ItemEquipped{equipped, slot});
    return true;
}

// ownerPID is the persisted form of the owner entity, so it goes stale when
// the owner gains a PID after pickup. This recomputes it from the entity.
bool RefreshItemOwnerPID(entt::entity item)
{
    if (!IsValidItem(item))
        return false;

    auto& owner = g_registry.get_or_emplace<ecs::ItemOwner>(item);
    owner.ownerPID = ecs::PlayerRuntime::GetPlayerID(owner.owner);
    owner.ownershipPID = owner.ownerPID;
    return true;
}

bool SyncItemStateFromLegacy(entt::entity item)
{
    LPITEM legacyItem = LegacyItemBoundary(item);
    if (!legacyItem || item == entt::null || !g_registry.valid(item))
        return false;

    g_registry.emplace_or_replace<ecs::ItemIdentity>(
        item, ecs::ItemIdentity{
                  legacyItem->GetID(),
                  legacyItem->GetVnum(),
                  legacyItem->GetOriginalVnum(),
                  legacyItem->GetVID(),
                  legacyItem->GetMaskVnum(),
#ifdef __CHANGELOOK_SYSTEM__
                  legacyItem->GetSIGVnum(),
                  legacyItem->GetSpecialGroup(),
                  legacyItem->GetTransmutation(),
#else
                  legacyItem->GetSIGVnum(),
                  legacyItem->GetSpecialGroup(),
                  0,
#endif
              });
    g_registry.emplace_or_replace<ecs::ItemCount>(
        item, ecs::ItemCount{legacyItem->GetCount()});
    g_registry.emplace_or_replace<ecs::ItemPrototypeMeta>(
        item, ecs::ItemPrototypeMeta{legacyItem->GetType(), legacyItem->GetSubType()});
    // Only flags still comes from the legacy object; exchanging, skipSave and
    // isLocked live in this component and are written directly.
    auto& flags = g_registry.get_or_emplace<ecs::ItemFlags>(item);
    flags.flags = legacyItem->GetFlag();

    RefreshItemEquippedSlot(item);
    RefreshItemOwnerPID(item);
    return true;
}

bool DestroyLoadedDuplicateItem(entt::entity item)
{
    LPITEM legacyItem = LegacyItemBoundary(item);
    if (!legacyItem) {
        LOG_ERROR("DUP_ITEM_DESTROY_RESOLVE_FAIL entity={}", static_cast<uint32_t>(item));
        return false;
    }

    const uint32_t itemID = GetItemID(item);
    const uint32_t itemVID = GetItemVID(item);
    const uint32_t itemVnum = GetItemVnum(item);
    const uint32_t lastOwnerPID = GetItemLastOwnerPID(item);
    const uint8_t itemWindow = GetItemWindow(item);
    const uint16_t itemCell = GetItemCell(item);
    LOG_ERROR("DUP_ITEM_DESTROY_BEGIN entity={} item={} id={} vid={} vnum={} last_owner_pid={} window={} cell={}",
        static_cast<uint32_t>(item), static_cast<const void*>(legacyItem), itemID, itemVID, itemVnum,
        lastOwnerPID, static_cast<int>(itemWindow), itemCell);

    ItemSystem::SetItemSkipSave(item, true);

    const auto* ownerState = g_registry.try_get<ecs::ItemOwner>(item);
    const uint32_t ownerPID = ownerState ? ownerState->ownerPID : 0;
    const entt::entity legacyOwner = GetItemOwnerEntity(item);
    const entt::entity liveOwner =
        ownerPID != 0 ? ecs::PlayerRuntime::FindByPlayerID(ownerPID) : entt::null;
    LOG_ERROR("DUP_ITEM_DESTROY_OWNER entity={} id={} owner_pid={} legacy_owner={} live_owner={}",
        static_cast<uint32_t>(item), itemID, ownerPID,
        static_cast<uint32_t>(legacyOwner), static_cast<uint32_t>(liveOwner));

    if (legacyOwner != entt::null) {
        LOG_ERROR("DUP_ITEM_DESTROY_REMOVE_FROM_CHARACTER_BEGIN entity={} id={} stale_owner={}",
            static_cast<uint32_t>(item), itemID, liveOwner != legacyOwner);
        InventorySystem::RemoveFromCharacter(item);
        LOG_ERROR("DUP_ITEM_DESTROY_REMOVE_FROM_CHARACTER_END entity={} id={} owner_after={}",
            static_cast<uint32_t>(item), itemID, static_cast<uint32_t>(GetItemOwnerEntity(item)));
    }

    LOG_ERROR("DUP_ITEM_DESTROY_LEGACY_BEGIN item={} id={}", static_cast<const void*>(legacyItem), itemID);
    M2_DESTROY_ITEM(item);
    LOG_ERROR("DUP_ITEM_DESTROY_LEGACY_END item={} id={}", static_cast<const void*>(legacyItem), itemID);
    return true;
}

static uint32_t EntityPlayerID(entt::entity e)
{
    if (const auto* playerID = g_registry.try_get<ecs::PlayerID>(e))
        return playerID->pid;

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

bool SetGroundOwnership(entt::entity item, entt::entity owner,
                                      int seconds)
{
    if (!IsValidItem(item) || !ecs::PlayerRuntime::IsValid(owner))
        return false;

    InventorySystem::SetOwnership(item, owner, seconds);
    return RefreshItemOwnerPID(item);
}

static bool ReceiveItemLegacyBoundary(entt::entity receiver,
                                      entt::entity from,
                                      entt::entity item)
{
    LPCHARACTER legacyReceiver = LegacyCharOf(receiver);
    LPITEM legacyItem = LegacyItemBoundary(item);
    if (!legacyReceiver || !ecs::PlayerRuntime::IsValid(from) || !IsValidItem(item) ||
        !legacyReceiver->CanReceiveItem(from, legacyItem))
        return false;

    legacyReceiver->ReceiveItem(from, legacyItem);
    return true;
}

bool ReceiveItemEcs(entt::entity receiver, entt::entity from, entt::entity item)
{
    if (receiver == entt::null || from == entt::null ||
        !g_registry.valid(receiver) || !g_registry.valid(from) ||
        !IsValidItem(item))
        return false;

    if (!ReceiveItemLegacyBoundary(receiver, from, item))
        return false;

    if (g_registry.valid(item) && IsValidItem(item)) {
        SyncItemStateFromLegacy(item);
        return true;
    }

    return true;
}

static bool GiveSpecialItemGroupLegacyBoundary(
    entt::entity owner, uint32_t groupNum,
    std::vector<uint32_t>& itemVnums, std::vector<uint32_t>& itemCounts,
    std::vector<entt::entity>& itemEntities, int& count)
{
    LPCHARACTER legacyOwner = LegacyCharOf(owner);
    return legacyOwner && legacyOwner->GiveItemFromSpecialItemGroup(
        groupNum, itemVnums, itemCounts, itemEntities, count);
}

SpecialItemGroupResult GiveItemFromSpecialItemGroup(entt::entity e, uint32_t groupNum)
{
    SpecialItemGroupResult result;
    if (e == entt::null || !g_registry.valid(e))
        return result;

    std::vector<entt::entity> itemGets;
    if (!GiveSpecialItemGroupLegacyBoundary(
            e, groupNum, result.itemVnums, result.itemCounts, itemGets,
            result.count))
        return result;

    result.itemEntities = std::move(itemGets);

    return result;
}

void ItemDivision(entt::entity e, TItemPos cell)
{
    (void)e;
    (void)cell;
}

static bool DoRefineLegacyBoundary(entt::entity owner, entt::entity item,
                                   bool moneyOnly)
{
    LPCHARACTER legacyOwner = LegacyCharOf(owner);
    LPITEM legacyItem = LegacyItemBoundary(item);
    return legacyOwner && IsValidItem(item) &&
        legacyOwner->DoRefine(legacyItem, moneyOnly);
}

static bool DoRefineWithScrollLegacyBoundary(entt::entity owner,
                                             entt::entity item)
{
    LPCHARACTER legacyOwner = LegacyCharOf(owner);
    LPITEM legacyItem = LegacyItemBoundary(item);
    return legacyOwner && IsValidItem(item) &&
        legacyOwner->DoRefineWithScroll(legacyItem);
}

static bool DoRefineItemSoulLegacyBoundary(entt::entity owner,
                                           entt::entity item)
{
    LPCHARACTER legacyOwner = LegacyCharOf(owner);
    LPITEM legacyItem = LegacyItemBoundary(item);
    return legacyOwner && IsValidItem(item) &&
        legacyOwner->DoRefineItemSoul(legacyItem);
}

bool DoRefine(entt::entity e, entt::entity item, bool moneyOnly)
{
    const bool result = DoRefineLegacyBoundary(e, item, moneyOnly);
    if (result)
        SyncItemStateFromLegacy(item);
    return result;
}

bool DoRefineWithScroll(entt::entity e, entt::entity item)
{
    const bool result = DoRefineWithScrollLegacyBoundary(e, item);
    if (result)
        SyncItemStateFromLegacy(item);
    return result;
}

bool DoRefineItemSoul(entt::entity e, entt::entity item)
{
    const bool result = DoRefineItemSoulLegacyBoundary(e, item);
    if (result)
        SyncItemStateFromLegacy(item);
    return result;
}

} // namespace ItemSystem
