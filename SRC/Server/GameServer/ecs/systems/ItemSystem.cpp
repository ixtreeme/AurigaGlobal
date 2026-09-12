#include "../../stdafx.h"
#include "PlayerRuntimeSystem.hpp"
#include "SessionSystem.hpp"
#include "QuestSystem.hpp"
#include "SocialSystem.hpp"
#include "InventorySystem.hpp"

#include "ItemSystem.hpp"
#include "MountSystem.hpp"
#include "NetworkSyncSystem.hpp"
#include "PointSystem.hpp"
#include "AffectSystem.hpp"
#include "CombatSystem.hpp"
#include "../EntityFactory.hpp"
#include "../ItemRegistry.hpp"
#include <charconv>
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
    return comp && ItemSystem::IsValidItem(comp->items[cell]) ? comp->items[cell] : entt::null;
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

static bool RetireItemEntity(entt::entity itemEntity, const char* reason)
{
    if (!ItemSystem::IsValidItem(itemEntity))
        return false;

    ITEM_MANAGER::instance().RemoveItem(itemEntity, reason);

    // A reentrant/busy or otherwise rejected manager cleanup is not success.
    // Never bypass it with a second raw factory destruction.
    return !g_registry.valid(itemEntity);
}

#ifndef ENABLE_SWITCHBOT
const int MAX_NORM_ATTR_NUM = ITEM_MANAGER::MAX_NORM_ATTR_NUM;
const int MAX_RARE_ATTR_NUM = ITEM_MANAGER::MAX_RARE_ATTR_NUM;
#endif

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

uint8_t ExtraCategory(uint32_t vnum, uint8_t type, uint8_t subtype)
{
	switch (type)
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

	switch (vnum) {
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



ecs::ItemProtoRef MakeItemPrototype(const TItemTable* proto, uint32_t displayVnum)
{
    ecs::ItemProtoRef result {};
    if (!proto) return result;
    result.base_vnum = proto->dwVnum;
    result.type = proto->bType;
    result.subtype = proto->bSubType;
    result.weapon_min = static_cast<uint32_t>(std::max<int32_t>(0, proto->alValues[3]));
    result.weapon_max = static_cast<uint32_t>(std::max<int32_t>(0, proto->alValues[4]));
    result.defense = static_cast<uint32_t>(std::max<int32_t>(0, proto->alValues[1]));
    // Weapon magic damage uses values 1/2; value 6 is outside the proto array.
    result.magic_min = static_cast<uint32_t>(std::max<int32_t>(0, proto->alValues[1]));
    result.magic_max = static_cast<uint32_t>(std::max<int32_t>(0, proto->alValues[2]));
#ifdef ENABLE_MULTI_NAMES
    std::strncpy(result.name, proto->szLocaleName[0], ITEM_NAME_MAX_LEN);
#else
    std::strncpy(result.name, proto->szLocaleName, ITEM_NAME_MAX_LEN);
#endif
    result.name[ITEM_NAME_MAX_LEN] = '\0';
    result.size = proto->bSize;
#ifdef ENABLE_EXTRA_INVENTORY
    result.extra_category = ExtraCategory(displayVnum, proto->bType, proto->bSubType);
#endif
    for (const auto& limit : proto->aLimits)
        if (limit.bType == LIMIT_LEVEL) {
            result.level_limit = static_cast<uint8_t>(std::clamp(limit.lValue, 0, 255));
            break;
        }
    result.wear_flags = proto->dwWearFlags;
    result.anti_flags = proto->dwAntiFlags;
    result.immune_flags = proto->dwImmuneFlag;
    result.refined_vnum = proto->dwRefinedVnum;
    // Preserve the original base-name +N convention, with a bounded parse.
    const std::string_view name(proto->szName, strnlen(proto->szName, sizeof(proto->szName)));
    const auto plus = name.rfind('+');
    if (plus != std::string_view::npos) {
        int level = 0;
        const auto suffix = name.substr(plus + 1);
        const auto parsed = std::from_chars(suffix.data(), suffix.data() + suffix.size(), level);
        if (parsed.ec == std::errc{})
            result.refine_level = static_cast<uint8_t>(std::clamp(level, 0, 255));
    }
    result.limit_timer_wear_index = proto->cLimitTimerBasedOnWearIndex;
    result.proto = proto;
    return result;
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


entt::entity EntityFactory::CreateItemEntity(entt::registry& reg, const TItemTable* proto,
    uint32_t vnum, uint32_t id, uint32_t vid, uint32_t mask)
{
    // Item indexes and runtime services belong to the world registry.
    if (&reg != &g_registry || !proto || !vnum || !vid || !proto->bSize ||
        (proto->bType != ITEM_ELK && !id)) return entt::null;
    auto& index = CItemRegistry::Instance();
    if (index.Find(id) != entt::null || index.FindByVID(vid) != entt::null) return entt::null;
    const uint32_t display = mask ? mask : vnum;
    const auto prototype = MakeItemPrototype(proto, display);
    const auto flags = static_cast<int32_t>(proto->dwFlags);
    const ecs::ItemIdentity identity {id, display, vnum, vid, mask, 0,
        ITEM_MANAGER::instance().GetSpecialGroupFromItem(display), 0};
    const entt::entity item = reg.create();
    const auto rollback = [&] {
        index.Unregister(item);
        if (reg.valid(item)) DestroyItemEntity(reg, item);
    };
    try {
        // Component construction can invoke callbacks. Publish ItemIdentity last,
        // so gameplay cannot find a half-constructed item. Check the generation
        // after every signal; never write through a returned component reference.
        const auto prepare = [&]<typename T>(T value = {}) {
            if (!reg.valid(item)) return false;
            // A single-element insert publishes construction without emplace's
            // trailing get(), which would access a callback-destroyed component.
            reg.insert<T>(&item, &item + 1, value);
            return reg.valid(item) && reg.all_of<T>(item);
        };
        if (!prepare(ecs::ItemLocation{}) ||
            !prepare(ecs::ItemGroundPosition{}) ||
            !prepare(ecs::ItemCount{}) ||
            !prepare(ecs::ItemPrototypeMeta {prototype.type, prototype.subtype}) ||
            !prepare(ecs::ItemOwner{}) ||
            !prepare(ecs::ItemEquipped{}) ||
            !prepare(ecs::ItemFlags {flags, false, false, false}) ||
            !prepare(ecs::ItemSockets{}) ||
            !prepare(ecs::ItemAttributes{}) ||
            !prepare(ecs::ItemLockedAttribute{}) ||
            !prepare(prototype) ||
            !prepare(ecs::ItemEvents{}) ||
            !prepare(ecs::ViewMap{}) ||
            !prepare(ecs::ViewerMap{}) ||
            !prepare(ecs::ViewAgeMap{}) ||
            !prepare(identity) ||
            !reg.all_of<ecs::ItemLocation, ecs::ItemGroundPosition, ecs::ItemCount,
                ecs::ItemPrototypeMeta, ecs::ItemOwner, ecs::ItemEquipped, ecs::ItemFlags,
                ecs::ItemSockets, ecs::ItemAttributes, ecs::ItemLockedAttribute,
                ecs::ItemProtoRef, ecs::ItemEvents, ecs::ViewMap, ecs::ViewerMap, ecs::ViewAgeMap>(item) ||
            !index.Register(id, vid, item)) {
            rollback();
            return entt::null;
        }
        ecs::ItemInvariants::ValidateItemEntity(reg, item, "item.factory.create");
        return item;
    } catch (...) {
        rollback();
        throw;
    }
}

void EntityFactory::DestroyItemEntity(entt::registry& reg, entt::entity item)
{
    if (&reg != &g_registry || !reg.valid(item)) return;
    struct RetiringItems { std::unordered_set<entt::entity> items; };
    auto& retiring = reg.ctx().contains<RetiringItems>()
        ? reg.ctx().get<RetiringItems>() : reg.ctx().emplace<RetiringItems>();
    if (!retiring.items.insert(item).second) return;
    struct Guard {
        RetiringItems& retiring; entt::entity item;
        ~Guard() { retiring.items.erase(item); }
    } guard {retiring, item};
    const auto detached = [&] {
        if (!reg.valid(item) || reg.any_of<ecs::SpatialEntity, ecs::SectorPlacement>(item)) return false;
        const auto* owner = reg.try_get<ecs::ItemOwner>(item);
        return !owner || (owner->owner == entt::null && owner->ownerPID == 0);
    };
    // The manager owns ground/storage detachment and persistence. Do not delete
    // an entity which an event callback has moved into someone else's storage.
    if (!detached()) return;
    ItemSystem::PrepareItemDestruction(item);
    if (!reg.valid(item)) { CItemRegistry::Instance().Unregister(item); return; }
    if (!detached()) return;
    CItemRegistry::Instance().Unregister(item);
    reg.destroy(item);
}

EVENTFUNC(unique_expire_event);
EVENTFUNC(timer_based_on_wear_expire_event);
EVENTFUNC(real_time_expire_event);
EVENTFUNC(accessory_socket_expire_event);
EVENTFUNC(soul_item_event);

namespace ItemSystem {

bool RefreshItemPrototype(entt::entity item, const TItemTable* proto)
{
    if (!IsValidItem(item) ||
        !g_registry.all_of<ecs::ItemProtoRef, ecs::ItemPrototypeMeta, ecs::ItemFlags>(item)) return false;
    // Existing component writes only: no callbacks can observe mixed old/new
    // prototype fields, and a removed prototype clears every borrowed reference.
    const auto snapshot = MakeItemPrototype(proto, GetItemVnum(item));
    g_registry.get<ecs::ItemProtoRef>(item) = snapshot;
    g_registry.get<ecs::ItemPrototypeMeta>(item) = {snapshot.type, snapshot.subtype};
    g_registry.get<ecs::ItemFlags>(item).flags = proto ? static_cast<int32_t>(proto->dwFlags) : 0;
    return true;
}

bool PickupItem(entt::entity character, uint32_t vid)
{
    if (!ecs::PlayerRuntime::IsPC(character) || CombatSystem::IsDead(character) ||
        ecs::PlayerRuntime::IsObserverMode(character)) return false;
    const entt::entity item = CItemRegistry::Instance().FindByVID(vid);
    const auto ground = [&] {
        if (!IsValidItem(item) || IsItemConsumptionPending(item) || !GetItemCount(item) ||
            IsItemLocked(item) || IsItemExchanging(item) || IsItemEquipped(item)) return false;
        const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
        return owner && owner->owner == entt::null && owner->ownerPID == 0 &&
            GetItemWindow(item) == GROUND && ecs::PlayerRuntime::GetSectree(item) &&
            DistanceValid(item, character);
    };
    if (!ground()) return false;
    // Packet/save/quest callbacks must not initiate another pickup of this item.
    struct ActivePickups { std::unordered_set<entt::entity> items; };
    auto& active = g_registry.ctx().contains<ActivePickups>()
        ? g_registry.ctx().get<ActivePickups>() : g_registry.ctx().emplace<ActivePickups>();
    if (!active.items.insert(item).second) return false;
    struct Guard {
        ActivePickups& active; entt::entity item;
        ~Guard() { active.items.erase(item); }
    } guard {active, item};

    if (GetItemType(item) == ITEM_QUEST) {
        auto* questPC = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(character));
        if (!questPC || questPC->IsRunning()) {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 692, "");
#endif
            return false;
        }
    }
    entt::entity owner = character;
    if (!IsOwnership(item, owner)) {
        if (GetItemAntiFlags(item) & (ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP)) return false;
        auto* party = ecs::SocialSystem::GetParty(character);
        if (!party) return false;
        owner = entt::null;
        const auto findOwner = [&](entt::entity member) { if (IsOwnership(item, member)) owner = member; };
        party->ForEachOnlineMember(findOwner);
        if (!ecs::PlayerRuntime::IsPC(owner)) return false;
    }
    if (!ground()) return false;
    if (GetItemType(item) == ITEM_ELK) {
        const uint32_t count = GetItemCount(item);
        if (!DestroyItemEntityEcs(item, "PICKUP_GOLD")) return false;
        // Snapshot the amount before retirement; never read a deleted stack.
        if (ecs::PlayerRuntime::IsPC(owner)) {
            GiveGold(owner, count);
#ifdef ENABLE_RANKING
            if (ecs::PlayerRuntime::IsPC(owner))
                ecs::PlayerRuntime::SetRankPoints(owner, 10, ecs::PlayerRuntime::GetRankPoints(owner, 10) + count);
#endif
            if (ecs::PlayerRuntime::IsPC(owner)) ecs::SessionSystem::Save(owner);
        }
        return true;
    }

    const uint32_t vnum = GetItemVnum(item);
#ifdef ENABLE_BATTLE_PASS
    const bool trackPickup = GetItemEvents(item).ownership != nullptr;
#endif
    const auto collected = [&](uint32_t count) {
#ifdef ENABLE_BATTLE_PASS
        if (!trackPickup || !ecs::PlayerRuntime::IsPC(owner)) return;
        const uint8_t pass = ecs::PlayerRuntime::GetBattlePassId(owner);
        if (!pass) return;
        for (const auto mission : {COLLECT_ITEM, COLLECT_ITEM1, COLLECT_ITEM2}) {
            if (!ecs::PlayerRuntime::IsPC(owner)) return;
            uint32_t wantedVnum = 0, goal = 0;
            if (CBattlePass::instance().BattlePassMissionGetInfo(pass, mission, &wantedVnum, &goal) &&
                wantedVnum == vnum && ecs::PlayerRuntime::GetMissionProgress(owner, mission, pass) < goal)
                ecs::PlayerRuntime::UpdateMissionProgress(owner, mission, pass, count, goal);
        }
#endif
    };
    const auto nameForOwner = [&](entt::entity source) {
        auto* desc = ecs::PlayerRuntime::GetDesc(owner);
        return std::string(GetItemName(source, desc ? desc->GetLanguage() : 0));
    };
    const auto notify = [&](uint32_t count, const std::string& name) {
#ifdef TEXTS_IMPROVEMENT
        if (ecs::PlayerRuntime::IsPC(owner))
            ecs::ChatSystem::SendNew(owner,
#ifdef ENABLE_NEW_CHAT
                CHAT_TYPE_INFO_ITEM,
#else
                CHAT_TYPE_INFO,
#endif
                102, "%d#%s", count, name.c_str());
#endif
    };
#ifdef ENABLE_EXTRA_INVENTORY
    const bool extra = IsExtraItem(item);
#else
    const bool extra = false;
#endif
    if (IsItemStackable(item) && !(GetItemAntiFlags(item) & ITEM_ANTIFLAG_STACK)) {
        const int slots =
#ifdef ENABLE_EXTRA_INVENTORY
            extra ? EXTRA_INVENTORY_MAX_NUM :
#endif
            INVENTORY_MAX_NUM;
        for (int cell = 0; cell < slots; ++cell) {
            if (!ground() || !ecs::PlayerRuntime::IsPC(owner) || !IsOwnership(item, owner)) return false;
            const auto target =
#ifdef ENABLE_EXTRA_INVENTORY
                extra ? GetExtraInventoryItem(owner, cell) :
#endif
                GetInventoryItem(owner, cell);
            if (!IsValidItem(target)) continue;
            const std::string name = nameForOwner(target);
            const auto merged = MergeItemStacksEcs(owner, item, target, 0, StackSource::GroundPickup);
            if (!merged.transferred) continue;
            collected(merged.transferred);
            if (merged.sourceDepleted) { notify(merged.transferred, name); return true; }
        }
    }
    if (!ground() || !ecs::PlayerRuntime::IsPC(owner) || !IsOwnership(item, owner)) return false;
    const uint8_t window = IsDragonSoulItem(item) ? DRAGON_SOUL_INVENTORY :
#ifdef ENABLE_EXTRA_INVENTORY
        extra ? EXTRA_INVENTORY :
#endif
        INVENTORY;
    const auto emptyCell = [&] {
        if (window == DRAGON_SOUL_INVENTORY) return GetEmptyDragonSoulInventory(owner, item);
#ifdef ENABLE_EXTRA_INVENTORY
        if (window == EXTRA_INVENTORY) return GetEmptyExtraInventory(owner, item);
#endif
        return InventorySystem::GetEmptyInventory(owner, GetItemSize(item));
    };
    int cell = emptyCell();
    if (cell < 0 && owner != character) {
#ifdef ENABLE_BUG_FIXES
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1248, "%s", ecs::PlayerRuntime::GetName(owner).data());
#endif
        return false;
#else
        owner = character;
        cell = emptyCell();
#endif
    }
    if (cell < 0) {
#ifdef TEXTS_IMPROVEMENT
        ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, extra ? 539 : 366, "");
#endif
        return false;
    }
    const uint32_t count = GetItemCount(item);
    const std::string name = nameForOwner(item);
    const uint32_t originalVnum = GetItemOriginalVnum(item);
    const int32_t map = ecs::PlayerRuntime::GetMapIndex(item);
    const PIXEL_POSITION position {ecs::PlayerRuntime::GetX(item), ecs::PlayerRuntime::GetY(item), ecs::PlayerRuntime::GetZ(item)};
    InventorySystem::RemoveFromGround(item);
    if (!IsValidItem(item)) return false;
    if (!InventorySystem::AddToCharacter(item, owner, TItemPos(window, cell))) {
        // Restore only our still-detached item, never a callback-transferred one.
        if (PlaceItemOnGround(item, map, position)) SetGroundOwnership(item, owner);
        return false;
    }
    collected(count);
    if (IsValidItem(item) && ecs::PlayerRuntime::IsPC(owner) && GetItemOwner(item) == owner) {
        char hint[64];
        snprintf(hint, sizeof(hint), "%s %u %u", name.c_str(), count, originalVnum);
        LogManager::instance().ItemLogEntity(owner, item, "GET", hint);
    }
    notify(count, name);
    return true;
}

entt::entity GetItem(entt::entity e, TItemPos cell)
{
    if (e == entt::null || !g_registry.valid(e))
        return entt::null;

    switch (cell.window_type)
    {
    case INVENTORY:
        return GetMainInventoryItem(e, cell.cell);
    case EQUIPMENT:
        if (cell.cell >= INVENTORY_AND_EQUIP_SLOT_MAX - INVENTORY_MAX_NUM)
            return entt::null;
        return GetMainInventoryItem(e, static_cast<uint16_t>(INVENTORY_MAX_NUM + cell.cell));
    case DRAGON_SOUL_INVENTORY:
        if (cell.cell < DRAGON_SOUL_INVENTORY_MAX_NUM)
            if (const auto* inventory = g_registry.try_get<ecs::DragonSoulInventoryComponent>(e))
                return IsValidItem(inventory->items[cell.cell]) ? inventory->items[cell.cell] : entt::null;
        return entt::null;
#ifdef ENABLE_EXTRA_INVENTORY
    case EXTRA_INVENTORY:
        if (cell.cell < EXTRA_INVENTORY_MAX_NUM)
            if (const auto* inventory = g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(e))
                return IsValidItem(inventory->items[cell.cell]) ? inventory->items[cell.cell] : entt::null;
        return entt::null;
#endif
#ifdef ENABLE_SWITCHBOT
    case SWITCHBOT:
        if (cell.cell < SWITCHBOT_SLOT_COUNT)
            if (const auto* switchbot = g_registry.try_get<ecs::SwitchbotRuntimeComponent>(e))
                return IsValidItem(switchbot->items[cell.cell]) ? switchbot->items[cell.cell] : entt::null;
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
    return inventory && IsValidItem(inventory->items[cell]) ? inventory->items[cell] : entt::null;
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

    return UseItemEx(owner, item, destCell);
}


bool IsValidItem(entt::entity item)
{
    return item != entt::null && g_registry.valid(item) &&
           g_registry.any_of<ecs::ItemIdentity>(item);
}

bool CheckItemUseLevel(entt::entity item, int level)
{
    const auto* proto = IsValidItem(item) ? GetItemProto(item) : nullptr;
    if (!proto)
        return false;
    for (const auto& limit : proto->aLimits)
        if (limit.bType == LIMIT_LEVEL)
            return level >= limit.lValue;
    return true;
}

bool OnAfterCreatedItem(entt::entity item)
{
    if (!IsValidItem(item) || !GetItemProto(item))
        return false;

    if (GetItemProto(item)->cLimitRealTimeFirstUseIndex != -1 && GetItemSocket(item, 1) != 0)
        if (!StartRealTimeExpireEventEcs(item))
            return false;

    // Timer publication can retire the item or replace its prototype. Re-read
    // the live entity before deciding whether it also needs soul charging.
    if (!IsValidItem(item) || !GetItemProto(item))
        return false;
#ifdef ENABLE_SOUL_SYSTEM
    if (GetItemType(item) == ITEM_SOUL &&
        static_cast<int>(GetItemSocket(item, 2) / 10000) < GetItemProto(item)->aLimits[1].lValue)
        if (!StartSoulItemEventEcs(item))
            return false;
#endif
    return IsValidItem(item);
}

bool IsDragonSoulItem(entt::entity item)
{
    return IsValidItem(item) && GetItemType(item) == ITEM_DS;
}

bool IsExtraItem(entt::entity item)
{
#ifdef ENABLE_EXTRA_INVENTORY
    if (!IsValidItem(item)) return false;
    // Classify the item's own prototype, not a second table lookup through a
    // masked display vnum. This preserves the original per-item rules.
    switch (GetItemVnum(item)) {
    case 70612: case 70613: case 70614: case 88968:
    case 30002: case 30003: case 30004: case 30005: case 30006:
    case 30015: case 30047: case 30050: case 30165: case 30166: case 30167: case 30168:
    case 30251: case 30252: case 2870: case 2871: case 2872: case 2873: case 2874:
    case 2875: case 2876: case 2877: case 2878:
        return false;
    case 30277: case 30279: case 30284: case 86053: case 86054: case 86055:
    case 70102: case 39008: case 71001: case 72310: case 39030: case 71094:
#ifdef __NEWPET_SYSTEM__
    case 86077: case 86076: case 55010: case 55011: case 55012: case 55013:
    case 55014: case 55015: case 55016: case 55017: case 55018: case 55019: case 55020: case 55021:
#endif
    case 50513: case 50525: case 50526: case 50527: case 71095:
        return true;
    }
    switch (GetItemType(item)) {
    case ITEM_MATERIAL: case ITEM_METIN: case ITEM_SKILLBOOK: case ITEM_SKILLFORGET:
    case ITEM_GIFTBOX: case ITEM_TREASURE_BOX: case ITEM_TREASURE_KEY:
        return true;
    case ITEM_USE:
        return IsExtraEnchantUseSubtype(GetItemSubType(item)) || IsExtraPotionUseSubtype(GetItemSubType(item));
    }
#else
    (void)item;
#endif
    return false;
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

const char* GetItemName(entt::entity item, uint8_t language)
{
    const auto* proto = IsValidItem(item) ? GetItemProto(item) : nullptr;
    if (!proto) return "";
#ifdef ENABLE_MULTI_NAMES
    constexpr size_t count = std::extent_v<decltype(TItemTable::szLocaleName)>;
    constexpr uint8_t fallback = count > 1 ? 1 : 0;
    if (language == 0) {
        language = fallback;
        const auto owner = GetItemOwner(item);
        if (owner != entt::null)
            if (auto* desc = ecs::PlayerRuntime::GetDesc(owner); desc && desc->GetLanguage() != 0)
                language = desc->GetLanguage();
    }
    if (language >= count) language = fallback;
    return proto->szLocaleName[language][0] ? proto->szLocaleName[language] : proto->szName;
#else
    return proto->szLocaleName;
#endif
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

bool ConsumeItem(entt::entity item, uint32_t amount)
{
    if (item == entt::null || !g_registry.valid(item) || amount == 0 || IsItemConsumptionPending(item))
        return false;

    const uint32_t count = GetItemCount(item);
    if (amount > count)
        return false;
    if (count > amount) {
        return SetItemCountEcs(item, count - amount);
    }

    return RetireItemEntity(item, "CONSUME_ITEM");
}

bool ConsumeItemEcs(entt::entity item, uint32_t amount)
{
    return ConsumeItem(item, amount);
}

bool DestroyItemEntityEcs(entt::entity item, const char* reason)
{
    return RetireItemEntity(item, reason ? reason : "DESTROY_ITEM_ENTITY_ECS");
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
    StartTimerBasedOnWearExpireEventEcs(item);
}

void StopTimerBasedOnWearExpireEvent(entt::entity item)
{
    auto* events = IsValidItem(item) ? g_registry.try_get<ecs::ItemEvents>(item) : nullptr;
    if (!events || !events->timerBasedOnWearExpire) return;
    LPEVENT pending = events->timerBasedOnWearExpire;
    const int elapsed = event_processing_time(pending) / passes_per_sec;
    events = IsValidItem(item) ? g_registry.try_get<ecs::ItemEvents>(item) : nullptr;
    if (!events || events->timerBasedOnWearExpire != pending) return;
    events->timerBasedOnWearExpire = nullptr;
    event_cancel(&pending);
    if (!IsValidItem(item) || !g_registry.all_of<ecs::ItemSockets>(item)) return;
    bool consumesTime = true;
#ifdef ENABLE_RUNE_SYSTEM
    consumesTime = !IsRuneItem(item) || (GetItemSubType(item) != RUNE_SLOT7 && GetItemSocket(item, 1) == 1);
#endif
    if (consumesTime) {
        const int remaining = g_registry.get<ecs::ItemSockets>(item).sockets[0];
        g_registry.get<ecs::ItemSockets>(item).sockets[0] = static_cast<int32_t>(
            std::max<int64_t>(0, static_cast<int64_t>(remaining) - std::max(0, elapsed)));
    }
    // Detach/cancel first. A save/update callback may remove ItemEvents, retire
    // the item, or install a new timer; none may be touched through the old reference.
    SaveItem(item);
    if (!IsValidItem(item)) return;
    ecs::ItemNetworkSystem::SendItemUpdate(g_registry, item);
    if (IsValidItem(item)) ITEM_MANAGER::instance().FlushDelayedSave(item);
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
    if (!g_registry.valid(item)) return;
    const uint32_t id = GetItemID(item);
    ecs::ItemEvents timers {};
    if (auto* events = g_registry.try_get<ecs::ItemEvents>(item))
        timers = std::exchange(*events, {});
    // Detach every timer first. Cancellation/destructors may retire the entity;
    // subsequent cancellations then operate on these owned local handles only.
    event_cancel(&timers.destroy);
    event_cancel(&timers.expire);
    event_cancel(&timers.ownership);
    event_cancel(&timers.uniqueExpire);
    event_cancel(&timers.soulItem);
    event_cancel(&timers.timerBasedOnWearExpire);
    event_cancel(&timers.realTimeExpire);
    event_cancel(&timers.accessorySocketExpire);
    if (g_registry.valid(item)) g_dispatcher.trigger(ecs::EvItemDestroyed {item, id});
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
    auto roll = [&](entt::entity memberEntity)
    {
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

bool DistanceValid(entt::entity item, entt::entity character)
{
    if (!IsValidItem(item) || !ecs::PlayerRuntime::IsPC(character) ||
        !ecs::PlayerRuntime::GetSectree(item) ||
        ecs::PlayerRuntime::GetMapIndex(item) != ecs::PlayerRuntime::GetMapIndex(character)) return false;
    const int64_t dx = std::abs(int64_t(ecs::PlayerRuntime::GetX(item)) - ecs::PlayerRuntime::GetX(character));
    const int64_t dy = std::abs(int64_t(ecs::PlayerRuntime::GetY(item)) - ecs::PlayerRuntime::GetY(character));
    return std::max(dx, dy) + std::min(dx, dy) / 2 <= 2400;
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
	}

	return true;
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

// The prototype reads UseItemEx still made through an item pointer.
uint8_t GetItemApplyType(entt::entity item, uint32_t idx)
{
    const TItemTable* proto = GetItemProto(item);
    return proto ? proto->aApplies[idx].bType : 0;
}

int32_t GetItemApplyValue(entt::entity item, uint32_t idx)
{
    const TItemTable* proto = GetItemProto(item);
    return proto ? proto->aApplies[idx].lValue : 0;
}

bool IsItemPCBangItem(entt::entity item)
{
    const TItemTable* proto = GetItemProto(item);
    if (!proto)
        return false;
    for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
        if (proto->aLimits[i].bType == LIMIT_PCBANG)
            return true;
    return false;
}

// How many sockets are filled, counting up to the first empty one.
int GetItemSocketCount(entt::entity item)
{
    for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
        if (GetItemSocket(item, i) == 0)
            return i;
    return ITEM_SOCKET_MAX_NUM;
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
#if defined(__SOULBINDING_SYSTEM__) || defined(ENABLE_SOULBIND_SYSTEM)
#error "Soulbinding requires an ECS binding component and persistence before it can be enabled."
#endif
    // Neither legacy soulbinding feature has an implementation in this server.
    (void)item;
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

bool StopTimerBasedOnWearExpireEventEcs(entt::entity item)
{
    if (!IsValidItem(item))
        return false;

    ItemSystem::StopTimerBasedOnWearExpireEvent(item);
    return true;
}

namespace {
// Allocation and event publication are callback boundaries. Never keep a
// component reference through either, or overwrite a nested timer start.
template <typename EventInfo = item_vid_event_info>
bool StartItemRuntimeTimer(entt::entity item, LPEVENT ecs::ItemEvents::*slot,
    TEVENTFUNC callback, int32_t delay, bool newPotion = false)
{
    if (!IsValidItem(item))
        return false;
    if (!g_registry.all_of<ecs::ItemEvents>(item))
        g_registry.insert<ecs::ItemEvents>(&item, &item + 1);
    if (!IsValidItem(item) || !g_registry.all_of<ecs::ItemEvents>(item))
        return false;
    if (g_registry.get<ecs::ItemEvents>(item).*slot)
        return true;

    auto* info = AllocEventInfo<EventInfo>();
    info->item = item;
#ifdef ENABLE_NEW_USE_POTION
    if constexpr (std::is_same_v<EventInfo, item_vid_event_info>)
        info->newpotion = newPotion;
#endif
    LPEVENT pending = event_create(callback, info, delay);
    if (!pending)
        return false;
    auto* state = IsValidItem(item) ? g_registry.try_get<ecs::ItemEvents>(item) : nullptr;
    if (!state || state->*slot)
    {
        event_cancel(&pending);
        const auto* current = IsValidItem(item) ? g_registry.try_get<ecs::ItemEvents>(item) : nullptr;
        return current && bool(current->*slot);
    }
    state->*slot = pending;
    g_dispatcher.trigger(ecs::EvItemExpired { item, GetItemID(item) });
    const auto* published = IsValidItem(item) ? g_registry.try_get<ecs::ItemEvents>(item) : nullptr;
    const bool scheduled = published && bool(published->*slot);
    if (!scheduled || published->*slot != pending) event_cancel(&pending);
    return scheduled;
}
} // namespace

bool StartTimerBasedOnWearExpireEventEcs(entt::entity item)
{
    if (!IsValidItem(item) || !GetItemProto(item)) return false;
    // A live item without this limit has nothing to schedule (as before).
    if (IsRealTimeItem(item) || GetItemProto(item)->cLimitTimerBasedOnWearIndex == -1) return true;
    const int seconds = static_cast<int32_t>(GetItemSocket(item, ITEM_SOCKET_REMAIN_SEC));
    const int delay = seconds <= 0 ? 1 : seconds % 60 == 0 ? 60 : seconds % 60;
    return StartItemRuntimeTimer<item_event_info>(item, &ecs::ItemEvents::timerBasedOnWearExpire,
        timer_based_on_wear_expire_event, PASSES_PER_SEC(delay));
}

bool StartRealTimeExpireEventEcs(entt::entity item)
{
    if (!IsValidItem(item))
        return false;

    const auto* events = g_registry.try_get<ecs::ItemEvents>(item);
    if (events && events->realTimeExpire)
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

#ifdef ENABLE_NEW_USE_POTION
        const int32_t delay = PASSES_PER_SEC(isNewPotion ? std::min(remainSec, 60) : 1);
        return StartItemRuntimeTimer(item, &ecs::ItemEvents::realTimeExpire,
            real_time_expire_event, delay, isNewPotion);
#else
        return StartItemRuntimeTimer(item, &ecs::ItemEvents::realTimeExpire,
            real_time_expire_event, PASSES_PER_SEC(1));
#endif
    }

    return false;
}

#ifdef ENABLE_SOUL_SYSTEM
bool StartSoulItemEventEcs(entt::entity item)
{
    if (!IsValidItem(item) || GetItemType(item) != ITEM_SOUL)
        return false;

    const auto* events = g_registry.try_get<ecs::ItemEvents>(item);
    if (events && events->soulItem)
        return true;

    const TItemTable* proto = GetItemProto(item);
    if (!proto)
        return false;

    const int minutes = static_cast<int>(GetItemSocket(item, 2) / 10000);
    if (minutes >= proto->aLimits[1].lValue)
        return false;

    return StartItemRuntimeTimer(item, &ecs::ItemEvents::soulItem,
        soul_item_event, PASSES_PER_SEC(test_server ? 5 : 60));
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

bool CanReceiveItemEcs(entt::entity receiver, entt::entity fromEntity, entt::entity item)
{
    if (!ecs::PlayerRuntime::IsValid(receiver) || ecs::PlayerRuntime::IsPC(receiver) ||
        !ecs::PlayerRuntime::IsPC(fromEntity) || !CanConsumeOwnedItem(fromEntity, item) ||
        ecs::PlayerRuntime::GetMapIndex(receiver) != ecs::PlayerRuntime::GetMapIndex(fromEntity))
        return false;

    const int64_t dx = std::abs(int64_t(ecs::PlayerRuntime::GetX(receiver)) - ecs::PlayerRuntime::GetX(fromEntity));
    const int64_t dy = std::abs(int64_t(ecs::PlayerRuntime::GetY(receiver)) - ecs::PlayerRuntime::GetY(fromEntity));
    if (dx > 2000 || dy > 2000 || DISTANCE_APPROX(static_cast<int>(dx), static_cast<int>(dy)) > 2000)
        return false;

	uint32_t racenum = ecs::PlayerRuntime::GetRaceNum(receiver);

	if (racenum == DEVILTOWER_BLACKSMITH_WEAPON_MOB ||
		racenum == DEVILTOWER_BLACKSMITH_ARMOR_MOB ||
		racenum == DEVILTOWER_BLACKSMITH_ACCESSORY_MOB) {
		bool bCanProced = true;

		for (uint8_t i = 0; i < ITEM_LIMIT_MAX_NUM; ++i) {
			if (ItemSystem::GetItemLimitType(item, i) == LIMIT_LEVEL && ItemSystem::GetItemLimitValue(item, i) >= 90) {
				bCanProced = false;
				break;
			}
		}

		if (!bCanProced) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 1360, "");
#endif
			return false;
		}
	}

	switch (racenum)
	{
	case fishing::CAMPFIRE_MOB:
		if (GetItemType(item) == ITEM_FISH &&
			(GetItemSubType(item) == FISH_ALIVE || GetItemSubType(item) == FISH_DEAD))
			return true;
		break;

	case fishing::FISHER_MOB:
		if (GetItemType(item) == ITEM_ROD)
			return true;
		break;

	case BLACKSMITH_WEAPON_MOB:
	case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
		if (GetItemType(item) == ITEM_WEAPON && GetItemRefineVnum(item)) {
			return true;
		}
		else {
			return false;
		}
		break;
	case BLACKSMITH_ARMOR_MOB:
	case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
		if ((GetItemType(item) == ITEM_BELT || (GetItemType(item) == ITEM_ARMOR && (GetItemSubType(item) == ARMOR_BODY || GetItemSubType(item) == ARMOR_SHIELD || GetItemSubType(item) == ARMOR_HEAD))) && GetItemRefineVnum(item)) {
			return true;
		}
		else {
			return false;
		}
		break;
	case BLACKSMITH_ACCESSORY_MOB:
	case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
		if (GetItemType(item) == ITEM_ARMOR && !(GetItemSubType(item) == ARMOR_BODY || GetItemSubType(item) == ARMOR_SHIELD || GetItemSubType(item) == ARMOR_HEAD
#ifdef ENABLE_PENDANT
			|| GetItemSubType(item) == ARMOR_PENDANT
#endif
			) && GetItemRefineVnum(item)) {
			return true;
		}
		else {
			return false;
		}
		break;
	case BLACKSMITH_MOB:
	case BLACKSMITH2_MOB:
		if (GetItemRefineVnum(item) && GetItemRefineSet(item)) {
			return true;
		}
		else {
			return false;
		}
	case ALCHEMIST_MOB:
		if (GetItemRefineVnum(item))
			return true;
		break;

	case 20101:
	case 20102:
	case 20103:
		// ÃÃÂ±Ã Â¸Â»
		if (GetItemVnum(item) == ITEM_REVIVE_HORSE_1)
		{
			if (!CombatSystem::IsDead(receiver))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (GetItemVnum(item) == ITEM_HORSE_FOOD_1)
		{
			if (CombatSystem::IsDead(receiver))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 466, "");
#endif
				return false;
			}
			return true;
		}
		else if (GetItemVnum(item) == ITEM_HORSE_FOOD_2 || GetItemVnum(item) == ITEM_HORSE_FOOD_3)
		{
			return false;
		}
		break;
	case 20104:
	case 20105:
	case 20106:
		// ÃÃÂ±Ã Â¸Â»
		if (GetItemVnum(item) == ITEM_REVIVE_HORSE_2)
		{
			if (!CombatSystem::IsDead(receiver))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (GetItemVnum(item) == ITEM_HORSE_FOOD_2)
		{
			if (CombatSystem::IsDead(receiver))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 466, "");
#endif
				return false;
			}
			return true;
		}
		else if (GetItemVnum(item) == ITEM_HORSE_FOOD_1 || GetItemVnum(item) == ITEM_HORSE_FOOD_3)
		{
			return false;
		}
		break;
	case 20107:
	case 20108:
	case 20109:
		// Â°Ã­Â±Ã Â¸Â»
		if (GetItemVnum(item) == ITEM_REVIVE_HORSE_3)
		{
			if (!CombatSystem::IsDead(receiver))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (GetItemVnum(item) == ITEM_HORSE_FOOD_3)
		{
			if (CombatSystem::IsDead(receiver))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 466, "");
#endif
				return false;
			}
			return true;
		}
		else if (GetItemVnum(item) == ITEM_HORSE_FOOD_1 || GetItemVnum(item) == ITEM_HORSE_FOOD_2)
		{
			return false;
		}
		break;
	}

	//if (IS_SET(GetItemFlags(item), ITEM_FLAG_QUEST_GIVE))
	{
		return true;
	}

	return false;
}

bool ReceiveItemEcs(entt::entity receiver, entt::entity fromEntity, entt::entity item)
{
    if (!CanReceiveItemEcs(receiver, fromEntity, item))
        return false;
    // Callbacks may consume the last item or destroy its entity.
    const std::string itemName = GetItemName(item);
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
	// Rune Dungeon: key pedestal (20507) consumes 89103 and progresses floor 5
	if (CRuneDungeon::instance().OnNpcTakeItem(fromEntity, receiver, item))
		return true;
	if (CHalloween2022Dungeon::instance().OnNpcTakeItem(fromEntity, receiver, item))
		return true;
	if (CVikingDungeon::instance().OnNpcTakeItem(fromEntity, receiver, item))
		return true;
#endif
	const entt::entity itemEntity = item;
	switch (ecs::PlayerRuntime::GetRaceNum(receiver))
	{
	case fishing::CAMPFIRE_MOB:
		if (GetItemType(item) == ITEM_FISH && (GetItemSubType(item) == FISH_ALIVE || GetItemSubType(item) == FISH_DEAD))
			fishing::GrillFishEcs(fromEntity, itemEntity);
		else
		{
			// TAKE_ITEM_BUG_FIX
			ecs::PlayerRuntime::SetQuestNPCID(fromEntity, ecs::PlayerRuntime::GetPacketVID(receiver));
			// END_OF_TAKE_ITEM_BUG_FIX
			quest::CQuestManager::instance().TakeItem(ecs::PlayerRuntime::GetPlayerID(fromEntity), ecs::PlayerRuntime::GetRaceNum(receiver), itemEntity);
		}
		break;

		// DEVILTOWER_NPC
	case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
	case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
	case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB: {
		int set = GetItemRefineSet(item);
		if (GetItemRefineVnum(item) != 0 && set != 0 /*&& GetItemRefineSet(item) < 500*/
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
			InventorySystem::SetRefineNPC(fromEntity, receiver);
			ItemSystem::RefineInformation(fromEntity, ItemSystem::GetItemCell(itemEntity), REFINE_TYPE_MONEY_ONLY);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 427, "");
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
		if (GetItemRefineVnum(item))
		{
			InventorySystem::SetRefineNPC(fromEntity, receiver);
			ItemSystem::RefineInformation(fromEntity, ItemSystem::GetItemCell(itemEntity), REFINE_TYPE_NORMAL);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 427, "");
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
		if (GetItemVnum(item) == ITEM_REVIVE_HORSE_1 ||
			GetItemVnum(item) == ITEM_REVIVE_HORSE_2 ||
			GetItemVnum(item) == ITEM_REVIVE_HORSE_3)
		{
			MountSystem::ReviveHorse(fromEntity);
			ItemSystem::ConsumeItemEcs(itemEntity);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 329, "%s", itemName.c_str());
#endif
		}
		else if (GetItemVnum(item) == ITEM_HORSE_FOOD_1 ||
			GetItemVnum(item) == ITEM_HORSE_FOOD_2 ||
			GetItemVnum(item) == ITEM_HORSE_FOOD_3)
		{
			MountSystem::FeedHorse(fromEntity);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 112, "%s", itemName.c_str());
#endif
			ItemSystem::ConsumeItemEcs(itemEntity);
			NetworkSyncSystem::BroadcastEffect(g_registry, receiver, SE_HPUP_RED);
		}
		break;

	default:
		LOG_INFO("TakeItem {} {} {}", ecs::PlayerRuntime::GetName(fromEntity), ecs::PlayerRuntime::GetRaceNum(receiver), itemName.c_str());
		ecs::PlayerRuntime::SetQuestNPCID(fromEntity, ecs::PlayerRuntime::GetPacketVID(receiver));
		quest::CQuestManager::instance().TakeItem(ecs::PlayerRuntime::GetPlayerID(fromEntity), ecs::PlayerRuntime::GetRaceNum(receiver), itemEntity);
		break;
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

} // namespace ItemSystem
