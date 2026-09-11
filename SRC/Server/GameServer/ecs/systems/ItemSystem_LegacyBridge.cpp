#include "../../stdafx.h"
#include "../../exchange.h"
#include "ViewSystem.hpp"
#include "PlayerRuntimeSystem.hpp"
#include "AffectSystem.hpp"
#include "ActivitySystem.hpp"

#include "ItemSystem.hpp"
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
#include <Core/Logging.hpp>
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
#include "../components/visibility_components.hpp"
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
#include "DragonSoulSystem.hpp"
#include "../../Halloween2022Dungeon.h"
#include "../../VikingDungeon.h"
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

bool IS_SUMMONABLE_ZONE(int map_index);
bool IS_BOTARYABLE_ZONE(int nMapIndex);
extern int stone_chance;

namespace {

const int ITEM_BROKEN_METIN_VNUM = 28960;

using LegacyCharHandle = decltype(std::declval<ecs::LegacyCharPtr>().ptr);

struct FFindStone
{
	// The stones found in range. Only their positions are ever read, so
	// there is nothing here a CHARACTER pointer answers that the entity
	// does not.
	std::map<uint32_t, entt::entity> m_mapStone;

	void operator()(LPENTITY pEnt)
	{
		if (pEnt->IsType(ENTITY_CHARACTER) == true)
		{
			const entt::entity pChar = pEnt->GetEntityHandle();

			if (ecs::PlayerRuntime::IsStone(pChar))
			{
				m_mapStone[ecs::PlayerRuntime::GetPacketVID(pChar)] = pChar;
			}
		}
	}
};

static LPITEM LegacyItemBoundary(entt::entity itemEntity);

static ecs::MainInventoryRuntimeComponent* EnsureMainInventoryRuntimeComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::MainInventoryRuntimeComponent>(character))
        return comp;

    return &g_registry.emplace<ecs::MainInventoryRuntimeComponent>(character);
}

static const ecs::MainInventoryRuntimeComponent* TryGetMainInventoryRuntimeComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    return g_registry.try_get<ecs::MainInventoryRuntimeComponent>(character);
}

static LPITEM GetMainInventoryItem(entt::entity character, uint16_t cell)
{
    if (cell >= INVENTORY_AND_EQUIP_SLOT_MAX)
        return nullptr;

    if (const auto* comp = TryGetMainInventoryRuntimeComponent(character))
        return LegacyItemBoundary(comp->items[cell]);

    return nullptr;
}

static uint16_t GetMainInventoryGrid(entt::entity character, uint16_t cell)
{
    if (cell >= INVENTORY_AND_EQUIP_SLOT_MAX)
        return 0;

    if (const auto* comp = TryGetMainInventoryRuntimeComponent(character))
        return comp->itemGrid[cell];

    return 0;
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

static const ecs::ExtraInventoryRuntimeComponent* TryGetExtraInventoryRuntimeComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    return g_registry.try_get<ecs::ExtraInventoryRuntimeComponent>(character);
}
#endif

static ecs::DragonSoulInventoryComponent* EnsureDragonSoulInventoryComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::DragonSoulInventoryComponent>(character))
        return comp;

    return &g_registry.emplace<ecs::DragonSoulInventoryComponent>(character);
}

static const ecs::DragonSoulInventoryComponent* TryGetDragonSoulInventoryComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    return g_registry.try_get<ecs::DragonSoulInventoryComponent>(character);
}


static ecs::CubeWindowComponent* EnsureCubeWindowComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::CubeWindowComponent>(character))
        return comp;

    return &g_registry.emplace<ecs::CubeWindowComponent>(character);
}

static const ecs::CubeWindowComponent* TryGetCubeWindowComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    return g_registry.try_get<ecs::CubeWindowComponent>(character);
}


#ifdef ENABLE_ACCE_SYSTEM
static ecs::AcceWindowComponent* EnsureAcceWindowComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    if (auto* comp = g_registry.try_get<ecs::AcceWindowComponent>(character))
        return comp;

    return &g_registry.emplace<ecs::AcceWindowComponent>(character);
}

static const ecs::AcceWindowComponent* TryGetAcceWindowComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    return g_registry.try_get<ecs::AcceWindowComponent>(character);
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

static const ecs::SwitchbotRuntimeComponent* TryGetSwitchbotRuntimeComponent(entt::entity character)
{
    if (character == entt::null || !g_registry.valid(character))
        return nullptr;

    return g_registry.try_get<ecs::SwitchbotRuntimeComponent>(character);
}
#endif

static LPITEM LegacyItemBoundary(entt::entity itemEntity)
{
    if (itemEntity == entt::null || !g_registry.valid(itemEntity))
        return nullptr;

    const auto* legacy = g_registry.try_get<ecs::LegacyItemPtr>(itemEntity);
    return legacy ? legacy->ptr : nullptr;
}


static uint32_t ItemVnumOrLegacy(LPITEM item)
{
    if (!item)
        return 0;

    entt::entity e = (item ? item->GetEntityHandle() : entt::null);
    if (e != entt::null)
    {
        if (const auto* identity = g_registry.try_get<ecs::ItemIdentity>(e))
            return identity->vnum;
    }

    return item->GetVnum();
}

static void SyncItemFlagsComponent(LPITEM item)
{
    entt::entity e = (item ? item->GetEntityHandle() : entt::null);
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

static void FN_copy_item_socket(entt::entity dest, entt::entity src)
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		ItemSystem::SetItemSocketEcs(dest, i, ItemSystem::GetItemSocket(src, i));
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



// item socket º¹»ç -- by mhh



} // namespace

EVENTFUNC(unique_expire_event);
EVENTFUNC(timer_based_on_wear_expire_event);
EVENTFUNC(real_time_expire_event);
EVENTFUNC(accessory_socket_expire_event);
EVENTFUNC(soul_item_event);

// Legacy CHARACTER/CItem method bodies split from ItemSystem.cpp.
// Keep gameplay semantics unchanged; this file is a physical bridge island.


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

		if (GetOwnerEntity() != entt::null)
		{
			if (LPDESC d = ecs::PlayerRuntime::GetDesc(GetOwnerEntity()))
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
    const auto item = GetEntityHandle();
    const int limit = ItemSystem::GetItemType(item) == ITEM_ELK ? INT_MAX : std::max(0, int(g_bItemCountLimit));
    return static_cast<int>(std::min(ItemSystem::GetItemCount(item), static_cast<uint32_t>(limit)));
}

bool CItem::SetCount(int count)
{
    // This is only the compatibility boundary. The ECS component owns the
    // count; never access this after publication, which may delete the item.
    const auto item = GetEntityHandle();
    if (!ItemSystem::IsValidItem(item) || ItemSystem::IsItemConsumptionPending(item))
        return false;
    if (count < 0) {
        LOG_ERROR("SetCount attempted negative value (count={}) vnum={}", count, ItemSystem::GetItemVnum(item));
        count = 0;
    }
    const bool committed = ItemSystem::SetItemCountEcs(item, static_cast<uint32_t>(count));
    // Preserve the legacy caller contract: false for zero/deleted items.
    return count > 0 && committed && ItemSystem::IsValidItem(item) &&
        !ItemSystem::IsItemConsumptionPending(item);
}

int32_t CItem::GetValue(uint32_t idx)
{
	assert(idx < ITEM_VALUES_MAX_NUM);
	return GetProto()->alValues[idx];
}


// The socket and attribute arrays live in ecs::ItemSockets and
// ecs::ItemAttributes. These accessors are what is left of the members that
// used to hold them: every read goes to the component for this item entity,
// and an item with no entity yet reads as empty.
namespace {
const ecs::ItemSockets& SocketsOf(entt::entity e)
{
	static const ecs::ItemSockets kEmpty {};
	if (e == entt::null || !g_registry.valid(e))
		return kEmpty;
	const auto* c = g_registry.try_get<ecs::ItemSockets>(e);
	return c ? *c : kEmpty;
}

const ecs::ItemAttributes& AttributesOf(entt::entity e)
{
	static const ecs::ItemAttributes kEmpty {};
	if (e == entt::null || !g_registry.valid(e))
		return kEmpty;
	const auto* c = g_registry.try_get<ecs::ItemAttributes>(e);
	return c ? *c : kEmpty;
}

ecs::ItemSockets* MutableSocketsOf(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;
	return &g_registry.get_or_emplace<ecs::ItemSockets>(e);
}

ecs::ItemAttributes* MutableAttributesOf(entt::entity e)
{
	if (e == entt::null || !g_registry.valid(e))
		return nullptr;
	return &g_registry.get_or_emplace<ecs::ItemAttributes>(e);
}
} // namespace

namespace {
const ecs::ItemFlags& FlagsOf(entt::entity e)
{
	static const ecs::ItemFlags kEmpty {};
	if (e == entt::null || !g_registry.valid(e))
		return kEmpty;
	const auto* c = g_registry.try_get<ecs::ItemFlags>(e);
	return c ? *c : kEmpty;
}
} // namespace

bool CItem::IsExchanging() const
{
	return FlagsOf(GetEntityHandle()).exchanging;
}

bool CItem::isLocked() const
{
	return FlagsOf(GetEntityHandle()).isLocked;
}

void CItem::Lock(bool f)
{
	if (GetEntityHandle() == entt::null || !g_registry.valid(GetEntityHandle()))
		return;
	g_registry.get_or_emplace<ecs::ItemFlags>(GetEntityHandle()).isLocked = f;
}

short CItem::GetLockedAttr() const
{
	if (GetEntityHandle() == entt::null || !g_registry.valid(GetEntityHandle()))
		return -1;
	const auto* c = g_registry.try_get<ecs::ItemLockedAttribute>(GetEntityHandle());
	return c ? c->index : -1;
}

const int32_t* CItem::GetSockets() const
{
	return SocketsOf(GetEntityHandle()).sockets.data();
}

int32_t CItem::GetSocket(int i) const
{
	return (i >= 0 && i < ITEM_SOCKET_MAX_NUM) ? SocketsOf(GetEntityHandle()).sockets[i] : 0;
}

const TPlayerItemAttribute* CItem::GetAttributes() const
{
	return AttributesOf(GetEntityHandle()).attrs.data();
}

const TPlayerItemAttribute& CItem::GetAttribute(int i) const
{
	static const TPlayerItemAttribute kEmpty {};
	if (i < 0 || i >= ITEM_ATTRIBUTE_MAX_NUM)
		return kEmpty;
	return AttributesOf(GetEntityHandle()).attrs[i];
}

uint8_t CItem::GetAttributeType(int i) const
{
	return GetAttribute(i).bType;
}

short CItem::GetAttributeValue(int i) const
{
	return GetAttribute(i).sValue;
}

void CItem::SetSockets(const int32_t* c_al)
{
	if (auto* sockets = MutableSocketsOf(GetEntityHandle()))
		std::copy_n(c_al, ITEM_SOCKET_MAX_NUM, sockets->sockets.begin());
	Save();
}

void CItem::SetSocket(int i, int32_t v, bool bLog)
{
	assert(i < ITEM_SOCKET_MAX_NUM);
	if (auto* sockets = MutableSocketsOf(GetEntityHandle()))
		sockets->sockets[i] = v;
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

// Phase 11: migrated from item_attribute.cpp batch A

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

void CItem::SetAttributes(const TPlayerItemAttribute* c_pAttribute)

{

	if (auto* attributes = MutableAttributesOf(GetEntityHandle()))
		std::copy_n(c_pAttribute, ITEM_ATTRIBUTE_MAX_NUM, attributes->attrs.begin());

	Save();

}

void CItem::SetAttribute(int i, uint8_t bType, short sValue)

{

	assert(i < MAX_NORM_ATTR_NUM);

	if (auto* attributes = MutableAttributesOf(GetEntityHandle()))
	{
		attributes->attrs[i].bType = bType;
		attributes->attrs[i].sValue = sValue;
	}

	UpdatePacket();

	Save();

	if (bType)

	{

		const char * pszIP = nullptr;

		if (GetOwnerEntity() != entt::null && ecs::PlayerRuntime::GetDesc(GetOwnerEntity()))

			pszIP = ecs::PlayerRuntime::GetDesc(GetOwnerEntity())->GetHostName();

		LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().ItemLog(i, bType, sValue, GetID(), "SET_ATTR", "", pszIP ? pszIP : "", GetOriginalVnum()));

	}

}

// Phase 11: migrated from item_attribute.cpp batch B

bool CItem::ChangeKKAK(int iAddonType)
{
	(void)iAddonType;

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

void CItem::AddAttr4(uint8_t bApply, uint8_t bLevel)
{
	if (ItemSystem::HasItemAttribute(GetEntityHandle(), bApply))
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

// Phase 11: migrated from item_attribute.cpp batch C

int CItem::GetRareAttrCount()
{
	int ret = 0;

	for (uint32_t dwIdx = ITEM_ATTRIBUTE_RARE_START; dwIdx < ITEM_ATTRIBUTE_RARE_END; dwIdx++)
	{
		if (GetAttributeType(dwIdx) != 0)
			ret++;
	}

	return ret;
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

namespace ItemSystem {
bool CheckItemUseLevel(entt::entity item, int level)
{
	LPITEM legacy = LegacyItemBoundary(item);
	return legacy ? legacy->CheckItemUseLevel(level) : false;
}

bool OnAfterCreatedItem(entt::entity item)
{
	LPITEM legacy = LegacyItemBoundary(item);
	return legacy ? legacy->OnAfterCreatedItem() : false;
}
} // namespace ItemSystem

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
			ItemSystem::StartRealTimeExpireEventEcs(GetEntityHandle());
		}
	}

#ifdef ENABLE_SOUL_SYSTEM
	if (GetType() == ITEM_SOUL)
	{
		ItemSystem::StartSoulItemEventEcs(GetEntityHandle());
	}
#endif

	return true;
}

bool CItem::IsDragonSoul()
{
	return GetType() == ITEM_DS;
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
void CHARACTER::SetCubeNpc(entt::entity npcEntity)
{
    ecs::SessionSystem::SetCubeNPC(GetEntityHandle(), npcEntity);

}

bool CHARACTER::IsCubeOpen() const
{
    return ecs::SessionSystem::IsCubeOpen(GetEntityHandle());
}


#ifdef ENABLE_ACCE_SYSTEM
#endif

#ifdef ENABLE_SWITCHBOT
LPITEM CHARACTER::GetSwitchbotItem(uint16_t wCell) const
{
    if (wCell >= SWITCHBOT_SLOT_COUNT)
        return nullptr;

    if (const auto* switchbot = TryGetSwitchbotRuntimeComponent(GetEntityHandle()))
        return LegacyItemBoundary(switchbot->items[wCell]);

    return nullptr;
}
#endif

LPITEM CHARACTER::GetDragonSoulItem(uint16_t wCell) const
{
	if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		return nullptr;

	if (const auto* comp = TryGetDragonSoulInventoryComponent(GetEntityHandle()))
		return LegacyItemBoundary(comp->items[wCell]);

	return nullptr;
}

uint16_t CHARACTER::GetDragonSoulGrid(uint16_t wCell) const
{
	if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		return 0;

	if (const auto* comp = TryGetDragonSoulInventoryComponent(GetEntityHandle()))
		return comp->itemGrid[wCell];

	return 0;
}

LPITEM CHARACTER::GetExtraInventoryItem(uint16_t wCell) const
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	LOG_INFO("Razor93 LOG:: Called: Char_item.cpp LPITEM CHARACTER::GetExtraInventoryItem(uint16_t wCell) const");
#endif
	if (wCell >= EXTRA_INVENTORY_MAX_NUM)
		return nullptr;

	if (const auto* comp = TryGetExtraInventoryRuntimeComponent(GetEntityHandle()))
		return LegacyItemBoundary(comp->items[wCell]);

	return nullptr;
}

#endif

LPITEM CHARACTER::GetItem(TItemPos Cell) const
{

	if (!InventorySystem::IsValidItemPosition(GetEntityHandle(), Cell))
		return nullptr;
	uint16_t wCell = Cell.cell;
	uint8_t window_type = Cell.window_type;
	switch (window_type)
	{
	case INVENTORY:
		if (wCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
		{
			LOG_ERROR("CHARACTER::GetInventoryItem: invalid item cell {}", wCell);
			return nullptr;
		}
		return GetMainInventoryItem(GetEntityHandle(), wCell);
	case EQUIPMENT:
	{
		const uint16_t storageCell = static_cast<uint16_t>(INVENTORY_MAX_NUM + wCell);
		if (storageCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
		{
			LOG_ERROR("CHARACTER::GetInventoryItem: invalid equipment cell {}", wCell);
			return nullptr;
		}
		return GetMainInventoryItem(GetEntityHandle(), storageCell);
	}
	case DRAGON_SOUL_INVENTORY:
		if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			LOG_ERROR("CHARACTER::GetInventoryItem: invalid DS item cell {}", wCell);
			return nullptr;
		}
		return GetDragonSoulItem(wCell);

#ifdef ENABLE_EXTRA_INVENTORY
	case EXTRA_INVENTORY:
		if (wCell >= EXTRA_INVENTORY_MAX_NUM)
		{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			LOG_INFO("Razor93 LOG:: Called: Char_item.cpp line :315: case switch :if (wCell >= EXTRA_INVENTORY_MAX_NUM)");
#endif
			LOG_ERROR("CHARACTER::GetInventoryItem: invalid EXTRA item cell {}", wCell);
			return nullptr;
		}
		return GetExtraInventoryItem(wCell);
#endif

#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
		if (wCell >= SWITCHBOT_SLOT_COUNT)
		{
			LOG_ERROR("CHARACTER::GetInventoryItem: invalid switchbot item cell {}", wCell);
			return nullptr;
		}
		return GetSwitchbotItem(wCell);
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
				if (ecs::SocialSystem::GetMyShop(GetEntityHandle()) && ecs::SocialSystem::GetMyShop(GetEntityHandle())->IsSellingItem(item->GetID())) {
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
				if (ecs::SocialSystem::GetMyShop(GetEntityHandle()) && ecs::SocialSystem::GetMyShop(GetEntityHandle())->IsSellingItem(item->GetID()))
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
			const entt::entity item = ItemSystem::GetExtraInventoryItem(GetEntityHandle(), i);

			if (item == entt::null)
				continue;

			if (ItemSystem::GetItemVnum(item) != vnum)
				continue;

			if (ecs::SocialSystem::GetMyShop(GetEntityHandle()))
			{
				if (ecs::SocialSystem::GetMyShop(GetEntityHandle())->IsSellingItem(ItemSystem::GetItemID(item)))
					continue;
			}

			if (cuberenewal) {
				if (ItemSystem::GetItemLockedAttr(item) != -1) {
					continue;
				}
			}

			if (count >= ItemSystem::GetItemCount(item))
			{
				count -= ItemSystem::GetItemCount(item);
				ItemSystem::ConsumeItemEcs(item, ItemSystem::GetItemCount(item));

				if (0 == count)
					return;
			}
			else
			{
				ItemSystem::ConsumeItemEcs(item, count);
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
			const entt::entity item = ItemSystem::GetInventoryItem(GetEntityHandle(), i);
			if (item == entt::null)
				continue;

			if (ItemSystem::GetItemVnum(item) != vnum)
				continue;

			if (ecs::SocialSystem::GetMyShop(GetEntityHandle()) && ecs::SocialSystem::GetMyShop(GetEntityHandle())->IsSellingItem(ItemSystem::GetItemID(item)))
				continue;

			if (cuberenewal && ItemSystem::GetItemLockedAttr(item) != -1)
				continue;

			if (vnum >= 80003 && vnum <= 80007)
				LogManager::instance().GoldBarLog(GetPlayerID(), ItemSystem::GetItemID(item), QUEST, "RemoveSpecifyItem");

			const int itemCount = ItemSystem::GetItemCount(item);
			if (count >= itemCount)
			{
				count -= itemCount;
				ItemSystem::ConsumeItemEcs(item, itemCount);

				if (0 == count)
					return;
			}
			else
			{
				ItemSystem::ConsumeItemEcs(item, count);
				return;
			}
		}

	// ?1?�A3���! 3a�I�U.
	if (count)
		LOG_INFO("CHARACTER::RemoveSpecifyItem cannot remove enough item vnum {}, still remain {}", vnum, count);
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
		const entt::entity pItem = ItemSystem::GetInventoryItem(GetEntityHandle(), i);
		if (pItem != entt::null && ItemSystem::GetItemType(pItem) == type)
		{
			count += ItemSystem::GetItemCount(pItem);
		}
	}

	return count;
}


namespace ecs::PlayerRuntime {

void SetWear(entt::entity e, uint8_t bCell, entt::entity item)
{
	// > WEAR_MAX_NUM : ?¡EY1¢ 11µÐµµ.
	if (bCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
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
bool CHARACTER::IsEquipUniqueItem(uint32_t dwItemVnum) const
{
	{
		const entt::entity u = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_UNIQUE1);

		if (u != entt::null && ItemSystem::GetItemVnum(u) == dwItemVnum)
			return true;
	}

	{
		const entt::entity u = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_UNIQUE2);

		if (u != entt::null && ItemSystem::GetItemVnum(u) == dwItemVnum)
			return true;
	}

	{
		const entt::entity u = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_COSTUME_MOUNT);

		if (u != entt::null && ItemSystem::GetItemVnum(u) == dwItemVnum)
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
		const entt::entity u = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_UNIQUE1);

		if (u != entt::null && ItemSystem::GetItemSpecialGroup(u) == (int)dwGroupVnum)
			return true;
	}

	{
		const entt::entity u = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_UNIQUE2);

		if (u != entt::null && ItemSystem::GetItemSpecialGroup(u) == (int)dwGroupVnum)
			return true;
	}

	{
		const entt::entity u = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_COSTUME_MOUNT);

		if (u != entt::null && ItemSystem::GetItemSpecialGroup(u) == (int)dwGroupVnum)
			return true;
	}

	return false;
}


bool CHARACTER::UnEquipSpecialRideUniqueItem()
{
	const entt::entity Unique1 = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_UNIQUE1);
	const entt::entity Unique2 = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_UNIQUE2);
	const entt::entity Unique3 = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_COSTUME_MOUNT);

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	const entt::entity MountCostume = ItemSystem::GetWearItem(GetEntityHandle(), WEAR_COSTUME_MOUNT);
#endif


	if (Unique1 != entt::null)
	{
		if (UNIQUE_GROUP_SPECIAL_RIDE == ItemSystem::GetItemSpecialGroup(Unique1))
		{
			return ItemSystem::UnequipItemEcs(GetEntityHandle(), Unique1);
		}
	}

	if (Unique2 != entt::null)
	{
		if (UNIQUE_GROUP_SPECIAL_RIDE == ItemSystem::GetItemSpecialGroup(Unique2))
		{
			return ItemSystem::UnequipItemEcs(GetEntityHandle(), Unique2);
		}
	}

	if (Unique3 != entt::null)
	{
		if (UNIQUE_GROUP_SPECIAL_RIDE == ItemSystem::GetItemSpecialGroup(Unique3))
		{
			return ItemSystem::UnequipItemEcs(GetEntityHandle(), Unique3);
		}
	}

	/*#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (MountCostume != entt::null)
			return ItemSystem::UnequipItemEcs(GetEntityHandle(), MountCostume);
	#endif*/

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
			if (ItemSystem::IsOwnership(item->GetEntityHandle(), ch->GetEntityHandle()))
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
					ecs::PointSystem::Change((ch ? ch->GetEntityHandle() : entt::null), POINT_GOLD, iMoney, true);

					if (iMoney > 1000) // Ãµ¿ø ÀÌ»ó¸¸ ±â·ÏÇÑ´Ù.
					{
						LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(ch->GetEntityHandle(), iMoney, "GET_GOLD", ""));
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
		LOG_ERROR("I am a stupid hacker 1: {} {}", GetName(), bCount);
		stupid = true;
	}

	bCount = abs(bCount);
	if (stupid)
	{
		LOG_ERROR("I am a stupid hacker 2: {} {}", GetName(), bCount);
		return false;
	}

	LPITEM item = nullptr;

	if (!CanHandleItem())
	{
#ifdef TEXTS_IMPROVEMENT
		if (DragonSoulSystem::CanRefine(GetEntityHandle())) {
			ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 232, "");
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
			LogManager::instance().HackLog("DROP_HACK", GetEntityHandle());
			desc->SetPhase(PHASE_CLOSE);
		}

		return false;
	}
#endif

	if (CombatSystem::IsDead(GetEntityHandle()))
		return false;

	if (!InventorySystem::IsValidItemPosition(GetEntityHandle(), Cell) || !(item = GetItem(Cell)))
		return false;

	if (item->isLocked() || item->IsExchanging() || ItemSystem::IsItemEquipped(item->GetEntityHandle()))
		return false;

	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
		return false;

	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_DROP | ITEM_ANTIFLAG_GIVE))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 353, "");
#endif
		return false;
	}

	if (bCount == 0 || bCount > item->GetCount())
		bCount = item->GetCount();

#ifdef ENABLE_EXTRA_INVENTORY
	if (item->IsExtraItem()) {
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
		ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "char_item.cpp::if (item->IsExtraItem()) {");//INGAME_DEBUG_RAZOR93

		LOG_INFO("Razor93 LOG:: Called: Char_item.cpp line 8391 if (item->IsExtraItem()) {{ ");

#endif
		SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, Cell.cell, 255);
	}
	else {
		SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 255);
	}
#else
	SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 255);
#endif

	entt::entity pkItemToDrop = entt::null;

	if (bCount == item->GetCount())
	{
		InventorySystem::RemoveFromCharacter(item->GetEntityHandle());
		pkItemToDrop = item->GetEntityHandle();
	}
	else
	{
		if (bCount == 0)
		{
			if (test_server)
				LOG_INFO("[DROP_ITEM] drop item count == 0");
			return false;
		}

		ItemSystem::ConsumeItemEcs((item ? item->GetEntityHandle() : entt::null), bCount);
		ITEM_MANAGER::instance().FlushDelayedSave(item->GetEntityHandle());

		pkItemToDrop = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), bCount);

		// copy item socket -- by mhh
		FN_copy_item_socket(pkItemToDrop, item->GetEntityHandle());

		char szBuf[51 + 1];
		snprintf(szBuf, sizeof(szBuf), "%u %u", ItemSystem::GetItemID(pkItemToDrop), ItemSystem::GetItemCount(pkItemToDrop));
		LogManager::instance().ItemLogEntity(GetEntityHandle(), item->GetEntityHandle(), "ITEM_SPLIT", szBuf);
	}

	PIXEL_POSITION pxPos = GetXYZ();

#ifdef ENABLE_NEWSTUFF
	const int dropDestroySeconds = g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPITEM];
#else
	const int dropDestroySeconds = 300;
#endif
	if (ItemSystem::PlaceItemOnGround(pkItemToDrop, GetMapIndex(), pxPos, dropDestroySeconds))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 321, "%d",
#ifdef ENABLE_NEWSTUFF
			g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPITEM]
#else
			300
#endif
		);
#endif

		ItemSystem::FlushDelayedSaveEcs(pkItemToDrop);

		char szHint[32 + 1];
		snprintf(szHint, sizeof(szHint), "%s %u %u", ItemSystem::GetItemName(pkItemToDrop), ItemSystem::GetItemCount(pkItemToDrop), ItemSystem::GetItemOriginalVnum(pkItemToDrop));
		LogManager::instance().ItemLogEntity(GetEntityHandle(), pkItemToDrop, "DROP", szHint);
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
	if (gold <= 0 || gold > ecs::PointSystem::GetGold(GetEntityHandle()))
		return false;

	if (!CanHandleItem())
		return false;

	if (0 != g_GoldDropTimeLimitValue)
	{
		if (get_dword_time() < m_dwLastGoldDropTime + g_GoldDropTimeLimitValue)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 510, "");
#endif
			return false;
		}
	}

	m_dwLastGoldDropTime = get_dword_time();

	const entt::entity item = ITEM_MANAGER::instance().CreateItem(1, gold);

	if (ItemSystem::IsValidItem(item))
	{
		PIXEL_POSITION pos = GetXYZ();

#ifdef ENABLE_NEWSTUFF
		const int goldDestroySeconds = g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPGOLD];
#else
		const int goldDestroySeconds = 300;
#endif
		if (ItemSystem::PlaceItemOnGround(item, GetMapIndex(), pos, goldDestroySeconds))
		{
			//Motion(MOTION_PICKUP);
			PointChange(POINT_GOLD, -gold, true);

			if (gold > 1000) // Ãµ¿ø ÀÌ»ó¸¸ ±â·ÏÇÑ´Ù.
				LogManager::instance().CharLog(GetEntityHandle(), gold, "DROP_GOLD", "");

#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 321, "%d", (150 / 60));
#endif
		}

		ecs::SessionSystem::Save(GetEntityHandle());
		return true;
	}

	return false;
}

bool CHARACTER::PickupItem(uint32_t dwVID)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "char_item.cpp::bool CHARACTER::PickupItem ");//INGAME_DEBUG_RAZOR93
#endif
	if (!IsPC() || CombatSystem::IsDead(GetEntityHandle()) || IsObserverMode())
	{
		return false;
	}

	LPITEM item = ITEM_MANAGER::instance().FindByVID(dwVID);
	if (!item || !item->GetSectree())
		return false;

#ifdef ENABLE_BATTLE_PASS
	bool bIsBattlePass = (ItemSystem::GetItemEvents(item->GetEntityHandle()).ownership != nullptr);
#endif

	if (ItemSystem::DistanceValid(item->GetEntityHandle(), GetEntityHandle()))
	{
		// @fixme150 BEGIN
		if (item->GetType() == ITEM_QUEST)
		{
			if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 692, "");
#endif
				return false;
			}
		}
		// @fixme150 END

		if (ItemSystem::IsOwnership(item->GetEntityHandle(), GetEntityHandle()))
		{
			// ¸¸¾à ÁÖÀ¸·Á ÇÏ´Â ¾ÆÀÌ�
// ÛÀÌ ¿¤�
// ©¶ó¸é
			if (item->GetType() == ITEM_ELK)
			{
				ItemSystem::GiveGold(GetEntityHandle(), (int64_t)item->GetCount());
				InventorySystem::RemoveFromGround(item->GetEntityHandle());
#ifdef ENABLE_RANKING
				SetRankPoints(10, GetRankPoints(10) + item->GetCount());
#endif
				ItemSystem::DestroyItemEntityEcs(
					(item ? item->GetEntityHandle() : entt::null),
					"PICKUP_GOLD");

				ecs::SessionSystem::Save(GetEntityHandle());
			}
			// Æò¹üÇÑ ¾ÆÀÌ�
// ÛÀÌ¶ó¸é
			else
			{
#ifdef ENABLE_EXTRA_INVENTORY
				if (item->IsExtraItem() && item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
				{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
					ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "char_item.cpp:else if (item->IsExtraItem() && item->IsStackable() && !IS_SET(item->GetAntiFlag()..");//INGAME_DEBUG_RAZOR93

					LOG_INFO("Razor93 LOG:: Called: Char_item.cpp if (item->IsExtraItem() && item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK)) ");

#endif
#ifdef ENABLE_NEW_STACK_LIMIT
					int
#else
					uint8_t
#endif
						bCount = item->GetCount(); // change type for some

					for (int i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
					{
						const entt::entity item2 = ItemSystem::GetExtraInventoryItem(GetEntityHandle(), i);

						if (item2 == entt::null)
							continue;

						if (ItemSystem::GetItemVnum(item2) == item->GetVnum())
						{
							int j = 0;

							for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
								if (ItemSystem::GetItemSocket(item2, j) != item->GetSocket(j))
									break;

							if (j != ITEM_SOCKET_MAX_NUM)
								continue;

#ifdef ENABLE_NEW_STACK_LIMIT
							int
#else
							uint8_t
#endif
								bCount2 = static_cast<uint8_t>(std::min<int64_t>(
						g_bItemCountLimit - ItemSystem::GetItemCount(item2), bCount)); // change type for some
							bCount -= bCount2;

#ifdef ENABLE_BATTLE_PASS
							if (bIsBattlePass)
							{
								uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(GetEntityHandle());
								if (bBattlePassId)
								{
									uint32_t dwItemVnum, dwCount;
									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(GetEntityHandle(), COLLECT_ITEM, bBattlePassId) < dwCount)
											ecs::PlayerRuntime::UpdateMissionProgress(GetEntityHandle(), COLLECT_ITEM, bBattlePassId, bCount2, dwCount);
									}

									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(GetEntityHandle(), COLLECT_ITEM1, bBattlePassId) < dwCount)
											ecs::PlayerRuntime::UpdateMissionProgress(GetEntityHandle(), COLLECT_ITEM1, bBattlePassId, bCount2, dwCount);
									}

									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM2, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(GetEntityHandle(), COLLECT_ITEM2, bBattlePassId) < dwCount)
											ecs::PlayerRuntime::UpdateMissionProgress(GetEntityHandle(), COLLECT_ITEM2, bBattlePassId, bCount2, dwCount);
									}
								}
							}
#endif

							ItemSystem::AddItemCountEcs(item2, bCount2);
							ItemSystem::ConsumeItemEcs((item ? item->GetEntityHandle() : entt::null), bCount2);

							if (bCount == 0)
							{
#ifdef TEXTS_IMPROVEMENT
								ecs::ChatSystem::SendNew(GetEntityHandle(),
#ifdef ENABLE_NEW_CHAT
									CHAT_TYPE_INFO_ITEM
#else
									CHAT_TYPE_INFO
#endif
									, 102, "%d#%s", bCount2, ItemSystem::GetItemName(item2));
								//ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 01 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
								return true;
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
						const entt::entity item2 = ItemSystem::GetInventoryItem(GetEntityHandle(), i);

						if (item2 == entt::null)
							continue;

						if (ItemSystem::GetItemVnum(item2) == item->GetVnum())
						{
							int j;

							for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
								if (ItemSystem::GetItemSocket(item2, j) != item->GetSocket(j))
									break;

							if (j != ITEM_SOCKET_MAX_NUM)
								continue;

#ifdef ENABLE_NEW_STACK_LIMIT
							int
#else
							uint8_t
#endif
								bCount2 = static_cast<uint8_t>(std::min<int64_t>(
						g_bItemCountLimit - ItemSystem::GetItemCount(item2), bCount));
							bCount -= bCount2;
#ifdef ENABLE_BATTLE_PASS
							if (bIsBattlePass)
							{
								uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(GetEntityHandle());
								if (bBattlePassId)
								{
									uint32_t dwItemVnum, dwCount;
									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(GetEntityHandle(), COLLECT_ITEM, bBattlePassId) < dwCount)
											ecs::PlayerRuntime::UpdateMissionProgress(GetEntityHandle(), COLLECT_ITEM, bBattlePassId, bCount2, dwCount);
									}

									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(GetEntityHandle(), COLLECT_ITEM1, bBattlePassId) < dwCount)
											ecs::PlayerRuntime::UpdateMissionProgress(GetEntityHandle(), COLLECT_ITEM1, bBattlePassId, bCount2, dwCount);
									}

									if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM2, &dwItemVnum, &dwCount))
									{
										if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(GetEntityHandle(), COLLECT_ITEM2, bBattlePassId) < dwCount)
											ecs::PlayerRuntime::UpdateMissionProgress(GetEntityHandle(), COLLECT_ITEM2, bBattlePassId, bCount2, dwCount);
									}
								}
							}
#endif
							ItemSystem::AddItemCountEcs(item2, bCount2);
							ItemSystem::ConsumeItemEcs((item ? item->GetEntityHandle() : entt::null), bCount2);

							if (bCount == 0)
							{
#ifdef TEXTS_IMPROVEMENT
								ecs::ChatSystem::SendNew(GetEntityHandle(),
#ifdef ENABLE_NEW_CHAT
									CHAT_TYPE_INFO_ITEM
#else
									CHAT_TYPE_INFO
#endif
									, 102, "%d#%s", bCount2, ItemSystem::GetItemName(item2));
								//ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 02 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
								return true;
							}
						}
					}

				}

				int iEmptyCell;
				if (item->IsDragonSoul())
				{
					if ((iEmptyCell = GetEmptyDragonSoulInventory(item)) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 366, "");
#endif
						return false;
					}
				}
#ifdef ENABLE_EXTRA_INVENTORY
				else if (item->IsExtraItem())
				{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
					ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "char_item.cpp: line 9217  else if (item->IsExtraItem()).");//INGAME_DEBUG_RAZOR93

					LOG_INFO("Razor93 LOG:: Called: Char_item.cpp else if (item->IsExtraItem()) ");

#endif
					if ((iEmptyCell = GetEmptyExtraInventory(item)) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 539, "");
#endif
						return false;
					}
				}
#endif
				else
				{
					if ((iEmptyCell = InventorySystem::GetEmptyInventory(GetEntityHandle(), item->GetSize())) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 366, "");
#endif
						return false;
					}
				}

				InventorySystem::RemoveFromGround(item->GetEntityHandle());

				if (item->IsDragonSoul())
					InventorySystem::AddToCharacter(item->GetEntityHandle(), GetEntityHandle(), TItemPos(DRAGON_SOUL_INVENTORY, iEmptyCell));
#ifdef ENABLE_EXTRA_INVENTORY
				else if (item->IsExtraItem())
					InventorySystem::AddToCharacter(item->GetEntityHandle(), GetEntityHandle(), TItemPos(EXTRA_INVENTORY, iEmptyCell));
#endif
				else
					InventorySystem::AddToCharacter(item->GetEntityHandle(), GetEntityHandle(), TItemPos(INVENTORY, iEmptyCell));

#ifdef ENABLE_BATTLE_PASS
				if (bIsBattlePass)
				{
					uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(GetEntityHandle());
					if (bBattlePassId)
					{
						uint32_t dwItemVnum, dwCount;
						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
						{
							if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(GetEntityHandle(), COLLECT_ITEM, bBattlePassId) < dwCount)
								ecs::PlayerRuntime::UpdateMissionProgress(GetEntityHandle(), COLLECT_ITEM, bBattlePassId, item->GetCount(), dwCount);
						}

						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
						{
							if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(GetEntityHandle(), COLLECT_ITEM1, bBattlePassId) < dwCount)
								ecs::PlayerRuntime::UpdateMissionProgress(GetEntityHandle(), COLLECT_ITEM1, bBattlePassId, item->GetCount(), dwCount);
						}

						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM2, &dwItemVnum, &dwCount))
						{
							if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(GetEntityHandle(), COLLECT_ITEM2, bBattlePassId) < dwCount)
								ecs::PlayerRuntime::UpdateMissionProgress(GetEntityHandle(), COLLECT_ITEM2, bBattlePassId, item->GetCount(), dwCount);
						}
					}
				}
#endif

				char szHint[32 + 1];
				snprintf(szHint, sizeof(szHint), "%s %u %u", item->GetName(), item->GetCount(), item->GetOriginalVnum());
				LogManager::instance().ItemLog(this, item, "GET", szHint);
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(GetEntityHandle(),
#ifdef ENABLE_NEW_CHAT
					CHAT_TYPE_INFO_ITEM
#else
					CHAT_TYPE_INFO
#endif
					, 102, "%d#%s", item->GetCount(), item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));//földröl
				//ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 03 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
			}

			//Motion(MOTION_PICKUP);
			return true;
		}
		else if (!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP) && GetParty())
		{
			// ´Ù¸¥ ÆÄÆ¼¿ø ¼ÒÀ¯±Ç ¾ÆÀÌ�
// ÛÀ» ÁÖÀ¸·Á°í ÇÑ´Ù¸é
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
					const entt::entity item2 = ItemSystem::GetExtraInventoryItem(owner->GetEntityHandle(), i);

					if (item2 == entt::null)
						continue;

					if (ItemSystem::GetItemVnum(item2) == item->GetVnum())
					{
						int j = 0;

						for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
							if (ItemSystem::GetItemSocket(item2, j) != item->GetSocket(j))
								break;

						if (j != ITEM_SOCKET_MAX_NUM)
							continue;

#ifdef ENABLE_NEW_STACK_LIMIT
						int
#else
						uint8_t
#endif
							bCount2 = static_cast<uint8_t>(std::min<int64_t>(
						g_bItemCountLimit - ItemSystem::GetItemCount(item2), bCount)); // change type for some
						bCount -= bCount2;
#ifdef ENABLE_BATTLE_PASS
						if (bIsBattlePass)
						{
							uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(owner->GetEntityHandle());
							if (bBattlePassId)
							{
								uint32_t dwItemVnum, dwCount;
								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM, bBattlePassId) < dwCount)
										ecs::PlayerRuntime::UpdateMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM, bBattlePassId, bCount2, dwCount);
								}

								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM1, bBattlePassId) < dwCount)
										ecs::PlayerRuntime::UpdateMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM1, bBattlePassId, bCount2, dwCount);
								}

								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM2, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM2, bBattlePassId) < dwCount)
										ecs::PlayerRuntime::UpdateMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM2, bBattlePassId, bCount2, dwCount);
								}
							}
						}
#endif
						ItemSystem::AddItemCountEcs(item2, bCount2);
						ItemSystem::ConsumeItemEcs((item ? item->GetEntityHandle() : entt::null), bCount2);

						if (bCount == 0)
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew((owner ? owner->GetEntityHandle() : entt::null),
#ifdef ENABLE_NEW_CHAT
								CHAT_TYPE_INFO_ITEM
#else
								CHAT_TYPE_INFO
#endif
								, 102, "%d#%s", ItemSystem::GetItemCount(item2), ItemSystem::GetItemName(item2));
							//ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 04 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
							return true;
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
					const entt::entity item2 = ItemSystem::GetInventoryItem(owner->GetEntityHandle(), i);

					if (item2 == entt::null)
						continue;

					if (ItemSystem::GetItemVnum(item2) == item->GetVnum())
					{
						int j;

						for (j = 0; j < ITEM_SOCKET_MAX_NUM; ++j)
							if (ItemSystem::GetItemSocket(item2, j) != item->GetSocket(j))
								break;

						if (j != ITEM_SOCKET_MAX_NUM)
							continue;

#ifdef ENABLE_NEW_STACK_LIMIT
						int
#else
						uint8_t
#endif
							bCount2 = static_cast<uint8_t>(std::min<int64_t>(
						g_bItemCountLimit - ItemSystem::GetItemCount(item2), bCount));
						bCount -= bCount2;
#ifdef ENABLE_BATTLE_PASS
						if (bIsBattlePass)
						{
							uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(owner->GetEntityHandle());
							if (bBattlePassId)
							{
								uint32_t dwItemVnum, dwCount;
								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM, bBattlePassId) < dwCount)
										ecs::PlayerRuntime::UpdateMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM, bBattlePassId, bCount2, dwCount);
								}

								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM1, bBattlePassId) < dwCount)
										ecs::PlayerRuntime::UpdateMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM1, bBattlePassId, bCount2, dwCount);
								}

								if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM2, &dwItemVnum, &dwCount))
								{
									if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM2, bBattlePassId) < dwCount)
										ecs::PlayerRuntime::UpdateMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM2, bBattlePassId, bCount2, dwCount);
								}
							}
						}
#endif
						ItemSystem::AddItemCountEcs(item2, bCount2);
						ItemSystem::ConsumeItemEcs((item ? item->GetEntityHandle() : entt::null), bCount2);

						if (bCount == 0)
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew((owner ? owner->GetEntityHandle() : entt::null),
#ifdef ENABLE_NEW_CHAT
								CHAT_TYPE_INFO_ITEM
#else
								CHAT_TYPE_INFO
#endif
								, 102, "%d#%s", bCount2, ItemSystem::GetItemName(item2));
							//ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 05 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
							return true;
						}
					}
				}

			}

			int iEmptyCell;

			if (item->IsDragonSoul())
			{
				if (!(owner && (iEmptyCell = owner->GetEmptyDragonSoulInventory(item)) != -1))
				{
#ifdef ENABLE_BUG_FIXES
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 1248, "%s", owner->GetName());
#endif
					return false;
#else
					owner = this;

					if ((iEmptyCell = GetEmptyDragonSoulInventory(item)) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew((owner ? owner->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 366, "");
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
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 1248, "%s", owner->GetName());
#endif
					return false;
#else
					owner = this;

					if ((iEmptyCell = GetEmptyExtraInventory(item)) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 539, "");
#endif
						return false;
					}
#endif
				}
			}
#endif
			else
			{
				if (!(owner && (iEmptyCell = InventorySystem::GetEmptyInventory(owner->GetEntityHandle(), item->GetSize())) != -1))
				{
#ifdef ENABLE_BUG_FIXES
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 1248, "%s", owner->GetName());
#endif
					return false;
#else
					owner = this;

					if ((iEmptyCell = InventorySystem::GetEmptyInventory(GetEntityHandle(), item->GetSize())) == -1)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew((owner ? owner->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 366, "");
#endif
						return false;
					}
#endif
				}
			}

			InventorySystem::RemoveFromGround(item->GetEntityHandle());

			if (item->IsDragonSoul())
				InventorySystem::AddToCharacter(item->GetEntityHandle(), owner->GetEntityHandle(), TItemPos(DRAGON_SOUL_INVENTORY, iEmptyCell));
#ifdef ENABLE_EXTRA_INVENTORY
			else if (item->IsExtraItem())
				InventorySystem::AddToCharacter(item->GetEntityHandle(), owner->GetEntityHandle(), TItemPos(EXTRA_INVENTORY, iEmptyCell));
#endif
			else
				InventorySystem::AddToCharacter(item->GetEntityHandle(), owner->GetEntityHandle(), TItemPos(INVENTORY, iEmptyCell));

#ifdef ENABLE_BATTLE_PASS
			if (bIsBattlePass)
			{
				uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(owner->GetEntityHandle());
				if (bBattlePassId)
				{
					uint32_t dwItemVnum, dwCount;
					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM, &dwItemVnum, &dwCount))
					{
						if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM, bBattlePassId) < dwCount)
							ecs::PlayerRuntime::UpdateMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM, bBattlePassId, item->GetCount(), dwCount);
					}

					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
					{
						if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM1, bBattlePassId) < dwCount)
							ecs::PlayerRuntime::UpdateMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM1, bBattlePassId, item->GetCount(), dwCount);
					}

					if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COLLECT_ITEM1, &dwItemVnum, &dwCount))
					{
						if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM1, bBattlePassId) < dwCount)
							ecs::PlayerRuntime::UpdateMissionProgress(owner->GetEntityHandle(), COLLECT_ITEM1, bBattlePassId, item->GetCount(), dwCount);
					}
				}
			}
#endif

			char szHint[32 + 1];
			snprintf(szHint, sizeof(szHint), "%s %u %u", item->GetName(), item->GetCount(), item->GetOriginalVnum());
			LogManager::instance().ItemLog(owner, item, "GET", szHint);

			if (owner == this) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(GetEntityHandle(),
#ifdef ENABLE_NEW_CHAT
					CHAT_TYPE_INFO_ITEM
#else
					CHAT_TYPE_INFO
#endif
					, 102, "%d#%s", item->GetCount(), item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
				//ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 06 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew((owner ? owner->GetEntityHandle() : entt::null),
#ifdef ENABLE_NEW_CHAT
					CHAT_TYPE_INFO_ITEM
#else
					CHAT_TYPE_INFO
#endif
					, 102, "%d#%s", item->GetCount(), item->GetName(GetDesc() ? GetDesc()->GetLanguage() : 0));
				//ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "|cffffc700[Kaptál:]|r 07 |cffffff00%u x %s|r", item->GetCount(), item->GetName());

#endif
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew((owner ? owner->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 401, "%s", item->GetName());
#endif
			}

			return true;
		}
	}

	return false;
}

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

	auto*	ch = ecs::LegacyCharOf(info->ch);

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
		const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
		// CheckNeedLifeForWarp
		const int curLife = ecs::PlayerRuntime::GetHP(chEntity);
		const int needPercent = WARP_NEED_LIFE_PERCENT;
		const int needLife = ecs::PointSystem::GetMaxHP(chEntity) * needPercent / 100;
		if (curLife < needLife)
		{
#ifdef TEXTS_IMPROVEMENT
			if (ch) {
				ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 284, "");
			}
#endif
			return -1;
		}

		consumeLife = needLife;


		// CheckMinLifeForWarp: µ¶¿¡ ÀÇÇØ¼­ Á×À¸¸é ¾ÈµÇ¹Ç·Î »ý¸í·Â ÃÖ¼Ò·®´Â ³²°ÜÁØ´Ù
		const int minPercent = WARP_MIN_LIFE_PERCENT;
		const int minLife = ecs::PointSystem::GetMaxHP(chEntity) * minPercent / 100;
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
	const entt::entity lpCharEntity = lpChar ? lpChar->GetEntityHandle() : entt::null;
	static const int NEED_WARP_SP_PERCENT = 30;

	const int curSP = ecs::PlayerRuntime::GetSP(lpCharEntity);
	const int needSP = ecs::PointSystem::GetMaxSP(lpCharEntity) * NEED_WARP_SP_PERCENT / 100;

	if (curSP < needSP)
	{
#ifdef TEXTS_IMPROVEMENT
		if (lpChar) {
			ecs::ChatSystem::SendNew(lpCharEntity, CHAT_TYPE_INFO, 287, "");
		}
#endif
		return -1;
	}

	return needSP;
}

// #define ENABLE_FIREWORK_STUN
#define ENABLE_ADDSTONE_FAILURE
int g_nPortalLimitTime = 10;

namespace ItemSystem { void TransformRefineItem(entt::entity pkOldItem, entt::entity pkNewItem); }
void NotifyRefineSuccess(entt::entity ch, entt::entity item, const char* way);
void NotifyRefineFail(entt::entity ch, entt::entity item, const char* way, int success = 0);

void CHARACTER::SetRefineNPC(entt::entity npc)
{
    InventorySystem::SetRefineNPC(GetEntityHandle(), npc);
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

#else

#endif
#ifdef ENABLE_SOUL_SYSTEM

#endif

// MYSHOP_PRICE_LIST

void CHARACTER::SendMyShopPriceListCmd(uint32_t dwItemVnum, int64_t dwItemPrice)
{
	char szLine[256];
	snprintf(szLine, sizeof(szLine), "MyShopPriceList %u %lld", dwItemVnum, dwItemPrice);
	ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, szLine);
	LOG_INFO("{}", szLine);
}


//
// DB Ä³½Ã·Î ºÎ�
// Í ¹ÞÀº ¸®½ºÆ®¸¦ User ¿¡°Ô Àü¼ÛÇÏ°í »óÁ¡À» ¿­¶ó´Â Ä¿¸Çµå¸¦ º¸³½´Ù.
//

void CHARACTER::UseSilkBotaryReal(const TPacketMyshopPricelistHeader * p)
{
	const TItemPriceInfo* pInfo = (const TItemPriceInfo*)(p + 1);

	if (!p->byCount)
		// °¡°Ý ¸®½ºÆ®°¡ ¾ø´Ù. dummy µ¥ÀÌ�
// Í¸¦ ³ÖÀº Ä¿¸Çµå¸¦ º¸³»ÁØ´Ù.
		SendMyShopPriceListCmd(1, 0);
	else {
		for (int idx = 0; idx < p->byCount; idx++)
			SendMyShopPriceListCmd(pInfo[idx].dwVnum, pInfo[idx].dwPrice);
	}

#ifdef KASMIR_PAKET_SYSTEM
	ecs::SocialSystem::OpenPrivateShop(GetEntityHandle(), ecs::SocialSystem::GetKasmirPaket(GetEntityHandle()));
#else
	ecs::SocialSystem::OpenPrivateShop(GetEntityHandle(), false);
#endif
}

//
// ÀÌ¹ø Á¢¼Ó ÈÄ Ã³À½ »óÁ¡À» Open ÇÏ´Â °æ¿ì ¸®½ºÆ®¸¦ Load ÇÏ±â À§ÇØ DB Ä³½Ã¿¡ °¡°ÝÁ¤º¸ ¸®½ºÆ® ¿äÃ» ÆÐ�
// ¶À» º¸³½´Ù.
// ÀÌÈÄºÎ�
// Í´Â ¹Ù·Î »óÁ¡À» ¿­¶ó´Â ÀÀ´äÀ» º¸³½´Ù.
//

// END_OF_MYSHOP_PRICE_LIST

void CHARACTER::SetRefineMode(int additionalCell)
{
    InventorySystem::SetRefineMode(GetEntityHandle(), additionalCell);
}

void CHARACTER::ClearRefineMode()
{
    InventorySystem::ClearRefineMode(GetEntityHandle());
}

void NotifyRefineSuccess(entt::entity ch, entt::entity item, const char* way)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(ch, CHAT_TYPE_INFO, "char_item.cpp::void NotifyRefineSuccess ");//INGAME_DEBUG_RAZOR93
#endif
	if (ch != entt::null && ItemSystem::IsValidItem(item))
	{
		ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "RefineSuceeded");

		LogManager::instance().RefineLog(ecs::PlayerRuntime::GetPlayerID(ch), ItemSystem::GetItemName(item),
			ItemSystem::GetItemID(item), ItemSystem::GetItemRefineLevel(item), 1, way);
	}
}

void NotifyRefineFail(entt::entity ch, entt::entity item, const char* way, int success)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(ch, CHAT_TYPE_INFO, "char_item.cpp:: void NotifyRefineFail ");//INGAME_DEBUG_RAZOR93
#endif
	if (ch != entt::null && ItemSystem::IsValidItem(item))
	{
		ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "RefineFailed");

		LogManager::instance().RefineLog(ecs::PlayerRuntime::GetPlayerID(ch), ItemSystem::GetItemName(item),
			ItemSystem::GetItemID(item), ItemSystem::GetItemRefineLevel(item), success, way);
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
		const entt::entity item = ItemSystem::GetInventoryItem(GetEntityHandle(), i);
		if (item == entt::null)
			continue;

		if (GetInventoryItem(i)->GetType() != type)
			continue;


		if (ecs::SocialSystem::GetMyShop(GetEntityHandle()) && ecs::SocialSystem::GetMyShop(GetEntityHandle())->IsSellingItem(ItemSystem::GetItemID(item)))
			continue;

		const int itemCount = ItemSystem::GetItemCount(item);
		if (count >= itemCount)
		{
			count -= itemCount;
			ItemSystem::ConsumeItemEcs(item, itemCount);

			if (0 == count)
				return;
		}
		else
		{
			ItemSystem::ConsumeItemEcs(item, count);
			return;
		}
	}
}

entt::entity CHARACTER::AutoGiveItem(uint32_t vnum,
#ifdef ENABLE_NEW_STACK_LIMIT
    int
#else
    uint8_t
#endif
    count, int rarePct, bool message
#ifdef __HIGHLIGHT_SYSTEM__
    , bool highlight
#endif
)
{
    if (count <= 0) return entt::null;
    return ItemSystem::AutoGiveItemEcs(GetEntityHandle(), vnum, static_cast<uint32_t>(count), rarePct, message
#ifdef __HIGHLIGHT_SYSTEM__
        , highlight
#endif
    );
}

bool CHARACTER::GiveItem(entt::entity victimEntity, TItemPos Cell)
{
	if (!CanHandleItem())
		return false;

	// @fixme150 BEGIN
	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 740, "");
#endif
		return false;
	}
	// @fixme150 END

	const entt::entity item = ItemSystem::GetItem(GetEntityHandle(), Cell);

	if (item != entt::null && !ItemSystem::IsItemExchanging(item))
	{
		const entt::entity itemEntity =
			item;
		if (ItemSystem::ReceiveItemEcs(victimEntity,
				GetEntityHandle(), itemEntity))
			return true;
	}

	return false;
}

bool CHARACTER::CanReceiveItem(entt::entity fromEntity, LPITEM item) const
{
	LPCHARACTER from = ecs::LegacyCharOf(fromEntity);
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
			if (ItemSystem::GetItemLimitType(item->GetEntityHandle(), i) == LIMIT_LEVEL && ItemSystem::GetItemLimitValue(item->GetEntityHandle(), i) >= 90) {
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
			if (!CombatSystem::IsDead(GetEntityHandle()))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_1)
		{
			if (CombatSystem::IsDead(GetEntityHandle()))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 466, "");
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
			if (!CombatSystem::IsDead(GetEntityHandle()))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_2)
		{
			if (CombatSystem::IsDead(GetEntityHandle()))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 466, "");
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
			if (!CombatSystem::IsDead(GetEntityHandle()))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 467, "");
#endif
				return false;
			}
			return true;
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_3)
		{
			if (CombatSystem::IsDead(GetEntityHandle()))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 466, "");
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

void CHARACTER::ReceiveItem(entt::entity fromEntity, LPITEM item)
{
	LPCHARACTER from = ecs::LegacyCharOf(fromEntity);
	if (IsPC())
		return;
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
	// Rune Dungeon: key pedestal (20507) consumes 89103 and progresses floor 5
	if (CRuneDungeon::instance().OnNpcTakeItem(fromEntity, GetEntityHandle(), item))
		return;
	if (CHalloween2022Dungeon::instance().OnNpcTakeItem(fromEntity, GetEntityHandle(), item))
		return;
	if (CVikingDungeon::instance().OnNpcTakeItem(fromEntity, GetEntityHandle(), item))
		return;
	// LostCastle Dungeon: statue/totem item usage
	//if (CLostCastleDungeon::instance().OnNpcTakeItem(fromEntity, GetEntityHandle(), item))
	//	return;
#endif
	const entt::entity itemEntity = item ? item->GetEntityHandle() : entt::null;
	switch (GetRaceNum())
	{
	case fishing::CAMPFIRE_MOB:
		if (item->GetType() == ITEM_FISH && (item->GetSubType() == FISH_ALIVE || item->GetSubType() == FISH_DEAD))
			fishing::GrillFishEcs(fromEntity, itemEntity);
		else
		{
			// TAKE_ITEM_BUG_FIX
			ecs::PlayerRuntime::SetQuestNPCID(fromEntity, GetPacketVID());
			// END_OF_TAKE_ITEM_BUG_FIX
			quest::CQuestManager::instance().TakeItem(ecs::PlayerRuntime::GetPlayerID(fromEntity), GetRaceNum(), itemEntity);
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
			from->SetRefineNPC(GetEntityHandle());
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
		if (item->GetRefinedVnum())
		{
			from->SetRefineNPC(GetEntityHandle());
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
		if (item->GetVnum() == ITEM_REVIVE_HORSE_1 ||
			item->GetVnum() == ITEM_REVIVE_HORSE_2 ||
			item->GetVnum() == ITEM_REVIVE_HORSE_3)
		{
			from->ReviveHorse();
			ItemSystem::ConsumeItemEcs(itemEntity);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 329, "%s", item->GetName());
#endif
		}
		else if (item->GetVnum() == ITEM_HORSE_FOOD_1 ||
			item->GetVnum() == ITEM_HORSE_FOOD_2 ||
			item->GetVnum() == ITEM_HORSE_FOOD_3)
		{
			from->FeedHorse();
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(fromEntity, CHAT_TYPE_INFO, 112, "%s", item->GetName());
#endif
			ItemSystem::ConsumeItemEcs(itemEntity);
			NetworkSyncSystem::BroadcastEffect(g_registry, GetEntityHandle(), SE_HPUP_RED);
		}
		break;

	default:
		LOG_INFO("TakeItem {} {} {}", from->GetName(), GetRaceNum(), item->GetName());
		ecs::PlayerRuntime::SetQuestNPCID(fromEntity, GetPacketVID());
		quest::CQuestManager::instance().TakeItem(ecs::PlayerRuntime::GetPlayerID(fromEntity), GetRaceNum(), itemEntity);
		break;
	}
}

bool CHARACTER::GiveItemFromSpecialItemGroup(uint32_t dwGroupNum, std::vector<uint32_t> &dwItemVnums,
	std::vector<uint32_t> &dwItemCounts, std::vector<entt::entity> &item_gets, int& count)
{
	const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(dwGroupNum);

	if (!pGroup)
	{
		LOG_ERROR("cannot find special item group {}", dwGroupNum);
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
		entt::entity item_get = entt::null;
		switch (dwVnum)
		{
		case CSpecialItemGroup::GOLD:
			PointChange(POINT_GOLD, dwCount);
			LogManager::instance().CharLog(GetEntityHandle(), dwCount, "TREASURE_GOLD", "");

			bSuccess = true;
			break;
		case CSpecialItemGroup::EXP:
		{
			PointChange(POINT_EXP, dwCount);
			LogManager::instance().CharLog(GetEntityHandle(), dwCount, "TREASURE_EXP", "");

			bSuccess = true;
		}
		break;

		case CSpecialItemGroup::MOB:
		{
			LOG_INFO("CSpecialItemGroup::MOB {}", dwCount);
			int x = GetX() + number(-500, 500);
			int y = GetY() + number(-500, 500);

			auto* ch = CHARACTER_MANAGER::instance().SpawnMob(dwCount, GetMapIndex(), x, y, 0, true, -1);
			if (ch)
				CombatSystem::SetAggressive(ch->GetEntityHandle());
			bSuccess = true;
		}
		break;
		case CSpecialItemGroup::SLOW:
		{
			LOG_INFO("CSpecialItemGroup::SLOW {}", -(int)dwCount);
			AffectSystem::AddAffect(GetEntityHandle(), AFFECT_SLOW, POINT_MOV_SPEED, -(int)dwCount, AFF_SLOW, 300, 0, true);
			bSuccess = true;
		}
		break;
		case CSpecialItemGroup::DRAIN_HP:
		{
			int64_t iDropHP = ecs::PointSystem::GetMaxHP(GetEntityHandle()) * dwCount / 100;
			LOG_INFO("CSpecialItemGroup::DRAIN_HP {}", -iDropHP);
			iDropHP = std::min(iDropHP, ecs::PlayerRuntime::GetHP(GetEntityHandle()) - 1);
			LOG_INFO("CSpecialItemGroup::DRAIN_HP {}", -iDropHP);
			PointChange(POINT_HP, -iDropHP);
			bSuccess = true;
		}
		break;
		case CSpecialItemGroup::POISON:
		{
			AffectSystem::ApplyPoison(GetEntityHandle(), entt::null);
			bSuccess = true;
		}
		break;
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

			if (ItemSystem::IsValidItem(item_get))
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
	ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "char_item.cpp::bool CHARACTER::DestroyItem(TItemPos Cell),");//INGAME_DEBUG_RAZOR93
#endif
	LPITEM item = nullptr;
	if (!CanHandleItem()) {
#ifdef TEXTS_IMPROVEMENT
		if (DragonSoulSystem::CanRefine(GetEntityHandle())) {
			ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 232, "");
		}
#endif

		return false;
	}

	if (CombatSystem::IsDead(GetEntityHandle()))
		return false;

	if (!InventorySystem::IsValidItemPosition(GetEntityHandle(), Cell) || !(item = GetItem(Cell)))
		return false;

	if (ItemSystem::IsItemEquipped(item->GetEntityHandle()))
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
	uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(GetEntityHandle());
	if (bBattlePassId)
	{
		uint32_t dwItemVnum, dwCnt;
		if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, DESTROY_ITEM, &dwItemVnum, &dwCnt))
		{
			if (dwItemVnum == item->GetVnum() && ecs::PlayerRuntime::GetMissionProgress(GetEntityHandle(), DESTROY_ITEM, bBattlePassId) < dwCnt)
				ecs::PlayerRuntime::UpdateMissionProgress(GetEntityHandle(), DESTROY_ITEM, bBattlePassId, item->GetCount(), dwCnt);
		}
	}
#endif

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 47, "%s", item->GetName());
#endif
	ITEM_MANAGER::instance().RemoveItem(item->GetEntityHandle(), "DESTROY");
	return true;
}

const char CHARACTER::msc_szLastChangeItemAttrFlag[] = "Item.LastChangeItemAttr";
// const char CHARACTER::msc_szChangeItemAttrCycleFlag[] = "change_itemattr_cycle";
// END_OF_CHANGE_ITEM_ATTRIBUTES

const uint8_t g_aBuffOnAttrPoints[] = { POINT_ENERGY, POINT_COSTUME_ATTR_BONUS };

#ifdef ENABLE_PVP_ADVANCED
#endif

using LegacyCharHandle = decltype(std::declval<ecs::LegacyCharPtr>().ptr);



//±ÍÈ¯ºÎ, ±ÍÈ¯±â¾ïºÎ, °áÈ¥¹ÝÁö
bool IS_SUMMONABLE_ZONE(int map_index)
{
	switch (map_index)
	{
	case 66: // »ç±Í�
// ¸¿ö
	case 71: // °�
// ¹Ì ´øÀü 2Ãþ
	case 72: // ÃµÀÇ µ¿±¼
	case 73: // ÃµÀÇ µ¿±¼ 2Ãþ
	case 193: // °�
// ¹Ì ´øÀü 2-1Ãþ
#if 0
	case 184: // ÃµÀÇ µ¿±¼(½�
// ¼ö)
	case 185: // ÃµÀÇ µ¿±¼ 2Ãþ(½�
// ¼ö)
	case 186: // ÃµÀÇ µ¿±¼(ÃµÁ¶)
	case 187: // ÃµÀÇ µ¿±¼ 2Ãþ(ÃµÁ¶)
	case 188: // ÃµÀÇ µ¿±¼(Áø³ë)
	case 189: // ÃµÀÇ µ¿±¼ 2Ãþ(Áø³ë)
#endif
		//		case 206 : // ¾Æ±Íµ¿±¼
	case 216: // ¾Æ±Íµ¿±¼
	case 217: // °�
// ¹Ì ´øÀü 3Ãþ
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

// item socket ÀÌ ÇÁ·Î�
// ä�
// ¸ÀÔ°ú °°ÀºÁö Ã¼�
// © -- by mhh



/////////////////////////////////////////////////////////////////////////////
// ITEM HANDLING
/////////////////////////////////////////////////////////////////////////////

bool CHARACTER::CanHandleItem(bool skipRefine, bool skipObserver)
{
    return InventorySystem::CanHandleItems(GetEntityHandle(), skipRefine, skipObserver);
}

#ifdef ENABLE_EXTRA_INVENTORY
#endif

#ifdef __HIGHLIGHT_SYSTEM__
void CHARACTER::SetItem(TItemPos Cell, entt::entity itemEntity, bool isHighLight)
{
	ecs::PlayerRuntime::SetItem(GetEntityHandle(), Cell, itemEntity, isHighLight);
}
#else
void CHARACTER::SetItem(TItemPos Cell, entt::entity itemEntity)
{
	ecs::PlayerRuntime::SetItem(GetEntityHandle(), Cell, itemEntity);
}
#endif

namespace ecs::PlayerRuntime {

#ifdef __HIGHLIGHT_SYSTEM__
void SetItem(entt::entity e, TItemPos Cell, entt::entity itemEntity, bool isHighLight)
#else
void SetItem(entt::entity e, TItemPos Cell, entt::entity itemEntity)
#endif
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "input_main.cpp:: void CInputMain::RequestLanguage ");//INGAME_DEBUG_RAZOR93
#endif
	uint16_t wCell = Cell.cell;
	uint8_t window_type = Cell.window_type;
	// The 0xff / 0xffffffff pointer sentinel check is gone with the pointer.
	// An entt::entity cannot hold a scribbled pointer value, and an invalid
	// one is caught by IsValidItem below rather than by core_dump().
	const bool hasItem = itemEntity != entt::null && ItemSystem::IsValidItem(itemEntity);
	if (itemEntity != entt::null && !hasItem)
	{
		LOG_ERROR("CHARACTER::SetItem: item entity {} is not a valid item (char: {} cell: {})",
			static_cast<uint32_t>(itemEntity), ecs::PlayerRuntime::GetName(e), wCell);
		return;
	}

	if (hasItem && ItemSystem::GetItemOwner(itemEntity) != entt::null)
	{
		assert(!"GetOwner exist");
		return;
	}
	// ��o� A�oYA丮
	switch (window_type)
	{
	case INVENTORY:
	{
		const uint16_t storageCell = wCell;
		if (storageCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
		{
			LOG_ERROR("CHARACTER::SetItem: invalid item cell {}", storageCell);
			return;
		}

		auto* pMainInventory = EnsureMainInventoryRuntimeComponent(e);
		if (!pMainInventory)
		{
			LOG_ERROR("CHARACTER::SetItem: missing MainInventoryRuntimeComponent");
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
			LOG_ERROR("CHARACTER::SetItem: invalid equipment item cell {}", wCell);
			return;
		}

		auto* pMainInventory = EnsureMainInventoryRuntimeComponent(e);
		if (!pMainInventory)
		{
			LOG_ERROR("CHARACTER::SetItem: missing MainInventoryRuntimeComponent");
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
	// ?�EY1� A�oYA丮
	case DRAGON_SOUL_INVENTORY:
	{
		if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			LOG_ERROR("CHARACTER::SetItem: invalid DS item cell {}", wCell);
			return;
		}

		auto* pDragonSoulInventory = EnsureDragonSoulInventoryComponent(e);
		if (!pDragonSoulInventory)
		{
			LOG_ERROR("CHARACTER::SetItem: missing DragonSoulInventoryComponent");
			return;
		}

		const entt::entity pOld = pDragonSoulInventory->items[wCell];

		if (pOld != entt::null)
		{
			for (int i = 0; i < ItemSystem::GetItemSize(pOld); ++i)
			{
				int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

				if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
					continue;

				if (pDragonSoulInventory->items[p] != entt::null && pDragonSoulInventory->items[p] != pOld)
					continue;

				pDragonSoulInventory->itemGrid[p] = 0;
			}
		}

		if (hasItem)
		{
			for (int i = 0; i < ItemSystem::GetItemSize(itemEntity); ++i)
			{
				int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

				if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
					continue;

				pDragonSoulInventory->itemGrid[p] = wCell + 1;
			}
		}

		pDragonSoulInventory->items[wCell] = itemEntity;
	}
	break;
#ifdef ENABLE_EXTRA_INVENTORY
	case EXTRA_INVENTORY:
	{
		if (wCell >= EXTRA_INVENTORY_MAX_NUM)
		{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "char_item.cpp::if (wCell >= EXTRA_INVENTORY_MAX_NUM)");//INGAME_DEBUG_RAZOR93
#endif
			LOG_ERROR("CHARACTER::SetItem: invalid EXTRA item cell {}", wCell);
			return;
		}

		auto* pExtraInventory = EnsureExtraInventoryRuntimeComponent(e);
		if (!pExtraInventory)
		{
			LOG_ERROR("CHARACTER::SetItem: missing ExtraInventoryRuntimeComponent");
			return;
		}

		const entt::entity pOld = pExtraInventory->items[wCell];

		if (pOld != entt::null)
		{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "char_item.cpp::if (pOld != entt::null)");//INGAME_DEBUG_RAZOR93
#endif

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
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "char_item.cpp::if (hasItem)");//INGAME_DEBUG_RAZOR93
#endif
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
			LOG_ERROR("CHARACTER::SetItem: invalid switchbot item cell {}", wCell);
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
		// E�Aa 3AAIAU: 1�1�?!1� 3AAIAU �A�!�� ��o��� o�31�U
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
			pack.highlight = (Cell.window_type == DRAGON_SOUL_INVENTORY);
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
		case DRAGON_SOUL_INVENTORY:
			ItemSystem::SetItemWindow(itemEntity, DRAGON_SOUL_INVENTORY);
			break;
#ifdef ENABLE_EXTRA_INVENTORY
		case EXTRA_INVENTORY:
			ItemSystem::SetItemWindow(itemEntity, EXTRA_INVENTORY);
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
			LOG_INFO("Razor93 LOG:: Called: Char_item.cpp line :653: case switch :ItemSystem::SetItemWindow(itemEntity, EXTRA_INVENTORY);");
#endif
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
void CHARACTER::ClearItem()
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_INFO, "char_item.cpp:: void CHARACTER::ClearItem ");//INGAME_DEBUG_RAZOR93
#endif
	int		i;
	LPITEM	item;

	for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
	{
		if ((item = GetInventoryItem(i)))
		{
			ItemSystem::SetItemSkipSave(item->GetEntityHandle(), true);
			ITEM_MANAGER::instance().FlushDelayedSave(item->GetEntityHandle());

			InventorySystem::RemoveFromCharacter(item->GetEntityHandle());
			ItemSystem::DestroyItemEntityEcs(
				(item ? item->GetEntityHandle() : entt::null),
				"CLEAR_ITEM_INVENTORY");

			SyncQuickslot(QUICKSLOT_TYPE_ITEM, i, 255);
		}
	}
	for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
	{
		if ((item = GetItem(TItemPos(DRAGON_SOUL_INVENTORY, i))))
		{
			ItemSystem::SetItemSkipSave(item->GetEntityHandle(), true);
			ITEM_MANAGER::instance().FlushDelayedSave(item->GetEntityHandle());

			InventorySystem::RemoveFromCharacter(item->GetEntityHandle());
			ItemSystem::DestroyItemEntityEcs(
				(item ? item->GetEntityHandle() : entt::null),
				"CLEAR_ITEM_DRAGON_SOUL");
		}
	}

#ifdef ENABLE_EXTRA_INVENTORY
	for (i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)
	{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
		LOG_INFO("Razor93 LOG:: Called: Char_item.cpp line :739: for (i = 0; i < EXTRA_INVENTORY_MAX_NUM; ++i)");
#endif
		if ((item = GetExtraInventoryItem(i)))
		{
			ItemSystem::SetItemSkipSave(item->GetEntityHandle(), true);
			ITEM_MANAGER::instance().FlushDelayedSave(item->GetEntityHandle());

			InventorySystem::RemoveFromCharacter(item->GetEntityHandle());
			ItemSystem::DestroyItemEntityEcs(
				(item ? item->GetEntityHandle() : entt::null),
				"CLEAR_ITEM_EXTRA_INVENTORY");

			SyncQuickslot(QUICKSLOT_TYPE_ITEM_EXTRA, i, 255);
		}
	}
#endif

#ifdef ENABLE_SWITCHBOT
	for (i = 0; i < SWITCHBOT_SLOT_COUNT; ++i)
	{
		if ((item = GetItem(TItemPos(SWITCHBOT, i))))
		{
			ItemSystem::SetItemSkipSave(item->GetEntityHandle(), true);
			ITEM_MANAGER::instance().FlushDelayedSave(item->GetEntityHandle());

			InventorySystem::RemoveFromCharacter(item->GetEntityHandle());
			ItemSystem::DestroyItemEntityEcs(
				(item ? item->GetEntityHandle() : entt::null),
				"CLEAR_ITEM_SWITCHBOT");
		}
	}
#endif
}


bool CHARACTER::IsEmptyItemGrid(TItemPos cell, uint8_t size, int exceptionCell) const
{
    return InventorySystem::IsEmptyItemGrid(GetEntityHandle(), cell, size, exceptionCell);
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
		iUnlock = ecs::PlayerRuntime::GetQuestFlag(GetEntityHandle(), "lock_extra.cat1") * 5;
		break;
	}
	case 1: {
		iUnlock = ecs::PlayerRuntime::GetQuestFlag(GetEntityHandle(), "lock_extra.cat2") * 5;
		break;
	}
	case 2: {
		iUnlock = ecs::PlayerRuntime::GetQuestFlag(GetEntityHandle(), "lock_extra.cat3") * 5;
		break;
	}
	case 3: {
		iUnlock = ecs::PlayerRuntime::GetQuestFlag(GetEntityHandle(), "lock_extra.cat4") * 5;
		break;
	}
	case 4: {
		iUnlock = ecs::PlayerRuntime::GetQuestFlag(GetEntityHandle(), "lock_extra.cat5") * 5;
		break;
	}
	case 5: {
		iUnlock = ecs::PlayerRuntime::GetQuestFlag(GetEntityHandle(), "lock_extra.cat6") * 5;
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
		ecs::ChatSystem::SendNew(GetEntityHandle(), CHAT_TYPE_INFO, 234, "%d", time);
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

	uint8_t stage = ecs::PlayerRuntime::GetQuestFlag(GetEntityHandle(), stageName.c_str());
	if (stage < 0 || stage >= 14)
		return;

	int needKeys = NeedKeysForExtraInventory[stage];
	if (CountSpecifyItem(72320) >= needKeys) {
		RemoveSpecifyItem(72320, needKeys);

		ecs::PlayerRuntime::SetQuestFlag(GetEntityHandle(), stageName.c_str(), stage + 1);
		PointChange(POINT_EXTRA_INVENTORY1 + category, stage + 1);
		ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, "RefreshExpandInventory");
#ifdef ENABLE_SPAM_CHECK
		SetLastUnlock();
#endif
	}
	else {
		ecs::ChatSystem::Send(GetEntityHandle(), CHAT_TYPE_COMMAND, "update_envanter_need %d", needKeys - CountSpecifyItem(72320));
	}
}
#endif

#ifdef ENABLE_EXTRA_INVENTORY
int CHARACTER::GetEmptyExtraInventory(LPITEM pItem) const
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	LOG_INFO("Razor93 LOG:: Called: Char_item.cpp  CHARACTER::GetEmptyExtraInventory(LPITEM pItem) const");
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
	uint16_t wBaseCell = DSManager::instance().GetBasePosition((pItem ? pItem->GetEntityHandle() : entt::null));

	if (WORD_MAX == wBaseCell)
		return -1;

	for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; ++i)
		if (IsEmptyItemGrid(TItemPos(DRAGON_SOUL_INVENTORY, i + wBaseCell), bSize))
			return i + wBaseCell;

	return -1;
}

void CHARACTER::BuffOnAttr_AddBuffsFromItem(LPITEM pItem)
{
	ecs::PlayerRuntime::BuffOnAttr_AddBuffsFromItem(
		GetEntityHandle(), pItem ? pItem->GetEntityHandle() : entt::null);
}

void CHARACTER::BuffOnAttr_RemoveBuffsFromItem(LPITEM pItem)
{
	ecs::PlayerRuntime::BuffOnAttr_RemoveBuffsFromItem(
		GetEntityHandle(), pItem ? pItem->GetEntityHandle() : entt::null);
}

namespace ecs::PlayerRuntime {

// The buff pools live in ecs::BuffOnAttrs. Reads tolerate a missing component -
// a character that never triggered a buff simply has none - while ValueChange
// creates it on the first pool it needs.
void BuffOnAttr_AddBuffsFromItem(entt::entity e, entt::entity item)
{
	auto* buffs = g_registry.try_get<ecs::BuffOnAttrs>(e);
	if (!buffs)
		return;

	for (size_t i = 0; i < _countof(g_aBuffOnAttrPoints); i++)
	{
		auto it = buffs->pools.find(g_aBuffOnAttrPoints[i]);
		if (it != buffs->pools.end() && it->second)
			it->second->AddBuffFromItem(item);
	}
}

void BuffOnAttr_RemoveBuffsFromItem(entt::entity e, entt::entity item)
{
	auto* buffs = g_registry.try_get<ecs::BuffOnAttrs>(e);
	if (!buffs)
		return;

	for (size_t i = 0; i < _countof(g_aBuffOnAttrPoints); i++)
	{
		auto it = buffs->pools.find(g_aBuffOnAttrPoints[i]);
		if (it != buffs->pools.end() && it->second)
			it->second->RemoveBuffFromItem(item);
	}
}

void BuffOnAttr_ClearAll(entt::entity e)
{
	auto* buffs = g_registry.try_get<ecs::BuffOnAttrs>(e);
	if (!buffs)
		return;

	for (auto& entry : buffs->pools)
	{
		if (entry.second)
			entry.second->Initialize();
	}
}

void BuffOnAttr_Destroy(entt::entity e)
{
	auto* buffs = g_registry.try_get<ecs::BuffOnAttrs>(e);
	if (!buffs)
		return;

	for (auto& entry : buffs->pools)
		M2_DELETE(entry.second);

	buffs->pools.clear();
}

void BuffOnAttr_ValueChange(entt::entity e, uint8_t bType, uint8_t bOldValue, uint8_t bNewValue)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	auto& buffs = g_registry.get_or_emplace<ecs::BuffOnAttrs>(e);
	auto it = buffs.pools.find(bType);

	if (0 == bNewValue)
	{
		if (buffs.pools.end() == it)
			return;
		else
			it->second->Off();
	}
	else if (0 == bOldValue)
	{
		CBuffOnAttributes* pBuff = nullptr;
		if (buffs.pools.end() == it)
		{
			switch (bType)
			{
			case POINT_ENERGY:
			{
				static uint8_t abSlot[] = { WEAR_BODY, WEAR_HEAD, WEAR_FOOTS, WEAR_WRIST, WEAR_WEAPON, WEAR_NECK, WEAR_EAR, WEAR_SHIELD };
				static std::vector <uint8_t> vec_slots(abSlot, abSlot + _countof(abSlot));
				pBuff = M2_NEW CBuffOnAttributes(e, bType, &vec_slots);
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
				pBuff = M2_NEW CBuffOnAttributes(e, bType, &vec_slots);
			}
			break;
			default:
				break;
			}
			buffs.pools.insert(std::make_pair(bType, pBuff));
		}
		else
			pBuff = it->second;
		if (pBuff != nullptr)
			pBuff->On(bNewValue);
	}
	else
	{
		assert(buffs.pools.end() != it);
		it->second->ChangeBuffValue(bNewValue);
	}
}

} // namespace ecs::PlayerRuntime


// CHECK_UNIQUE_GROUP
// END_OF_CHECK_UNIQUE_GROUP

// NEW_HAIR_STYLE_ADD
// END_NEW_HAIR_STYLE_ADD

bool CHARACTER::CanDoCube() const
{
	if (m_bIsObserver)	return false;
	if (ecs::SocialSystem::GetShop(GetEntityHandle()))		return false;
	if (ecs::SocialSystem::GetMyShop(GetEntityHandle()))	return false;
	if (InventorySystem::IsRefining(GetEntityHandle()))	return false;
	if (ecs::PlayerRuntime::IsWarping(GetEntityHandle()))	return false;

	return true;
}

#ifdef ENABLE_RECALL
void CHARACTER::AutoRecallProcess()
{
	if (!IsPC())
		return;

#ifdef __PET_SYSTEM__
	{
		const CAffect* pAffect = AffectSystem::FindAffect(GetEntityHandle(), AFFECT_RECALL1);
		if (pAffect) {
			const entt::entity pItem = ItemSystem::FindItemByID(GetEntityHandle(), pAffect->dwFlag);
			if (pItem != entt::null) {
				if (ItemSystem::GetItemSocket(pItem, 2) == false) {
					CPetSystem* petSystem = GetPetSystem();
					if (petSystem) {
						if (petSystem->CountSummoned() < 1) {
							CPetActor* pPet = petSystem->Summon(ItemSystem::GetItemValue(pItem, 1), pItem, "", false);
							if (!pPet)
								AffectSystem::RemoveAffect(GetEntityHandle(), const_cast<CAffect*>(pAffect));
						}
					}
					else
						AffectSystem::RemoveAffect(GetEntityHandle(), const_cast<CAffect*>(pAffect));
				}
			}
			else
				AffectSystem::RemoveAffect(GetEntityHandle(), const_cast<CAffect*>(pAffect));
		}
	}
#endif
#ifdef __NEWPET_SYSTEM__
	{
		const CAffect* pAffect = AffectSystem::FindAffect(GetEntityHandle(), AFFECT_RECALL2);
		if (pAffect) {
			const entt::entity pItem = ItemSystem::FindItemByID(GetEntityHandle(), pAffect->dwFlag);
			if (pItem != entt::null) {
				if (ItemSystem::GetItemSocket(pItem, 0) == false) {
					CNewPetSystem* petSystem = GetNewPetSystem();
					if (petSystem) {
						if (petSystem->CountSummoned() < 1) {
							CNewPetActor* pPet = petSystem->Summon(ItemSystem::GetItemValue(pItem, 0), pItem, "", false);
							if (!pPet)
								AffectSystem::RemoveAffect(GetEntityHandle(), const_cast<CAffect*>(pAffect));
						}
					}
					else
						AffectSystem::RemoveAffect(GetEntityHandle(), const_cast<CAffect*>(pAffect));
				}
			}
			else
				AffectSystem::RemoveAffect(GetEntityHandle(), const_cast<CAffect*>(pAffect));
		}
	}
#endif
}
#endif

/// ÇöÀç Ä³¸¯�
// ÍÀÇ »ó�
// Â¸¦ ¹Ù�
// ÁÀ¸·Î ÁÖ¾îÁø itemÀ» Âø¿ëÇÒ ¼ö ÀÖ´Â Áö È®ÀÎÇÏ°í, ºÒ°¡´É ÇÏ´Ù¸é Ä³¸¯�
// Í¿¡°Ô ÀÌÀ¯¸¦ ¾Ë·ÁÁÖ´Â ÇÔ¼ö
#

/// ÇöÀç Ä³¸¯�
// ÍÀÇ »ó�
// Â¸¦ ¹Ù�
// ÁÀ¸·Î Âø¿ë ÁßÀÎ itemÀ» ¹þÀ» ¼ö ÀÖ´Â Áö È®ÀÎÇÏ°í, ºÒ°¡´É ÇÏ´Ù¸é Ä³¸¯�
// Í¿¡°Ô ÀÌÀ¯¸¦ ¾Ë·ÁÁÖ´Â ÇÔ¼ö
void CItem::Initialize()
{
	CEntity::Initialize(ENTITY_ITEM);
	SetEntityHandle(entt::null);

	m_dwID = 0;
	m_dwVID = m_lFlag = 0;
	m_pProto = nullptr;

}

void CItem::Destroy()
{
	CEntity::Destroy();

	if (GetSectree())
		GetSectree()->RemoveEntity(this);
}

void CItem::Save()
{
	ItemSystem::SaveItem(GetEntityHandle());
}

void CItem::SetProto(const TItemTable* table)
{
	assert(table != NULL);
	m_pProto = table;
	SetFlag(m_pProto->dwFlags);
}

#ifdef ENABLE_ITEM_EXTRA_PROTO
TItemExtraProto* CItem::GetExtraProto()
{
	return ItemSystem::GetItemExtraProto(GetEntityHandle());
}
#endif


#ifdef ENABLE_RUNE_SYSTEM
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
		ItemSystem::SetItemForceAttributeEcs(GetEntityHandle(), i, GetAttributeType(i), lValue);
	}

	if (isActive)
		ModifyPoints(true);

	UpdatePacket();
}

void CItem::ActivateRuneBonus() {
	const entt::entity pOwner = GetOwnerEntity();
	if (pOwner == entt::null)
		return;

	LPITEM pkItem1 = LegacyItemBoundary(ItemSystem::GetWearItem(pOwner, WEAR_RUNE7));
	if (!pkItem1)
		return;

	if (pkItem1->GetSocket(1) == 1)
		return;

	bool bCan = true;
	int iMaxSubTypes = RUNE_SUBTYPES - 1;
	LPITEM pkItem2 = nullptr;
	for (int i = 0; i < iMaxSubTypes; i++) {
		pkItem2 = LegacyItemBoundary(ItemSystem::GetWearItem(pOwner, WEAR_RUNE1 + i));
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
		if (AffectSystem::FindAffect(pOwner, AFFECT_RUNE2))
			AffectSystem::RemoveAffect(pOwner, AFFECT_RUNE2);

		if (!AffectSystem::FindAffect(pOwner, AFFECT_RUNE1))
			AffectSystem::AddAffect(pOwner, AFFECT_RUNE1, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, false, false);

		return;
	}
	else {
		if (AffectSystem::FindAffect(pOwner, AFFECT_RUNE1))
			AffectSystem::RemoveAffect(pOwner, AFFECT_RUNE1);

		if (!AffectSystem::FindAffect(pOwner, AFFECT_RUNE2))
			AffectSystem::AddAffect(pOwner, AFFECT_RUNE2, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, false, false);
	}

	ItemSystem::SetItemSocketEcs((pkItem1 ? pkItem1->GetEntityHandle() : entt::null), 1, 1);
	ItemSystem::ModifyPoints(pkItem1->GetEntityHandle(), true);
	pkItem1->UpdatePacket();
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(pOwner, CHAT_TYPE_INFO, 31, "%s", pkItem1->GetName());
#endif
}

void CItem::DeactivateRuneBonus() {
	const entt::entity pOwner = GetOwnerEntity();
	if (pOwner == entt::null)
		return;

	LPITEM pkItem1 = LegacyItemBoundary(ItemSystem::GetWearItem(pOwner, WEAR_RUNE7));
	if (!pkItem1)
		return;

	if (pkItem1->GetSocket(1) != 1)
		return;

	if (AffectSystem::FindAffect(pOwner, AFFECT_RUNE2))
		AffectSystem::RemoveAffect(pOwner, AFFECT_RUNE2);

	ItemSystem::SetItemSocketEcs((pkItem1 ? pkItem1->GetEntityHandle() : entt::null), 1, 0);
	ItemSystem::ModifyPoints(pkItem1->GetEntityHandle(), false);
	pkItem1->UpdatePacket();
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(pOwner, CHAT_TYPE_INFO, 901, "%s", pkItem1->GetName());
#endif
}

void CItem::DeactivateRuneBonusRefresh() {
	const entt::entity pOwner = GetOwnerEntity();
	int iMaxSubTypes = RUNE_SUBTYPES - 1;
	bool bAdd = false;
	LPITEM pkItem2 = nullptr;
	if (!AffectSystem::FindAffect(pOwner, AFFECT_RUNE1)) {
		for (int i = 0; i < iMaxSubTypes; i++) {
			pkItem2 = LegacyItemBoundary(ItemSystem::GetWearItem(pOwner, WEAR_RUNE1 + i));
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
			AffectSystem::AddAffect(pOwner, AFFECT_RUNE1, APPLY_NONE, 0, 0, INFINITE_AFFECT_DURATION, false, false);
	}
	else {
		for (int i = 0; i < iMaxSubTypes; i++) {
			pkItem2 = LegacyItemBoundary(ItemSystem::GetWearItem(pOwner, WEAR_RUNE1 + i));
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
			AffectSystem::RemoveAffect(pOwner, AFFECT_RUNE1);
	}
}

void CItem::ActivateRune() {
	const entt::entity pOwner = GetOwnerEntity();
	if (!IsRune())
		return;

	if (GetSocket(1) == 1)
		return;

	if (GetSocket(ITEM_SOCKET_REMAIN_SEC) <= 0) {
#ifdef TEXTS_IMPROVEMENT
		if (pOwner != entt::null) {
			ecs::ChatSystem::SendNew(pOwner, CHAT_TYPE_INFO, 30, "%s", GetName());
		}
#endif
		return;
	}

	SetSocket(1, 1);
	ModifyPoints(true);
	UpdatePacket();
#ifdef TEXTS_IMPROVEMENT
	if (pOwner != entt::null) {
		ecs::ChatSystem::SendNew(pOwner, CHAT_TYPE_INFO, 31, "%s", GetName());
	}
#endif

	ActivateRuneBonus();
}

void CItem::DeactivateRune() {
	if (!IsRune())
		return;

	if (GetSocket(1) == 0)
		return;

	const entt::entity pOwner = GetOwnerEntity();
	DeactivateRuneBonus();

	SetSocket(1, 0);
	ModifyPoints(false);
	UpdatePacket();
	DeactivateRuneBonusRefresh();
#ifdef TEXTS_IMPROVEMENT
	if (pOwner != entt::null) {
		ecs::ChatSystem::SendNew(pOwner, CHAT_TYPE_INFO, 32, "%s", GetName());
	}
#endif
}
#endif

void CItem::SetAccessorySocketGrade(int iGrade
#ifdef ENABLE_INFINITE_RAFINES
	, bool infinite
#endif
)
{
	ItemSystem::SetItemAccessorySocketGrade(GetEntityHandle(), iGrade
#ifdef ENABLE_INFINITE_RAFINES
		, infinite
#endif
	);
}

int CItem::GetAccessorySocketGrade()
{
	return ItemSystem::GetItemAccessorySocketGrade(GetEntityHandle());
}

int CItem::GetAccessorySocketMaxGrade()
{
	return ItemSystem::GetItemAccessorySocketMaxGrade(GetEntityHandle());
}

uint32_t CItem::GetSIGVnum() const
{
    return ItemSystem::GetItemSIGVnum(GetEntityHandle());
}

namespace ItemSystem {
const char* GetItemName(entt::entity item, uint8_t language)
{
	LPITEM legacy = LegacyItemBoundary(item);
	return legacy ? legacy->GetName(language) : "";
}
} // namespace ItemSystem


namespace ItemSystem {

int GiveMoreTime_Per(entt::entity item, float fPercent)
{
	if (ItemSystem::IsDragonSoulItem(item))
	{
		uint32_t duration = DSManager::instance().GetDuration(item);
		uint32_t remain_sec = ItemSystem::GetItemSocket(item, ITEM_SOCKET_REMAIN_SEC);
		uint32_t given_time = fPercent * duration / 100u;
		if (remain_sec == duration)
			return false;
		if ((given_time + remain_sec) >= duration)
		{
			ItemSystem::SetItemSocket(item, ITEM_SOCKET_REMAIN_SEC, duration);
			return duration - remain_sec;
		}
		else
		{
			ItemSystem::SetItemSocket(item, ITEM_SOCKET_REMAIN_SEC, given_time + remain_sec);
			return given_time;
		}
	}
	// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
	else
		return 0;
}

int GiveMoreTime_Fix(entt::entity item, uint32_t dwTime)
{
	if (ItemSystem::IsDragonSoulItem(item))
	{
		uint32_t duration = DSManager::instance().GetDuration(item);
		uint32_t remain_sec = ItemSystem::GetItemSocket(item, ITEM_SOCKET_REMAIN_SEC);
		if (remain_sec == duration)
			return false;
		if ((dwTime + remain_sec) >= duration)
		{
			ItemSystem::SetItemSocket(item, ITEM_SOCKET_REMAIN_SEC, duration);
			return duration - remain_sec;
		}
		else
		{
			ItemSystem::SetItemSocket(item, ITEM_SOCKET_REMAIN_SEC, dwTime + remain_sec);
			return dwTime;
		}
	}
	// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
	else
		return 0;
}

} // namespace ItemSystem

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
			LOG_ERROR("refine_level_based_on_NAME({}) is not equal to refine_level_based_on_LOCALE_NAME({}).", rtn, locale_rtn);
		}
	}

	return rtn;
}

void CItem::SetLockedAttr(short sIndex)
{
	if (GetEntityHandle() != entt::null && g_registry.valid(GetEntityHandle()))
		g_registry.get_or_emplace<ecs::ItemLockedAttribute>(GetEntityHandle()).index = sIndex;
	if (const entt::entity itemEntity = GetEntityHandle();
		itemEntity != entt::null && g_registry.valid(itemEntity))
		g_registry.emplace_or_replace<ecs::ItemLockedAttribute>(itemEntity, ecs::ItemLockedAttribute{sIndex});
	UpdatePacket();
	Save();
}

void CItem::SetExchanging(bool bOn)
{
	if (GetEntityHandle() != entt::null && g_registry.valid(GetEntityHandle()))
		g_registry.get_or_emplace<ecs::ItemFlags>(GetEntityHandle()).exchanging = bOn;
}


int32_t CItem::GetRuneAttrType(int index) {
    return ItemSystem::GetRuneAttributeType(GetEntityHandle(), index);
}

int32_t CItem::GetRuneAttrValue(int index, int32_t time) {
    return ItemSystem::GetRuneAttributeValue(GetEntityHandle(), index, time);
}


CItem::CItem(uint32_t dwVnum)
	: m_pProto(nullptr), m_dwVnum(dwVnum), m_dwID(0), m_dwVID(0),
	m_lFlag(0),
	m_dwMaskVnum(0)
{
}

CItem::~CItem()
{
	Destroy();
}


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

	int remain_time = static_cast<int>(ItemSystem::GetItemSocket(itemEntity, ITEM_SOCKET_REMAIN_SEC))
		- processing_time / passes_per_sec;
#ifdef ENABLE_RUNE_SYSTEM
	if (ItemSystem::IsRuneItem(itemEntity)) {
		if (remain_time <= 0) {
			ItemSystem::SetItemSocket(itemEntity, ITEM_SOCKET_REMAIN_SEC, 0);
			ItemSystem::DeactivateRuneLegacyBoundary(itemEntity);
			return 0;
		}

		const int runeStep = ItemSystem::GetItemValue(itemEntity, 0) / 100;
		if (runeStep > 0 && remain_time / runeStep < 50)
			ItemSystem::DeactivateRuneBonusLegacyBoundary(itemEntity);

		if (ItemSystem::GetItemSubType(itemEntity) == RUNE_SLOT7 ||
			ItemSystem::GetItemSocket(itemEntity, 1) != 1)
			return PASSES_PER_SEC(MIN(60, remain_time));

		if (ItemSystem::GetItemSocket(itemEntity, 1) == 1)
			ItemSystem::ChangeRuneAttributesLegacyBoundary(itemEntity, remain_time);
	}
#endif

	if (remain_time <= 0)
	{
		LOG_INFO("ITEM EXPIRED : expired {} {}", ItemSystem::GetItemName(itemEntity),
			ItemSystem::GetItemID(itemEntity));
		ItemSystem::GetItemEvents(itemEntity).timerBasedOnWearExpire = nullptr;
		ItemSystem::SetItemSocket(itemEntity, ITEM_SOCKET_REMAIN_SEC, 0);

		if (ItemSystem::IsDragonSoulItem(itemEntity))
		{
			DSManager::instance().DeactivateDragonSoul(itemEntity);
		}
		else
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
		const entt::entity pkOwner = ItemSystem::GetItemOwnerEntity(item);

		if (pkOwner != entt::null && ecs::PlayerRuntime::GetDesc(pkOwner) && ItemSystem::GetItemWindow(item) == MOUNT_INVENTORY)
		{
			TPacketGCWhisper pack;
			char msg[CHAT_MAX_LEN + 1];

			const int len = snprintf(msg, sizeof(msg), "Mount expired in mountinventory: %s", ItemSystem::GetItemName(item));

			pack.bHeader = HEADER_GC_WHISPER;
			pack.bType = WHISPER_TYPE_SYSTEM;
			pack.wSize = static_cast<uint16_t>(sizeof(TPacketGCWhisper) + len + 1);
			strlcpy(pack.szNameFrom, "[MountInventory]", sizeof(pack.szNameFrom));

			ecs::PlayerRuntime::GetDesc(pkOwner)->BufferedPacket(&pack, sizeof(pack));
			ecs::PlayerRuntime::GetDesc(pkOwner)->Packet(msg, len + 1);
		}

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

namespace ItemSystem {

// What this item refines back down to when a scroll fails.
uint32_t GetItemRefineFromVnum(entt::entity item)
{
	return ITEM_MANAGER::instance().GetRefineFromVnum(GetItemVnum(item));
}

// The refine itself, with both sides as entities.
bool DoRefine(entt::entity e, entt::entity item, bool bMoneyOnly)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "char_item.cpp:: bool CHARACTER::DoRefine ");
#endif
	if (!InventorySystem::CanHandleItems(e, true))
	{
		InventorySystem::ClearRefineMode(e);
		return false;
	}

	//°³·® ½Ã°£Á¦ÇÑ : upgrade_refine_scroll.quest ¿¡¼­ °³·®ÈÄ 5ºÐÀÌ³»¿¡ ÀÏ¹Ý °³·®À»
	//ÁøÇàÇÒ¼ö ¾øÀ½
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			LOG_INFO("can't refine {} {}", ecs::PlayerRuntime::GetPlayerID(e), ecs::PlayerRuntime::GetName(e).data());
			return false;
		}
	}

	const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(GetItemRefineSet(item));

	if (!prt)
		return false;

	uint32_t result_vnum = GetItemRefinedVnum(item);
	int64_t cost = InventorySystem::ComputeRefineFee(e, prt->cost);

	if (result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	if (GetItemType(item) == ITEM_USE && GetItemSubType(item) == USE_TUNING)
		return false;

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(GetItemRefinedVnum(item));

	if (!pProto)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	// REFINE_COST
	if (ecs::PointSystem::GetGold(e) < cost)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 232, "");
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset_percent(e);
#endif
#endif
		return false;
	}

	if (!bMoneyOnly)
	{
		for (int i = 0; i < prt->material_count; ++i)
		{
			if (CountItem(e, prt->materials[i].vnum) < prt->materials[i].count)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 233, "");
#endif
				return false;
			}
		}

		for (int i = 0; i < prt->material_count; ++i)
			RemoveSpecifyItemEcs(e, prt->materials[i].vnum, prt->materials[i].count);
	}

	int prob = number(1, 100);


#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	if (ecs::SocialSystem::IsRefineThroughGuild(e) || bMoneyOnly)
	{
		prob -= 10;
	}

	int success_prob = prt->prob;
	success_prob += CRefineManager::instance().Result(e);
#else
	if (ecs::SocialSystem::IsRefineThroughGuild(e) || bMoneyOnly)
		prob -= 10;

#endif
	// END_OF_REFINE_COST
#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	if (prob <= success_prob)
#else
	if (prob <= prt->prob)
#endif
	{
		// ¼º°ø! ¸ðµç ¾ÆÀÌ�
// ÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ´Ù¸¥ ¾ÆÀÌ�
// Û È¹µæ
		const entt::entity pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (IsValidItem(pkNewItem))
		{
			CopyAllAttrToEcs(item, pkNewItem);
			LogManager::instance().ItemLogEntity(e, pkNewItem, "REFINE SUCCESS", GetItemName(pkNewItem));

			uint8_t bCell = GetItemCell(item);


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(e);
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == GetItemVnum(item) && ecs::PlayerRuntime::GetMissionProgress(e, REFINE_ITEM, bBattlePassId) < dwCount)
						ecs::PlayerRuntime::UpdateMissionProgress(e, REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			// DETAIL_REFINE_LOG
			NotifyRefineSuccess(e, item, ecs::SocialSystem::IsRefineThroughGuild(e) ? "GUILD" : "POWER");
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, GetItemVnum(item), -cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");
			// END_OF_DETAIL_REFINE_LOG

			InventorySystem::AddToCharacter(pkNewItem, e, TItemPos(INVENTORY, bCell));
			FlushDelayedSaveEcs(pkNewItem);

			LOG_INFO("Refine Success {}", (long long)cost);
			AttrLog(pkNewItem);
			//ecs::PointSystem::Change(e, POINT_GOLD, -cost);
			LOG_INFO("PayPee {}", (long long)cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(e);
#endif
			InventorySystem::PayRefineFee(e, cost);
			LOG_INFO("PayPee End {}", cost);
		}
		else
		{
			// DETAIL_REFINE_LOG
			// ¾ÆÀÌ�
// Û »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			LOG_ERROR("cannot create item {}", result_vnum);
			NotifyRefineFail(e, item, ecs::SocialSystem::IsRefineThroughGuild(e) ? "GUILD" : "POWER");
			// END_OF_DETAIL_REFINE_LOG
		}
	}
	else
	{
		// ½ÇÆÐ! ¸ðµç ¾ÆÀÌ�
// ÛÀÌ »ç¶óÁü.
		DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, GetItemVnum(item), -cost);
		NotifyRefineFail(e, item, ecs::SocialSystem::IsRefineThroughGuild(e) ? "GUILD" : "POWER");
		AttrLog(item);
		ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

		//ecs::PointSystem::Change(e, POINT_GOLD, -cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset(e);
#endif
		InventorySystem::PayRefineFee(e, cost);
	}

	// Both paths above normally consume the item, so this finds nothing
	// to copy. The one path that keeps it is the failed creation above,
	// and there its components are re-read - which is what the old
	// entry point did on every successful return.
	SyncItemStateFromLegacy(item);
	return true;
}

#ifdef ENABLE_MUSIN_SCROLL_REFINE_100_SUCCESS_RAZOR93
// Refining with a scroll rather than materials.
bool DoRefineWithScroll(entt::entity e, entt::entity item)
{
	
	//if (item && IsRefineBlockedVnum(GetItemVnum(item)))
	//{
	//	ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Ezt a targyat nem lehet fejleszteni.");
	//	InventorySystem::ClearRefineMode(e);
	//	return false;
	//}

	if (!InventorySystem::CanHandleItems(e, true))
	{
		InventorySystem::ClearRefineMode(e);
		return false;
	}

	InventorySystem::ClearRefineMode(e);

	//°³·® ½Ã°£Á¦ÇÑ : upgrade_refine_scroll.quest ¿¡¼­ °³·®ÈÄ 5ºÐÀÌ³»¿¡ ÀÏ¹Ý °³·®À»
		//ÁøÇàÇÒ¼ö ¾øÀ½
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			LOG_INFO("can't refine {} {}", ecs::PlayerRuntime::GetPlayerID(e), ecs::PlayerRuntime::GetName(e).data());
			return false;
		}
	}

	const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(GetItemRefineSet(item));

	if (!prt)
		return false;


	// °³·®¼­ Ã¼�
// ©
	if (InventorySystem::GetRefineScrollCell(e) < 0)
		return false;

	const entt::entity pkItemScroll = GetInventoryItem(e, InventorySystem::GetRefineScrollCell(e));

	if (pkItemScroll == entt::null)
		return false;

	if (!(GetItemType(pkItemScroll) == ITEM_USE && GetItemSubType(pkItemScroll) == USE_TUNING))
		return false;

	if (GetItemVnum(pkItemScroll) == GetItemVnum(item))
		return false;

	uint32_t result_vnum = GetItemRefinedVnum(item);
	uint32_t result_fail_vnum = GetItemRefineFromVnum(item);

	if (result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	// MUSIN_SCROLL
	if (GetItemValue(pkItemScroll, 0) == MUSIN_SCROLL)
	{
		
		//if (GetItemRefineLevel(item) >= 4)
		//{
		//	ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "MAX +9 with this scroll!");
		//	return false;
		//}
	}
	// END_OF_MUSIC_SCROLL

	else if (GetItemValue(pkItemScroll, 0) == MEMO_SCROLL)
	{
		if (GetItemRefineLevel(item) != GetItemValue(pkItemScroll, 1))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 417, "%s#%s", GetItemName(item), GetItemName(pkItemScroll));
#endif
			return false;
		}
	}
	else if (GetItemValue(pkItemScroll, 0) == BDRAGON_SCROLL)
	{
		if (GetItemType(item) != ITEM_METIN || GetItemRefineLevel(item) != 4)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 665, "%s#%s", GetItemName(item), GetItemName(pkItemScroll));
#endif
			return false;
		}
	}

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(GetItemRefinedVnum(item));

	if (!pProto)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	if (ecs::PointSystem::GetGold(e) < prt->cost)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 232, "");
#endif
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset_percent(e);
#endif
		return false;
	}

	for (int i = 0; i < prt->material_count; ++i)
	{
		if (CountItem(e, prt->materials[i].vnum) < prt->materials[i].count)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 233, "");
#endif
			return false;
		}
	}

	for (int i = 0; i < prt->material_count; ++i)
		RemoveSpecifyItemEcs(e, prt->materials[i].vnum, prt->materials[i].count);

	int prob = number(1, 100);
	int success_prob = prt->prob;
	bool bDestroyWhenFail = false;

	const char* szRefineType = "SCROLL";

	if (GetItemValue(pkItemScroll, 0) == HYUNIRON_CHN ||
		GetItemValue(pkItemScroll, 0) == YONGSIN_SCROLL ||
		GetItemValue(pkItemScroll, 0) == YAGONG_SCROLL) // ÇöÃ¶, ¿ë½�
// ÀÇ Ãàº¹¼­, ¾ß°øÀÇ ºñÀü¼­  Ã³¸®
	{
		const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };
		const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

		if (GetItemValue(pkItemScroll, 0) == YONGSIN_SCROLL)
		{
			success_prob = hyuniron_prob[MINMAX(0, GetItemRefineLevel(item), 8)];
		}
		else if (GetItemValue(pkItemScroll, 0) == YAGONG_SCROLL)
		{
			success_prob = yagong_prob[MINMAX(0, GetItemRefineLevel(item), 8)];
		}
		else if (GetItemValue(pkItemScroll, 0) == HYUNIRON_CHN) {} // @fixme121
		else
		{
			LOG_ERROR("REFINE : Unknown refine scroll item. Value0: {}", GetItemValue(pkItemScroll, 0));
		}

		if (GetItemValue(pkItemScroll, 0) == HYUNIRON_CHN) // ÇöÃ¶Àº ¾ÆÀÌ�
// ÛÀÌ ºÎ¼­Á®¾ß ÇÑ´Ù.
			bDestroyWhenFail = true;

		// DETAIL_REFINE_LOG
		if (GetItemValue(pkItemScroll, 0) == HYUNIRON_CHN)
		{
			szRefineType = "HYUNIRON";
		}
		else if (GetItemValue(pkItemScroll, 0) == YONGSIN_SCROLL)
		{
			szRefineType = "GOD_SCROLL";
		}
		else if (GetItemValue(pkItemScroll, 0) == YAGONG_SCROLL)
		{
			szRefineType = "YAGONG_SCROLL";
		}
		// END_OF_DETAIL_REFINE_LOG
	}
	// DETAIL_REFINE_LOG
	if (GetItemValue(pkItemScroll, 0) == MUSIN_SCROLL)
	{
		
		success_prob += 100; // Musin izé mindig sikeres 
		if (success_prob > 100)
			success_prob = 100;

		szRefineType = "MUSIN_SCROLL";
	}
	// END_OF_DETAIL_REFINE_LOG
	else if (GetItemValue(pkItemScroll, 0) == MEMO_SCROLL)
	{
		success_prob = 100;
		szRefineType = "MEMO_SCROLL";
	}
	else if (GetItemValue(pkItemScroll, 0) == BDRAGON_SCROLL)
	{
		success_prob = 80;
		szRefineType = "BDRAGON_SCROLL";
	}

#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	success_prob += CRefineManager::instance().Result(e);

#endif
	ConsumeItemEcs(pkItemScroll);

	if (prob <= success_prob)
	{
		// ¼º°ø! ¸ðµç ¾ÆÀÌ�
// ÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ´Ù¸¥ ¾ÆÀÌ�
// Û È¹µæ
		const entt::entity pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (IsValidItem(pkNewItem))
		{
			CopyAllAttrToEcs(item, pkNewItem);
			LogManager::instance().ItemLogEntity(e, pkNewItem, "REFINE SUCCESS", GetItemName(pkNewItem));

			uint8_t bCell = GetItemCell(item);


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(e);
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == GetItemVnum(item) && ecs::PlayerRuntime::GetMissionProgress(e, REFINE_ITEM, bBattlePassId) < dwCount)
						ecs::PlayerRuntime::UpdateMissionProgress(e, REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			NotifyRefineSuccess(e, item, szRefineType);

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, GetItemVnum(item), -prt->cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			InventorySystem::AddToCharacter(pkNewItem, e, TItemPos(INVENTORY, bCell));
			FlushDelayedSaveEcs(pkNewItem);


			AttrLog(pkNewItem);
			//ecs::PointSystem::Change(e, POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(e);
#endif
			InventorySystem::PayRefineFee(e, prt->cost);
#ifdef ENABLE_UPGRADE_NOTICE_BY_RAZOR93
			if (GetItemRefineLevel(pkNewItem) >= 8)
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					GetItemVnum(pkNewItem),
					GetItemSocket(pkNewItem, 0),
					GetItemSocket(pkNewItem, 1),
					GetItemSocket(pkNewItem, 2),
					0, // transmute
					0  // transmute2 
				);

				// Bónuszok
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = GetItemAttributeType(pkNewItem, i);
					short val = GetItemAttributeValue(pkNewItem, i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				// debug log:
				//LOG_INFO("ItemLink Debug: {}", itemlink);
				//LOG_INFO(0, "Socket0=%d Socket1=%d Socket2=%d",
					//GetItemSocket(pkNewItem, 0),
					//GetItemSocket(pkNewItem, 1),
					//GetItemSocket(pkNewItem, 2));

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					ecs::PlayerRuntime::GetName(e).data(), itemlink, GetItemName(pkNewItem));

				SPacketGGNotice packet;
				strlcpy(packet.szText, szChat, sizeof(packet.szText));
				//P2P_MANAGER::instance().Send(&packet, sizeof(packet));

				BroadcastNotice(szChat); // ez kell a jelenlegi ch-ra

			}


			if (allowedVnums.find(GetItemVnum(pkNewItem)) != allowedVnums.end())
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					GetItemVnum(pkNewItem),
					GetItemSocket(pkNewItem, 0),
					GetItemSocket(pkNewItem, 1),
					GetItemSocket(pkNewItem, 2),
					0, // transmute
					0  // transmute2 
				);

				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = GetItemAttributeType(pkNewItem, i);
					short val = GetItemAttributeValue(pkNewItem, i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					ecs::PlayerRuntime::GetName(e).data(), itemlink, GetItemName(pkNewItem));

				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, szChat);
			}
#endif ENABLE_UPGRADE_NOTICE_BY_RAZOR93
		}
		else
		{
			// ¾ÆÀÌ�
// Û »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			LOG_ERROR("cannot create item {}", result_vnum);
			NotifyRefineFail(e, item, szRefineType);
		}

	}
	else if (!bDestroyWhenFail && result_fail_vnum)
	{
		// ½ÇÆÐ! ¸ðµç ¾ÆÀÌ�
// ÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ³·Àº µî±ÞÀÇ ¾ÆÀÌ�
// Û È¹µæ
		const entt::entity pkNewItem = ITEM_MANAGER::instance().CreateItem(result_fail_vnum, 1, 0, false);

		if (IsValidItem(pkNewItem))
		{
			CopyAllAttrToEcs(item, pkNewItem);
			LogManager::instance().ItemLogEntity(e, pkNewItem, "REFINE FAIL", GetItemName(pkNewItem));

			uint8_t bCell = GetItemCell(item);


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(e);
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == GetItemVnum(item) && ecs::PlayerRuntime::GetMissionProgress(e, REFINE_ITEM, bBattlePassId) < dwCount)
						ecs::PlayerRuntime::UpdateMissionProgress(e, REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, GetItemVnum(item), -prt->cost);
			NotifyRefineFail(e, item, szRefineType, -1);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

			InventorySystem::AddToCharacter(pkNewItem, e, TItemPos(INVENTORY, bCell));
			FlushDelayedSaveEcs(pkNewItem);

			AttrLog(pkNewItem);

			//ecs::PointSystem::Change(e, POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(e);
#endif
			InventorySystem::PayRefineFee(e, prt->cost);
		}
		else
		{
			// ¾ÆÀÌ�
// Û »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			LOG_ERROR("cannot create item {}", result_fail_vnum);
			NotifyRefineFail(e, item, szRefineType);
		}
	}
	else
	{
		NotifyRefineFail(e, item, szRefineType); // °³·®½Ã ¾ÆÀÌ�
// Û »ç¶óÁöÁö ¾ÊÀ½

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset(e);
#endif
		InventorySystem::PayRefineFee(e, prt->cost);
	}

	return true;

}
#else
// Refining with a scroll rather than materials.
bool DoRefineWithScroll(entt::entity e, entt::entity item)
{
	if (!InventorySystem::CanHandleItems(e, true))
	{
		InventorySystem::ClearRefineMode(e);
		return false;
	}

	InventorySystem::ClearRefineMode(e);

	//°³·® ½Ã°£Á¦ÇÑ : upgrade_refine_scroll.quest ¿¡¼­ °³·®ÈÄ 5ºÐÀÌ³»¿¡ ÀÏ¹Ý °³·®À»
	//ÁøÇàÇÒ¼ö ¾øÀ½
	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			LOG_INFO("can't refine {} {}", ecs::PlayerRuntime::GetPlayerID(e), ecs::PlayerRuntime::GetName(e).data());
			return false;
		}
	}

	const TRefineTable* prt = CRefineManager::instance().GetRefineRecipe(GetItemRefineSet(item));

	if (!prt)
		return false;


	// °³·®¼­ Ã¼�
// ©
	if (InventorySystem::GetRefineScrollCell(e) < 0)
		return false;

	const entt::entity pkItemScroll = GetInventoryItem(e, InventorySystem::GetRefineScrollCell(e));

	if (pkItemScroll == entt::null)
		return false;

	if (!(GetItemType(pkItemScroll) == ITEM_USE && GetItemSubType(pkItemScroll) == USE_TUNING))
		return false;

	if (GetItemVnum(pkItemScroll) == GetItemVnum(item))
		return false;

	uint32_t result_vnum = GetItemRefinedVnum(item);
	uint32_t result_fail_vnum = GetItemRefineFromVnum(item);

	if (result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	// MUSIN_SCROLL
	if (GetItemValue(pkItemScroll, 0) == MUSIN_SCROLL)
	{
		if (GetItemRefineLevel(item) >= 4)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 305, "");
#endif
			return false;
		}
	}
	// END_OF_MUSIC_SCROLL

	else if (GetItemValue(pkItemScroll, 0) == MEMO_SCROLL)
	{
		if (GetItemRefineLevel(item) != GetItemValue(pkItemScroll, 1))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 417, "%s#%s", GetItemName(item), GetItemName(pkItemScroll));
#endif
			return false;
		}
	}
	else if (GetItemValue(pkItemScroll, 0) == BDRAGON_SCROLL)
	{
		if (GetItemType(item) != ITEM_METIN || GetItemRefineLevel(item) != 4)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 665, "%s#%s", GetItemName(item), GetItemName(pkItemScroll));
#endif
			return false;
		}
	}

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(GetItemRefinedVnum(item));

	if (!pProto)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	if (ecs::PointSystem::GetGold(e) < prt->cost)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 232, "");
#endif
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset_percent(e);
#endif
		return false;
	}

	for (int i = 0; i < prt->material_count; ++i)
	{
		if (CountItem(e, prt->materials[i].vnum) < prt->materials[i].count)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 233, "");
#endif
			return false;
		}
	}

	for (int i = 0; i < prt->material_count; ++i)
		RemoveSpecifyItemEcs(e, prt->materials[i].vnum, prt->materials[i].count);

	int prob = number(1, 100);
	int success_prob = prt->prob;
	bool bDestroyWhenFail = false;

	const char* szRefineType = "SCROLL";

	if (GetItemValue(pkItemScroll, 0) == HYUNIRON_CHN ||
		GetItemValue(pkItemScroll, 0) == YONGSIN_SCROLL ||
		GetItemValue(pkItemScroll, 0) == YAGONG_SCROLL) // ÇöÃ¶, ¿ë½�
// ÀÇ Ãàº¹¼­, ¾ß°øÀÇ ºñÀü¼­  Ã³¸®
	{
		const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };
		const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

		if (GetItemValue(pkItemScroll, 0) == YONGSIN_SCROLL)
		{
			success_prob = hyuniron_prob[MINMAX(0, GetItemRefineLevel(item), 8)];
		}
		else if (GetItemValue(pkItemScroll, 0) == YAGONG_SCROLL)
		{
			success_prob = yagong_prob[MINMAX(0, GetItemRefineLevel(item), 8)];
		}
		else if (GetItemValue(pkItemScroll, 0) == HYUNIRON_CHN) {} // @fixme121
		else
		{
			LOG_ERROR("REFINE : Unknown refine scroll item. Value0: {}", GetItemValue(pkItemScroll, 0));
		}

		if (GetItemValue(pkItemScroll, 0) == HYUNIRON_CHN) // ÇöÃ¶Àº ¾ÆÀÌ�
// ÛÀÌ ºÎ¼­Á®¾ß ÇÑ´Ù.
			bDestroyWhenFail = true;

		// DETAIL_REFINE_LOG
		if (GetItemValue(pkItemScroll, 0) == HYUNIRON_CHN)
		{
			szRefineType = "HYUNIRON";
		}
		else if (GetItemValue(pkItemScroll, 0) == YONGSIN_SCROLL)
		{
			szRefineType = "GOD_SCROLL";
		}
		else if (GetItemValue(pkItemScroll, 0) == YAGONG_SCROLL)
		{
			szRefineType = "YAGONG_SCROLL";
		}
		// END_OF_DETAIL_REFINE_LOG
	}

	// DETAIL_REFINE_LOG
	if (GetItemValue(pkItemScroll, 0) == MUSIN_SCROLL) // ¹«½�
// ÀÇ Ãàº¹¼­´Â 100% ¼º°ø (+4±îÁö¸¸)
	{
		success_prob = 100;

		szRefineType = "MUSIN_SCROLL";
	}
	// END_OF_DETAIL_REFINE_LOG
	else if (GetItemValue(pkItemScroll, 0) == MEMO_SCROLL)
	{
		success_prob = 100;
		szRefineType = "MEMO_SCROLL";
	}
	else if (GetItemValue(pkItemScroll, 0) == BDRAGON_SCROLL)
	{
		success_prob = 80;
		szRefineType = "BDRAGON_SCROLL";
	}

#ifdef ENABLE_FEATURES_REFINE_SYSTEM	
	success_prob += CRefineManager::instance().Result(e);

#endif
	ConsumeItemEcs(pkItemScroll);

	if (prob <= success_prob)
	{
		// ¼º°ø! ¸ðµç ¾ÆÀÌ�
// ÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ´Ù¸¥ ¾ÆÀÌ�
// Û È¹µæ
		const entt::entity pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem != entt::null)
		{
			CopyAllAttrToEcs(item, pkNewItem);
			LogManager::instance().ItemLogEntity(e, pkNewItem, "REFINE SUCCESS", GetItemName(pkNewItem));

			uint8_t bCell = GetItemCell(item);


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(e);
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == GetItemVnum(item) && ecs::PlayerRuntime::GetMissionProgress(e, REFINE_ITEM, bBattlePassId) < dwCount)
						ecs::PlayerRuntime::UpdateMissionProgress(e, REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			NotifyRefineSuccess(e, item, szRefineType);

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, GetItemVnum(item), -prt->cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			InventorySystem::AddToCharacter(pkNewItem, e, TItemPos(INVENTORY, bCell));
			FlushDelayedSaveEcs(pkNewItem);


			AttrLog(pkNewItem);
			//ecs::PointSystem::Change(e, POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(e);
#endif
			InventorySystem::PayRefineFee(e, prt->cost);
#ifdef ENABLE_UPGRADE_NOTICE_BY_RAZOR93
			if (GetItemRefineLevel(pkNewItem) >= 8)
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					GetItemVnum(pkNewItem),
					GetItemSocket(pkNewItem, 0),
					GetItemSocket(pkNewItem, 1),
					GetItemSocket(pkNewItem, 2),
					0, // transmute
					0  // transmute2 
				);

				// Bónuszok
				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = GetItemAttributeType(pkNewItem, i);
					short val = GetItemAttributeValue(pkNewItem, i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				// debug log:
				//LOG_INFO(0, "ItemLink Debug: %s", itemlink);
				//LOG_INFO(0, "Socket0=%d Socket1=%d Socket2=%d",
					//GetItemSocket(pkNewItem, 0),
					//GetItemSocket(pkNewItem, 1),
					//GetItemSocket(pkNewItem, 2));

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					ecs::PlayerRuntime::GetName(e).data(), itemlink, GetItemName(pkNewItem));

				SPacketGGNotice packet;
				strlcpy(packet.szText, szChat, sizeof(packet.szText));
				//P2P_MANAGER::instance().Send(&packet, sizeof(packet));

				BroadcastNotice(szChat); // ez kell a jelenlegi ch-ra

			}


			if (allowedVnums.find(GetItemVnum(pkNewItem)) != allowedVnums.end())
			{
				char itemlink[512];
				int len = 0;

				len += snprintf(itemlink + len, sizeof(itemlink) - len, "item:%x:%x:%x:%x:%x:%x",
					GetItemVnum(pkNewItem),
					GetItemSocket(pkNewItem, 0),
					GetItemSocket(pkNewItem, 1),
					GetItemSocket(pkNewItem, 2),
					0, // transmute
					0  // transmute2 
				);

				for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					uint8_t type = GetItemAttributeType(pkNewItem, i);
					short val = GetItemAttributeValue(pkNewItem, i);

					if (type != 0 && val != 0)
						len += snprintf(itemlink + len, sizeof(itemlink) - len, ":%x:%d", type, val);
				}

				char szChat[2048];
				snprintf(szChat, sizeof(szChat),
					"|cff00ff00[%s]|r Successfully upgraded:|cffffd700|H%s|h[%s]|h|r",
					ecs::PlayerRuntime::GetName(e).data(), itemlink, GetItemName(pkNewItem));

				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, szChat);
			}
#endif ENABLE_UPGRADE_NOTICE_BY_RAZOR93
		}
		else
		{
			// ¾ÆÀÌ�
// Û »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			LOG_ERROR("cannot create item {}", result_vnum);
			NotifyRefineFail(e, item, szRefineType);
		}

	}
	else if (!bDestroyWhenFail && result_fail_vnum)
	{
		// ½ÇÆÐ! ¸ðµç ¾ÆÀÌ�
// ÛÀÌ »ç¶óÁö°í, °°Àº ¼Ó¼ºÀÇ ³·Àº µî±ÞÀÇ ¾ÆÀÌ�
// Û È¹µæ
		const entt::entity pkNewItem = ITEM_MANAGER::instance().CreateItem(result_fail_vnum, 1, 0, false);

		if (pkNewItem != entt::null)
		{
			CopyAllAttrToEcs(item, pkNewItem);
			LogManager::instance().ItemLogEntity(e, pkNewItem, "REFINE FAIL", GetItemName(pkNewItem));

			uint8_t bCell = GetItemCell(item);


#ifdef ENABLE_BATTLE_PASS
			uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(e);
			if (bBattlePassId)
			{
				uint32_t dwItemVnum, dwCount;
				if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, REFINE_ITEM, &dwItemVnum, &dwCount))
				{
					if (dwItemVnum == GetItemVnum(item) && ecs::PlayerRuntime::GetMissionProgress(e, REFINE_ITEM, bBattlePassId) < dwCount)
						ecs::PlayerRuntime::UpdateMissionProgress(e, REFINE_ITEM, bBattlePassId, 1, dwCount);
				}
			}
#endif

			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, GetItemVnum(item), -prt->cost);
			NotifyRefineFail(e, item, szRefineType, -1);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

			InventorySystem::AddToCharacter(pkNewItem, e, TItemPos(INVENTORY, bCell));
			FlushDelayedSaveEcs(pkNewItem);

			AttrLog(pkNewItem);

			//ecs::PointSystem::Change(e, POINT_GOLD, -prt->cost);
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
			CRefineManager::instance().Reset(e);
#endif
			InventorySystem::PayRefineFee(e, prt->cost);
		}
		else
		{
			// ¾ÆÀÌ�
// Û »ý¼º¿¡ ½ÇÆÐ -> °³·® ½ÇÆÐ·Î °£ÁÖ
			LOG_ERROR("cannot create item {}", result_fail_vnum);
			NotifyRefineFail(e, item, szRefineType);
		}
	}
	else
	{
		NotifyRefineFail(e, item, szRefineType); // °³·®½Ã ¾ÆÀÌ�
// Û »ç¶óÁöÁö ¾ÊÀ½

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
		CRefineManager::instance().Reset(e);
#endif
		InventorySystem::PayRefineFee(e, prt->cost);
	}

	return true;

}
#endif

// Refining a soul item with its scroll.
bool DoRefineItemSoul(entt::entity e, entt::entity item)
{
	if (!InventorySystem::CanHandleItems(e, true))
	{
		InventorySystem::ClearRefineMode(e);
		return false;
	}

	InventorySystem::ClearRefineMode(e);


	if (InventorySystem::GetRefineScrollCell(e) < 0)
		return false;

	const entt::entity pkItemScroll = GetInventoryItem(e, InventorySystem::GetRefineScrollCell(e));

	if (pkItemScroll == entt::null)
		return false;

	if (!(GetItemType(pkItemScroll) == ITEM_USE && GetItemSubType(pkItemScroll) == USE_TUNING))
		return false;

	if (GetItemVnum(pkItemScroll) == GetItemVnum(item))
		return false;

	uint32_t resultVnum = GetItemRefinedVnum(item);

	if (resultVnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 666, "%s", GetItemName(item));
#endif
		return false;
	}

	TItemTable* pProto = ITEM_MANAGER::instance().GetTable(GetItemRefinedVnum(item));

	if (!pProto)
	{
		LOG_ERROR("DoRefineWithScroll NOT GET ITEM PROTO {}", GetItemRefinedVnum(item));
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 305, "");
#endif
		return false;
	}

	int prob = number(1, 100);
	int successProb = GetItemValue(pkItemScroll, 1);

	ConsumeItemEcs(pkItemScroll);

	if (prob <= successProb)
	{
		const entt::entity pkNewItem = ITEM_MANAGER::instance().CreateItem(resultVnum, 1, 0, false);
		if (IsValidItem(pkNewItem))
		{
			uint8_t bCell = GetItemCell(item);
			ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "RefineSoulSuceeded");
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			InventorySystem::AddToCharacter(pkNewItem, e, TItemPos(INVENTORY, bCell));
			FlushDelayedSaveEcs(pkNewItem);
		}
		else
		{
			LOG_ERROR("Cannot create item soul {}", resultVnum);
			ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "RefineSoulFailed");
		}
	}
	else
	{
		ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "RefineSoulFailed");
	}

	return true;
}

// The refine window: what it costs, what it needs and how likely it is.
bool RefineInformation(entt::entity e, uint8_t bCell, uint8_t bType, int iAdditionalCell)
{
	if (bCell > INVENTORY_MAX_NUM)
		return false;

	const entt::entity item = GetInventoryItem(e, bCell);


	if (item == entt::null)
		return false;

#ifdef ATTR_LOCK
	if (GetItemLockedAttr(item) != -1)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 784, "");
#endif
		return false;
	}
#endif

	// REFINE_COST
	if (bType == REFINE_TYPE_MONEY_ONLY && !ecs::PlayerRuntime::GetQuestFlag(e, "deviltower_zone.can_refine"))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 361, "");
#endif
		return false;
	}
	// END_OF_REFINE_COST

	TPacketGCRefineInformation p;

	p.header = HEADER_GC_REFINE_INFORMATION;
	p.pos = bCell;
	p.src_vnum = GetItemVnum(item);
	p.result_vnum = GetItemRefinedVnum(item);
	p.type = bType;

	if (p.result_vnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	if (GetItemType(item) == ITEM_USE && GetItemSubType(item) == USE_TUNING)
	{
		if (bType == 0)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 424, "");
#endif
			return false;
		}
		else
		{
			const entt::entity itemScroll = GetInventoryItem(e, iAdditionalCell);
			if (itemScroll == entt::null || GetItemVnum(item) == GetItemVnum(itemScroll))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 229, "");
#endif
				return false;
			}
		}
	}

#ifdef ENABLE_SOUL_SYSTEM
	if (bType == REFINE_TYPE_SOUL)
	{
		const entt::entity itemScroll = GetInventoryItem(e, iAdditionalCell);
		if (itemScroll == entt::null)
			return false;

		p.cost = 0;
		p.prob = GetItemValue(itemScroll, 1);
		p.material_count = 0;
		memset(p.materials, 0, sizeof(p.materials));

		ecs::PlayerRuntime::GetDesc(e)->Packet(&p, sizeof(TPacketGCRefineInformation));

		InventorySystem::SetRefineMode(e, iAdditionalCell);
		return true;
	}
#endif

	CRefineManager& rm = CRefineManager::instance();

	const TRefineTable* prt = rm.GetRefineRecipe(GetItemRefineSet(item));

	if (!prt)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 427, "");
#endif
		return false;
	}

	p.cost = InventorySystem::ComputeRefineFee(e, prt->cost);
#ifdef NEW_POINT_EXP_DOUBLE_BONUS_RAZOR93
	int success_prob = prt->prob;

	// Kijelzett esély igazítása scroll típus alapján (hogy a kliens ugyanazt lássa, mint amit a szerver használ)
	if (bType != REFINE_TYPE_MONEY_ONLY)
	{
		const entt::entity pkScroll = GetInventoryItem(e, iAdditionalCell);
		if (pkScroll != entt::null && GetItemType(pkScroll) == ITEM_USE && GetItemSubType(pkScroll) == USE_TUNING)
		{
			const int scrollType = GetItemValue(pkScroll, 0);

			if (scrollType == YONGSIN_SCROLL || scrollType == YAGONG_SCROLL || scrollType == HYUNIRON_CHN)
			{
				const char hyuniron_prob[9] = { 100, 75, 65, 55, 45, 40, 35, 25, 20 };
				const char yagong_prob[9] = { 100, 100, 90, 80, 70, 60, 50, 30, 20 };

				if (scrollType == YONGSIN_SCROLL)
					success_prob = hyuniron_prob[MINMAX(0, GetItemRefineLevel(item), 8)];
				else if (scrollType == YAGONG_SCROLL)
					success_prob = yagong_prob[MINMAX(0, GetItemRefineLevel(item), 8)];
				// HYUNIRON_CHN: marad a prt->prob
			}
			else if (scrollType == MUSIN_SCROLL)
			{
				//if (GetItemRefineLevel(item) >= 9)
				//{
				//	ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "MAX +9 with this scroll!");
				//	return false;
				//}
				success_prob += 100;
				if (success_prob > 100)
					success_prob = 100;
			}
			else if (scrollType == MEMO_SCROLL)
			{
				if (GetItemRefineLevel(item) != GetItemValue(pkScroll, 1))
					return false;
				success_prob = 100;
			}
			else if (scrollType == BDRAGON_SCROLL)
			{
				if (GetItemType(item) != ITEM_METIN || GetItemRefineLevel(item) != 4)
					return false;
				success_prob = 80;
			}
		}
	}

#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	success_prob += CRefineManager::instance().Result(e);
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

	ecs::PlayerRuntime::GetDesc(e)->Packet(&p, sizeof(TPacketGCRefineInformation));

	InventorySystem::SetRefineMode(e, iAdditionalCell);
	return true;
}

// Using one item on another: a scroll opens the refine window, a
// detachment scroll pulls the metin stones back out.
bool RefineItem(entt::entity e, entt::entity pkItem, entt::entity pkTarget)
{
	if (!InventorySystem::CanHandleItems(e))
		return false;

#ifdef ENABLE_SOUL_SYSTEM
	uint32_t vnum = GetItemVnum(pkItem);
	if ((vnum == 70602 || vnum == 70603 || vnum == 88958) && GetItemType(pkTarget) != ITEM_SOUL) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 1294, "%s", GetItemName(pkItem));
#endif
		return false;
	}
#endif

	if (GetItemSubType(pkItem) == USE_TUNING)
	{
		// XXX ¼º´É, ¼ÒÄÏ °³·®¼­´Â »ç¶óÁ³½À´Ï´Ù...
		// XXX ¼º´É°³·®¼­´Â Ãàº¹ÀÇ ¼­°¡ µÇ¾ú´Ù!
		// MUSIN_SCROLL
		if (GetItemValue(pkItem, 0) == MUSIN_SCROLL)
			RefineInformation(e, GetItemCell(pkTarget), REFINE_TYPE_MUSIN, GetItemCell(pkItem));
		// END_OF_MUSIN_SCROLL

#ifdef ENABLE_SOUL_SYSTEM
		else if (GetItemValue(pkItem, 0) == SOUL_SCROLL)
			RefineInformation(e, GetItemCell(pkTarget), REFINE_TYPE_SOUL, GetItemCell(pkItem));
#endif

		else if (GetItemValue(pkItem, 0) == HYUNIRON_CHN)
			RefineInformation(e, GetItemCell(pkTarget), REFINE_TYPE_HYUNIRON, GetItemCell(pkItem));
		else if (GetItemValue(pkItem, 0) == BDRAGON_SCROLL)
		{
			if (GetItemRefineSet(pkTarget) != 702) return false;
			RefineInformation(e, GetItemCell(pkTarget), REFINE_TYPE_BDRAGON, GetItemCell(pkItem));
		}
		else
		{
			if (GetItemRefineSet(pkTarget) == 501) return false;
			RefineInformation(e, GetItemCell(pkTarget), REFINE_TYPE_SCROLL, GetItemCell(pkItem));
		}
	}
	else if (GetItemSubType(pkItem) == USE_DETACHMENT && IS_SET(GetItemFlags(pkTarget), ITEM_FLAG_REFINEABLE))
	{
		LogManager::instance().ItemLogEntity(e, pkTarget, "USE_DETACHMENT", GetItemName(pkTarget));

		bool bHasMetinStone = false;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
		{
			int32_t socket = GetItemSocket(pkTarget, i);
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
				int32_t socket = GetItemSocket(pkTarget, i);
				if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				{
					AutoGiveItemEcs(e, socket);
					//TItemTable* pTable = ITEM_MANAGER::instance().GetTable(GetItemSocket(pkTarget, i));
					//SetItemSocket(pkTarget, i, pTable->alValues[2]);
					// ±úÁøµ¹·Î ´ëÃ¼ÇØÁØ´Ù
					SetItemSocketEcs(pkTarget, i, ITEM_BROKEN_METIN_VNUM);
				}
			}
			ConsumeItemEcs(pkItem);
			return true;
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 360, "");
#endif
			return false;
		}
	}

	return false;
}

// Carries a refined item's sockets across to the item that replaces it.
void TransformRefineItem(entt::entity pkOldItem, entt::entity pkNewItem)
{

	// ACCESSORY_REFINE
	if (IsAccessoryForSocket(pkOldItem))
	{
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			SetItemSocket(pkNewItem, i, GetItemSocket(pkOldItem, i));
		}
		//StartAccessorySocketExpireEvent(pkNewItem);
	}
	// END_OF_ACCESSORY_REFINE
	else
	{
		// ¿©±â¼­ ±úÁø¼®ÀÌ ÀÚµ¿ÀûÀ¸·Î Ã»¼Ò µÊ
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			if (!GetItemSocket(pkOldItem, i))
				break;
			else
				SetItemSocket(pkNewItem, i, 1);
		}

		// ¼ÒÄÏ ¼³Á¤
		int slot = 0;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			int32_t socket = GetItemSocket(pkOldItem, i);

			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				SetItemSocket(pkNewItem, slot++, socket);
		}

	}

	// ¸�
// Á÷ ¾ÆÀÌ�
// Û ¼³Á¤
	CopyItemAttributesEcs(pkOldItem, pkNewItem);
}

} // namespace ItemSystem

namespace ItemSystem {

// Stripping an acce of the attributes it absorbed.
bool CleanAcceAttr(entt::entity e, entt::entity pkItem, entt::entity pkTarget)
{
    if (!InventorySystem::CanHandleItems(e))
        return false;
    else if (!IsValidItem(pkItem) || !IsValidItem(pkTarget))
        return false;

    if ((GetItemType(pkTarget) != ITEM_COSTUME) &&
		(GetItemSubType(pkTarget) != COSTUME_ACCE))
        return false;

    if (GetItemSocket(pkTarget, ACCE_ABSORBED_SOCKET) <= 0)
        return false;

    SetItemSocket(pkTarget, ACCE_ABSORBED_SOCKET, 0);
    for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
        SetItemForceAttributeEcs(pkTarget, i, 0, 0);

    ConsumeItemEcs(pkItem);
    LogManager::instance().ItemLogEntity(
		e, pkTarget, "USE_DETACHMENT (CLEAN ATTR)",
		GetItemName(pkTarget));
    return true;
}

} // namespace ItemSystem

namespace InventorySystem {

// The first inventory cell an item of this size fits in, or -1. The belt
// is a special inventory and is deliberately not searched.
int GetEmptyInventory(entt::entity e, uint8_t size)
{
	// NOTE: ÇöÀç ÀÌ ÇÔ¼ö´Â ¾ÆÀÌ�
// Û Áö±Þ, È¹µæ µîÀÇ ÇàÀ§¸¦ ÇÒ ¶§ ÀÎº¥�
// ä¸®ÀÇ ºó Ä­À» Ã£±â À§ÇØ »ç¿ëµÇ°í ÀÖ´Âµ¥,
	//		º§Æ® ÀÎº¥�
// ä¸®´Â Æ¯¼ö ÀÎº¥�
// ä¸®ÀÌ¹Ç·Î °Ë»çÇÏÁö ¾Êµµ·Ï ÇÑ´Ù. (±âº» ÀÎº¥�
// ä¸®: INVENTORY_MAX_NUM ±îÁö¸¸ °Ë»ç)
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

	case DRAGON_SOUL_INVENTORY:
		return cell < (DRAGON_SOUL_INVENTORY_MAX_NUM);
#ifdef ENABLE_SWITCHBOT
	case SWITCHBOT:
		return cell < SWITCHBOT_SLOT_COUNT;
#endif
	case SAFEBOX:
		if (const auto storage = SafeboxSystem::Get(owner, SAFEBOX))
			return storage->IsValidPosition(cell);
		else
			return false;

	case MALL:
		if (const auto storage = SafeboxSystem::Get(owner, MALL))
			return storage->IsValidPosition(cell);
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

// The entry point from the client: check the cell, merge onto a stack if
// that is what the drop means, otherwise use the item.
bool UseItem(entt::entity e, TItemPos Cell, TItemPos DestCell)
{

#ifdef ENABLE_USEITEM_COOLDOWN
	if (ecs::PlayerRuntime::GetMapIndex(e) == 113) {
		return false;
	}
#endif

	uint16_t wCell = Cell.cell;
	uint8_t window_type = Cell.window_type;
	//uint16_t wDestCell = DestCell.cell;
	//uint8_t bDestInven = DestCell.window_type;
	entt::entity item = entt::null;

	if (!InventorySystem::CanHandleItems(e))
		return false;

	if (!InventorySystem::IsValidItemPosition(e, Cell) || (item = GetItem(e, Cell)) == entt::null)
		return false;

#ifdef ENABLE_USEITEM_COOLDOWN
	if (GetItemVnum(item) >= 39999 && GetItemType(item) == ITEM_QUEST) {
		int pulse = thecore_pulse();
		if (pulse > ecs::PlayerRuntime::GetCmdAntiFloodPulse(e) + PASSES_PER_SEC(1)) {
			ecs::PlayerRuntime::SetItemUseAntiFloodCount(e, 0);
			ecs::PlayerRuntime::SetItemUseAntiFloodPulse(e, thecore_pulse());
		}

		if (ecs::PlayerRuntime::IncreaseItemUseAntiFloodCount(e) >= 10) {
			ecs::PlayerRuntime::GetDesc(e)->DelayedDisconnect(0);
			return false;
		}

		ecs::PlayerRuntime::SetCmdAntiFloodPulse(e, pulse);
	}
#endif

	const entt::entity destItem = GetItem(e, DestCell);
	if (destItem != entt::null && item != destItem && IsItemStackable(destItem) && !IS_SET(GetItemAntiFlags(destItem), ITEM_ANTIFLAG_STACK) && GetItemVnum(destItem) == GetItemVnum(item))
	{
		// A committed merge can consume item and destroy this character.
		InventorySystem::MoveItem(e, Cell, DestCell, 0);
		return false;
	}

#ifdef ENABLE_BUG_FIXES
	if (quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(e))->IsRunning() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 1247, "");
#endif
		//if (ecs::PlayerRuntime::GetDesc(e)) {
		//	ecs::PlayerRuntime::GetDesc(e)->DelayedDisconnect(3);
		//}
		return false;
	}
#endif

	LOG_INFO("{}: USE_ITEM {} (inven {}, cell: {})", ecs::PlayerRuntime::GetName(e).data(), GetItemName(item), window_type, wCell);

	if (IsItemExchanging(item))
		return false;
	// Lua-less item_change quest handlers
	if (item_change::HandleUse(ecs::LegacyCharOf(e), LegacyItemBoundary(item)))
		return true;
#ifdef ENABLE_SWITCHBOT
	if (Cell.IsSwitchbotPosition())
	{
		CSwitchbot* pkSwitchbot = CSwitchbotManager::Instance().FindSwitchbot(ecs::PlayerRuntime::GetPlayerID(e));
		if (pkSwitchbot && pkSwitchbot->IsActive(Cell.cell))
		{
			return false;
		}

		int iEmptyCell = InventorySystem::GetEmptyInventory(e, GetItemSize(item));

		if (iEmptyCell == -1)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 687, "");
#endif
			return false;
		}

		return InventorySystem::MoveItem(e, Cell, TItemPos(INVENTORY, iEmptyCell), 0);
	}
#endif
	if (!CanUsedBy(item, e))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 495, "");
#endif
		return false;
	}

	if (CombatSystem::IsStun(e))
		return false;

	if (false == InventorySystem::IsEquipmentSexAllowed(e, item))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 496, "");
#endif
		return false;
	}

#ifdef ENABLE_PVP_ADVANCED	
	if ((ecs::PlayerRuntime::GetDuelOption(e, "BlockPotion")) && IS_POTION_PVP_BLOCKED(GetItemVnum(item)))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 516, "");
#endif
		return false;
	}
#endif	

	//PREVENT_TRADE_WINDOW
	if (IS_SUMMON_ITEM(GetItemVnum(item)))
	{
		if (false == IS_SUMMONABLE_ZONE(ecs::PlayerRuntime::GetMapIndex(e)))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 688, "");
#endif
			return false;
		}

		int iPulse = thecore_pulse();

		//Ã¢°í ¿¬ÈÄ Ã¼�
// ©
		if (iPulse - ecs::SocialSystem::GetSafeboxLoadTime(e) < PASSES_PER_SEC(g_nPortalLimitTime))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
			return false;
		}

		//°�
// ·¡°ü·Ã Ã¢ Ã¼�
// ©
		if (ExchangeSystem::IsActive(e) || ecs::SocialSystem::GetMyShop(e) || ecs::SocialSystem::GetShopOwner(e) != entt::null || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 235, "");
#endif
			return false;
		}

#ifdef __ATTR_TRANSFER_SYSTEM__
		if (AttrTransfer_is_open(e))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 235, "");
#endif
			return false;
		}
#endif

		//PREVENT_REFINE_HACK
		//°³·®ÈÄ ½Ã°£Ã¼�
// ©
		{
			if (iPulse - ecs::SocialSystem::GetRefineTime(e) < PASSES_PER_SEC(g_nPortalLimitTime))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
				return false;
			}
		}
		//END_PREVENT_REFINE_HACK


		//PREVENT_ITEM_COPY
		{
			if (iPulse - ecs::SocialSystem::GetMyShopTime(e) < PASSES_PER_SEC(g_nPortalLimitTime))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
				return false;
			}

		}
		//END_PREVENT_ITEM_COPY


		//±ÍÈ¯ºÎ °�
// ¸®Ã¼�
// ©
		if (GetItemVnum(item) != 70302)
		{
			PIXEL_POSITION posWarp;

			int x = 0;
			int y = 0;

			double nDist = 0;
			const double nDistant = 5000.0;
			//±ÍÈ¯±â¾ïºÎ
			if (GetItemVnum(item) == 22010)
			{
				x = GetItemSocket(item, 0) - ecs::PlayerRuntime::GetX(e);
				y = GetItemSocket(item, 1) - ecs::PlayerRuntime::GetY(e);
			}
			//±ÍÈ¯ºÎ
			else if (GetItemVnum(item) == 22000)
			{
				ecs::GetRecallPosition(ecs::PlayerRuntime::GetMapIndex(e), ecs::PlayerRuntime::GetEmpire(e), posWarp);

				if (GetItemSocket(item, 0) == 0)
				{
					x = posWarp.x - ecs::PlayerRuntime::GetX(e);
					y = posWarp.y - ecs::PlayerRuntime::GetY(e);
				}
				else
				{
					x = GetItemSocket(item, 0) - ecs::PlayerRuntime::GetX(e);
					y = GetItemSocket(item, 1) - ecs::PlayerRuntime::GetY(e);
				}
			}

			nDist = sqrt(pow((float)x, 2) + pow((float)y, 2));
			if (nDistant > nDist) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 433, "");
#endif
				return false;
			}
		}

		//PREVENT_PORTAL_AFTER_EXCHANGE
		//±³È¯ ÈÄ ½Ã°£Ã¼�
// ©
		if (iPulse - ExchangeSystem::GetLastExchangePulse(e) < PASSES_PER_SEC(g_nPortalLimitTime))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 234, "%d", g_nPortalLimitTime);
#endif
			return false;
		}
		//END_PREVENT_PORTAL_AFTER_EXCHANGE

	}

	//º¸µû¸® ºñ´Ü »ç¿ë½Ã °�
// ·¡Ã¢ Á¦ÇÑ Ã¼�
// ©
	if ((GetItemVnum(item) == 50200) || (GetItemVnum(item) == 71049)
#ifdef KASMIR_PAKET_SYSTEM
		|| (GetItemVnum(item) == 88901)
#endif
		)
	{
		if (ExchangeSystem::IsActive(e) || ecs::SocialSystem::GetMyShop(e) || ecs::SocialSystem::GetShopOwner(e) != entt::null || ecs::SessionSystem::IsSafeboxOpen(e) || ecs::SessionSystem::IsCubeOpen(e))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 237, "");
#endif
			return false;
		}

#ifdef __ATTR_TRANSFER_SYSTEM__
		if (AttrTransfer_is_open(e))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 237, "");
#endif
			return false;
		}
#endif
	}
	//END_PREVENT_TRADE_WINDOW

	if (IS_SET(GetItemFlags(item), ITEM_FLAG_LOG)) // »ç¿ë ·Î±×¸¦ ³²±â´Â ¾ÆÀÌ�
// Û Ã³¸®
	{
		uint32_t vid = GetItemVID(item);
		int oldCount = GetItemCount(item);
		uint32_t vnum = GetItemVnum(item);

		char hint[ITEM_NAME_MAX_LEN + 48 + 1];
		int len = snprintf(hint, sizeof(hint) - 48, "%s", GetItemName(item));

		if (len < 0 || len >= (int)sizeof(hint) - 48)
			len = (sizeof(hint) - 48) - 1;

		bool ret = UseItemEx(e, item, DestCell);

		if (nullptr == ITEM_MANAGER::instance().FindByVID(vid)) // UseItemEx¿¡¼­ ¾ÆÀÌ�
// ÛÀÌ »èÁ¦ µÇ¾ú´Ù. »èÁ¦ ·Î±×¸¦ ³²±è
		{
			LogManager::instance().ItemLog(e, vid, vnum, "REMOVE", hint);
		}
		else if (oldCount != GetItemCount(item))
		{
			snprintf(hint + len, sizeof(hint) - len, " %u", oldCount - 1);
			LogManager::instance().ItemLog(e, vid, vnum, "USE_ITEM", hint);
		}
		return (ret);
	}
	else
		return UseItemEx(e, item, DestCell);
}

} // namespace ItemSystem

namespace ItemSystem {

// Using an item: the switch over every usable item type. Both the
// character and the item come in as entities.
bool UseItemEx(entt::entity e, entt::entity item, TItemPos DestCell)
{
	// A handful of the item types still reach APIs that speak in CHARACTER
	// pointers - the stamina and SP consumption helpers, the battle pass
	// open, the sex macro and the multi-argument special item group. Each
	// is its own migration; they share one resolve here.
	LPCHARACTER self = ecs::LegacyCharOf(e);
	if (!self)
		return false;

	entt::entity itemEntity = item;
	int iLimitRealtimeStartFirstUseFlagIndex = -1;
	//int iLimitTimerBasedOnWearFlagIndex = -1;

	uint16_t wDestCell = DestCell.cell;
	uint8_t bDestInven = DestCell.window_type;
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		int32_t limitValue = GetItemProto(item)->aLimits[i].lValue;

		switch (GetItemProto(item)->aLimits[i].bType)
		{
		case LIMIT_LEVEL:
			if (ecs::PointSystem::GetLevel(e) < limitValue)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 325, "%d", limitValue);
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
		LOG_INFO("USE_ITEM {}, Inven {}, Cell {}, ItemType {}, SubType {}", GetItemName(item), bDestInven, wDestCell, GetItemType(item), GetItemSubType(item));
	}

	if (CArenaManager::instance().IsLimitedItem(ecs::PlayerRuntime::GetMapIndex(e), GetItemVnum(item)) == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
		return false;
	}
#ifdef ENABLE_NEWSTUFF
	else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && IsLimitedPotionOnPVP(GetItemVnum(item)))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
		return false;
	}
#endif

	// @fixme402 (IsLoadedAffect to block affect hacking)
	if (!AffectSystem::IsLoaded(e)) {
		return false;
	}

	// @fixme141 BEGIN
/* 	if (TItemPos(GetItemWindow(item), GetItemCell(itemEntity)).IsBeltInventoryPosition())// @Razor93 GetWear(WEAR_BELT); ne legyen szukseges a wear_mount_costume hez
	{
		const entt::entity beltItem = GetWearItem(e, WEAR_BELT);

		if (NULL == beltItem)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 785, "");
#endif
			return false;
		}

		if (false == CBeltInventoryHelper::IsAvailableCell(GetItemCell(itemEntity) - BELT_INVENTORY_SLOT_START, GetItemValue(beltItem, 0)))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 786, "");
#endif
			return false;
		}
	} */
	// @fixme141 END

	// ¾ÆÀÌ�
// Û ÃÖÃÊ »ç¿ë ÀÌÈÄºÎ�
// Í´Â »ç¿ëÇÏÁö ¾Ê¾Æµµ ½Ã°£ÀÌ Â÷°¨µÇ´Â ¹æ½Ä Ã³¸®.
	if (-1 != iLimitRealtimeStartFirstUseFlagIndex)
	{
		// ÇÑ ¹øÀÌ¶óµµ »ç¿ëÇÑ ¾ÆÀÌ�
// ÛÀÎÁö ¿©ºÎ´Â Socket1À» º¸°í ÆÇ´ÜÇÑ´Ù. (Socket1¿¡ »ç¿ëÈ½¼ö ±â·Ï)
		if (0 == GetItemSocket(item, 1))
		{
			// »ç¿ë°¡´É½Ã°£Àº Default °ªÀ¸·Î Limit Value °ªÀ» »ç¿ëÇÏµÇ, Socket0¿¡ °ªÀÌ ÀÖÀ¸¸é ±× °ªÀ» »ç¿ëÇÏµµ·Ï ÇÑ´Ù. (´ÜÀ§´Â ÃÊ)
			int32_t duration = (0 != GetItemSocket(item, 0)) ? GetItemSocket(item, 0) : GetItemProto(item)->aLimits[iLimitRealtimeStartFirstUseFlagIndex].lValue;

			if (0 == duration)
				duration = 60 * 60 * 24 * 7;

			SetItemSocket(item, 0, time(nullptr) + duration);
			StartRealTimeExpireEventEcs(itemEntity);
		}

		if (false == IsItemEquipped(itemEntity))
			SetItemSocket(item, 1, GetItemSocket(item, 1) + 1);
	}

#ifdef __NEWPET_SYSTEM__
	if (GetItemVnum(item) == 55001)
	{

		entt::entity item2 = entt::null;

		if (!InventorySystem::IsValidItemPosition(e, DestCell) || (item2 = GetItem(e, DestCell)) == entt::null)
			return false;

		if (IsItemExchanging(item2) || IsItemEquipped(item2)) // ENABLE_BUG_FIXES
			return false;

		if (GetItemVnum(item2) > 55711 || GetItemVnum(item2) < 55701)
			return false;


		char szQuery1[1024];
		snprintf(szQuery1, sizeof(szQuery1), "SELECT duration FROM new_petsystem WHERE id = %d LIMIT 1", GetItemID(item2));
		std::unique_ptr<SQLMsg> pmsg2(DBManager::instance().DirectQuery(szQuery1));
		if (pmsg2->Get()->uiNumRows > 0) {
			MYSQL_ROW row = mysql_fetch_row(pmsg2->Get()->pSQLResult);
			if (atoi(row[0]) > 0) {
				if (ecs::PlayerRuntime::GetNewPetSystem(e)->IsActivePet()) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 787, "");
#endif
					return false;
				}

				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE new_petsystem SET duration =(tduration) WHERE id = %d", GetItemID(item2)));
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 788, "");
#endif
			}
			else {
				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE new_petsystem SET duration =(tduration/2) WHERE id = %d", GetItemID(item2)));
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 788, "");
#endif
			}
			ConsumeItemEcs(itemEntity);
			return true;
		}
		else
			return false;
	}

	if (GetItemVnum(item) >= 55701 && GetItemVnum(item) <= 55711) {
		const entt::entity box = GetItem(e, DestCell);
		if (box != entt::null) {
			if (GetItemSocket(item, 1) == 0) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 858, "");
#endif
				return false;
			}

			if (GetItemSocket(box, 0) != 0) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 853, "%s", GetItemName(box));
#endif
				return false;
			}
			else {
				if (GetItemSocket(item, 0) == true) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 854, "");
#endif
					return false;
				}
				else {
					char query[1024];
					snprintf(query, sizeof(query), "SELECT level"
#ifdef ENABLE_NEW_PET_EDITS
						", minAge "
#endif
						", evolution, bonus0, bonus1, bonus2, skill0, skill0lv, skill1, skill1lv, skill2, skill2lv, skill3, skill3lv FROM player.new_petsystem WHERE id = %d", GetItemID(item));
					std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(query));
					if (pmsg->Get()->uiNumRows > 0)
					{
						MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);
						uint32_t evolution = atoi(row[2]);
						uint32_t petVnum = 0;
						switch (GetItemVnum(item)) {
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

						SetItemSocket(box, 1, GetItemID(item));
						SetItemSocket(box, 0, petVnum);
						ITEM_MANAGER::instance().RemoveItem(itemEntity);
#ifdef ENABLE_NEW_PET_EDITS
						SetItemSocket(box, 2, atoi(row[1]));
#endif
						uint8_t res1 = atoi(row[0]);
						uint8_t res2 = atoi(row[2]);
						uint8_t res3 = atoi(row[3]);
						uint8_t res4 = atoi(row[4]);
						SetItemForceAttributeEcs(box, 0, res1, res2);
						SetItemForceAttributeEcs(box, 1, res3, res4);
						uint8_t dwskill1 = atoi(row[6]) == -1 ? 255 : atoi(row[6]), dwskilllv1 = atoi(row[7]);
						SetItemForceAttributeEcs(box, 2, atoi(row[5]), dwskill1);
						uint8_t dwskill2 = atoi(row[8]) == -1 ? 255 : atoi(row[8]), dwskilllv2 = atoi(row[9]);
						SetItemForceAttributeEcs(box, 3, dwskilllv1, dwskill2);
						uint8_t dwskill3 = atoi(row[10]) == -1 ? 255 : atoi(row[10]), dwskilllv3 = atoi(row[11]);
						SetItemForceAttributeEcs(box, 4, dwskilllv2, dwskill3);
						uint8_t dwskill4 = atoi(row[12]) == -1 ? 255 : atoi(row[12]), dwskilllv4 = atoi(row[13]);
						SetItemForceAttributeEcs(box, 5, dwskilllv3, dwskill4);
						SetItemForceAttributeEcs(box, 6, dwskilllv4, 1);
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 855, "%s", GetItemName(box));
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
	else if (GetItemVnum(item) == 55002) {
		if (GetItemSocket(item, 0) != 0) {
			uint32_t itemVnum = 0;
			switch (GetItemSocket(item, 0)) {
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

			const entt::entity petItem = AutoGiveItemEcs(e, itemVnum, 1);
			if (!IsValidItem(petItem)) {
				return false;
			}

			SetItemSocket(petItem, 0, 0);
			SetItemForceAttributeEcs(petItem, 0, 1, GetItemAttributeType(item, 1));
			SetItemForceAttributeEcs(petItem, 1, 1, GetItemAttributeValue(item, 1));
			SetItemForceAttributeEcs(petItem, 2, 1, GetItemAttributeType(item, 2));

			char query[256];
			snprintf(query, sizeof(query), "SELECT tduration FROM player.new_petsystem WHERE id = %ld", GetItemSocket(item, 1));
			std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(query));
			if (pmsg->Get()->uiNumRows > 0) {
				MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);
#ifdef ENABLE_NEW_PET_EDITS
				SetItemSocket(petItem, 1, atoi(row[0]));
				SetItemSocket(petItem, 2, atoi(row[0]));
#else
				SetItemForceAttributeEcs(petItem, 3, 1, atoi(row[0]));
				SetItemForceAttributeEcs(petItem, 4, 1, atoi(row[0]));
#endif
			}
#ifdef ENABLE_NEW_PET_EDITS
			SetItemForceAttributeEcs(petItem, 3, 1, GetItemAttributeType(item, 0));
#else
			SetItemSocket(petItem, 1, GetItemAttributeType(item, 0));
#endif
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE player.new_petsystem SET id = %d WHERE id = %ld", GetItemID(petItem), GetItemSocket(item, 1)));
			ITEM_MANAGER::instance().RemoveItem(itemEntity);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 857, "%s", GetItemName(item));
			return true;
#endif
		}
		else {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 856, "%s", GetItemName(item));
#endif
			return false;
		}
	}
#endif

	// 30001: Teszt klonok torlese (dungeon instance-ben blokkolva)
	//switch (GetItemVnum(item))
	//{
	//	case 30001: return CLostCastleDungeon::instance().OnUseItem30001(e);
	//	default: break;
	//}

#ifdef ENABLE_CPP_DUNGEON_RAZOR93
	switch (GetItemVnum(item))
	{
	case 89103: return CRuneDungeon::instance().OnUseItem89103(e);
	case 89102: return CRuneDungeon::instance().OnUseItem89102(e);
	case 89100: return CRuneDungeon::instance().OnUseItem89100(e);
	default: break;
	}
#endif

	switch (GetItemType(item))
	{
#ifdef ENABLE_ITEMSHOP_ITEM
	case ITEM_TYPE_ISHOP:
	{
		uint32_t vnum = GetItemSocket(item, 0);
		if (vnum == 0) {
			return false;
		}

		const entt::entity reward = AutoGiveItemEcs(e, vnum, 1);
		if (!IsValidItem(reward)) {
			return false;
		}

		ConsumeItemEcs(itemEntity);
		return true;
	}
	break;
#endif
	case ITEM_HAIR:
		return ItemProcess_Hair(e, item, wDestCell);

	case ITEM_POLYMORPH:
		return ItemProcess_Polymorph(e, item);

	case ITEM_QUEST:
		if (ecs::PlayerRuntime::GetArena(e) != nullptr || ecs::PlayerRuntime::IsObserverMode(e) == true)
		{
			if (GetItemVnum(item) == 50051 || GetItemVnum(item) == 50052 || GetItemVnum(item) == 50053)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
		}

		if (!IS_SET(GetItemFlags(item), ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE))
		{
			if (GetItemSIGVnum(item) == 0)
			{
				quest::CQuestManager::instance().UseItem(ecs::PlayerRuntime::GetPlayerID(e), itemEntity, false);
			}
			else
			{
				quest::CQuestManager::instance().SIGUse(ecs::PlayerRuntime::GetPlayerID(e), GetItemSIGVnum(item), itemEntity, false);
			}
		}

#ifdef __AUTO_QUQUE_ATTACK__
		if (GetItemVnum(item) >= 61400 && GetItemVnum(item) <= 61405)
		{
			if (IsItemLocked(item) || IsItemExchanging(item))
				return false;

			if (AffectSystem::FindAffect(e, AFFECT_AUTO_METIN_FARM)) {
				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "You has already affect.");
				return false;
			}
			ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Affect successfully added.");
			AffectSystem::AddAffect(e, AFFECT_AUTO_METIN_FARM, 0, 0, AFF_NONE, GetItemValue(item, 0) == 999 ? INFINITE_AFFECT_DURATION : 60 * 60 * 24 * GetItemValue(item, 0), 0, false);
			ConsumeItemEcs(itemEntity);
			return true;
		}
#endif
		break;

	case ITEM_CAMPFIRE:
	{
		float fx, fy;
		GetDeltaByDegree(ecs::PlayerRuntime::GetRotation(e), 100.0f, &fx, &fy);

		LPSECTREE tree = ecs::SectorAt(ecs::PlayerRuntime::GetMapIndex(e), (int32_t)(ecs::PlayerRuntime::GetX(e) + fx), (int32_t)(ecs::PlayerRuntime::GetY(e) + fy));

		if (!tree)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 344, "");
#endif
			return false;
		}

		if (tree->IsAttr((int32_t)(ecs::PlayerRuntime::GetX(e) + fx), (int32_t)(ecs::PlayerRuntime::GetY(e) + fy), ATTR_WATER))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 346, "");
#endif
			return false;
		}

#ifdef ENABLE_BUG_FIXES
		if (get_global_time() - ecs::PlayerRuntime::GetQuestFlag(e, "kamp.spawned") < 60) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 1246, "");
#endif
			return false;
		}
		else {
			ecs::PlayerRuntime::SetQuestFlag(e, "kamp.spawned", get_global_time());
		}
#endif

		auto* campfire = CHARACTER_MANAGER::instance().SpawnMob(fishing::CAMPFIRE_MOB, ecs::PlayerRuntime::GetMapIndex(e), (int32_t)(ecs::PlayerRuntime::GetX(e) + fx), (int32_t)(ecs::PlayerRuntime::GetY(e) + fy), 0, false, number(0, 359));

		char_event_info* info = AllocEventInfo<char_event_info>();

		info->ch = campfire->GetEntityHandle();

		campfire->m_pkMiningEvent = event_create(kill_campfire_event, info, PASSES_PER_SEC(40));

		ConsumeItemEcs(itemEntity);
	}
	break;

	case ITEM_UNIQUE:
	{
		switch (GetItemSubType(item))
		{
		case USE_ABILITY_UP:
		{
			switch (GetItemValue(item, 0))
			{
			case APPLY_MOV_SPEED:
				AffectSystem::AddAffect(e, AFFECT_UNIQUE_ABILITY, POINT_MOV_SPEED, GetItemValue(item, 2), AFF_MOV_SPEED_POTION, GetItemValue(item, 1), 0, true, true);
				break;

			case APPLY_ATT_SPEED:
				AffectSystem::AddAffect(e, AFFECT_UNIQUE_ABILITY, POINT_ATT_SPEED, GetItemValue(item, 2), AFF_ATT_SPEED_POTION, GetItemValue(item, 1), 0, true, true);
				break;

			case APPLY_STR:
				AffectSystem::AddAffect(e, AFFECT_UNIQUE_ABILITY, POINT_ST, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true, true);
				break;

			case APPLY_DEX:
				AffectSystem::AddAffect(e, AFFECT_UNIQUE_ABILITY, POINT_DX, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true, true);
				break;

			case APPLY_CON:
				AffectSystem::AddAffect(e, AFFECT_UNIQUE_ABILITY, POINT_HT, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true, true);
				break;

			case APPLY_INT:
				AffectSystem::AddAffect(e, AFFECT_UNIQUE_ABILITY, POINT_IQ, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true, true);
				break;

			case APPLY_CAST_SPEED:
				AffectSystem::AddAffect(e, AFFECT_UNIQUE_ABILITY, POINT_CASTING_SPEED, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true, true);
				break;

			case APPLY_RESIST_MAGIC:
				AffectSystem::AddAffect(e, AFFECT_UNIQUE_ABILITY, POINT_RESIST_MAGIC, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true, true);
				break;

			case APPLY_ATT_GRADE_BONUS:
				AffectSystem::AddAffect(e, AFFECT_UNIQUE_ABILITY, POINT_ATT_GRADE_BONUS,
					GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true, true);
				break;

			case APPLY_DEF_GRADE_BONUS:
				AffectSystem::AddAffect(e, AFFECT_UNIQUE_ABILITY, POINT_DEF_GRADE_BONUS,
					GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true, true);
				break;
			}
		}

		if (ecs::SocialSystem::GetWarMap(e))
			ecs::SocialSystem::GetWarMap(e)->UsePotion(e, itemEntity);

		ConsumeItemEcs(itemEntity);
		break;

		default:
		{
			if (GetItemSubType(item) == USE_SPECIAL)
			{
				LOG_INFO("ITEM_UNIQUE: USE_SPECIAL {}", GetItemVnum(item));

				switch (GetItemVnum(item))
				{
				case 71049: // ºñ´Üº¸µû¸®
#ifdef KASMIR_PAKET_SYSTEM
				case 88901:
#endif
					if (g_bEnableBootaryCheck)
					{
						if (IS_BOTARYABLE_ZONE(ecs::PlayerRuntime::GetMapIndex(e)) == true)
						{
#ifdef KASMIR_PAKET_SYSTEM
							ecs::SocialSystem::SetKasmirPaket(e, GetItemVnum(item) == 88901 ? true : false);
#endif

							ecs::SocialSystem::UseSilkBotary(e);
						}
#ifdef TEXTS_IMPROVEMENT
						else {
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 668, "");
						}
#endif
					}
					else
					{
#ifdef KASMIR_PAKET_SYSTEM
						ecs::SocialSystem::SetKasmirPaket(e, GetItemVnum(item) == 88901 ? true : false);
#endif

						ecs::SocialSystem::UseSilkBotary(e);
					}
					break;
				}
			}
			else
			{
				if (!IsItemEquipped(item))
					EquipItemEcs(e, item);
				else
					UnequipItemEcs(e, item);
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
	case ITEM_RING:		// ½�
// ±Ô ¹ÝÁö ¾ÆÀÌ�
// Û
	case ITEM_BELT:		// ½�
// ±Ô º§Æ® ¾ÆÀÌ�
// Û
		//ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "You can put in your Mount inventory");
		// MINING
	case ITEM_PICK:
		// END_OF_MINING
		if (!IsItemEquipped(item))
			EquipItemEcs(e, item);
		else
			UnequipItemEcs(e, item);
		break;
		// Âø¿ëÇÏÁö ¾ÊÀº ¿ëÈ¥¼®Àº »ç¿ëÇÒ ¼ö ¾ø´Ù.
		// Á¤»óÀûÀÎ �
// ¬¶ó¶ó¸é, ¿ëÈ¥¼®¿¡ °üÇÏ¿© item use ÆÐ�
// ¶À» º¸³¾ ¼ö ¾ø´Ù.
		// ¿ëÈ¥¼® Âø¿ëÀº item move ÆÐ�
// ¶À¸·Î ÇÑ´Ù.
		// Âø¿ëÇÑ ¿ëÈ¥¼®Àº ÃßÃâÇÑ´Ù.
	case ITEM_DS:
	{
		if (!IsItemEquipped(item))
			return false;
		return DSManager::instance().PullOutEcs(e, NPOS, itemEntity);
		break;
	}
	case ITEM_SPECIAL_DS:
		if (!IsItemEquipped(item))
			EquipItemEcs(e, item);
		else
			UnequipItemEcs(e, item);
		break;

	case ITEM_FISH:
	{
		if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
			return false;
		}
#ifdef ENABLE_NEWSTUFF
		else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && !IsAllowedPotionOnPVP(GetItemVnum(item)))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
			return false;
		}
#endif

		if (GetItemSubType(item) == FISH_ALIVE)
			fishing::UseFishEcs(e, itemEntity);
	}
	break;

	case ITEM_TREASURE_BOX:
	{
		return false;
	}
	break;

	case ITEM_TREASURE_KEY:
	{
		entt::entity item2 = entt::null;

		if (GetItem(e, DestCell) == entt::null || (item2 = GetItem(e, DestCell)) == entt::null)
			return false;

		if (IsItemExchanging(item2) || IsItemEquipped(item2)) // @fixme114
			return false;

		if (GetItemType(item2) != ITEM_TREASURE_BOX)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 408, "");
#endif
			return false;
		}

		if (GetItemValue(item, 0) == GetItemValue(item2, 0))
		{
			uint32_t dwBoxVnum = GetItemVnum(item2);
			std::vector <uint32_t> dwVnums;
			std::vector <uint32_t> dwCounts;
			std::vector<entt::entity> item_gets;
			int count = 0;

			if (self->GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
			{
				ITEM_MANAGER::instance().RemoveItem(itemEntity);
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
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 378, "");
#endif
						break;
					case CSpecialItemGroup::SLOW:
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 377, "");
#endif
						break;
					case CSpecialItemGroup::DRAIN_HP:
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 373, "");
#endif
						break;
					case CSpecialItemGroup::POISON:
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 376, "");
#endif
						break;
					case CSpecialItemGroup::MOB_GROUP:
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 380, "");
#endif
						break;
					default:
						//#ifdef TEXTS_IMPROVEMENT
						//									if (item_gets[i]) {
						//										if (dwCounts[i] > 1) {
						//											ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 374, "%d#%s", dwCounts[i], item_gets[i]->GetName());
						//										} else {
						//											ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 375, "%s", item_gets[i]->GetName());
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
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 408, "");
#endif
				return false;
			}
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 408, "");
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
			if (get_dword_time() < ecs::PlayerRuntime::GetBoxUseTime(e) + g_BoxUseTimeLimitValue)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 510, "");
#endif
				return false;
			}
		}

		ecs::PlayerRuntime::SetBoxUseTime(e, get_dword_time());
#endif
		uint32_t dwBoxVnum = GetItemVnum(item);

		std::vector <uint32_t> dwVnums;
		std::vector <uint32_t> dwCounts;
		std::vector<entt::entity> item_gets;
		int count = 0;

		if (self->GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
		{
			ConsumeItemEcs(itemEntity);
#ifdef ENABLE_RANKING
			ecs::PlayerRuntime::SetRankPoints(e, 17, ecs::PlayerRuntime::GetRankPoints(e, 17) + 1);
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
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 378, "");
#endif
					break;
				case CSpecialItemGroup::SLOW:
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 377, "");
#endif
					break;
				case CSpecialItemGroup::DRAIN_HP:
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 373, "");
#endif
					break;
				case CSpecialItemGroup::POISON:
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 376, "");
#endif
					break;
				case CSpecialItemGroup::MOB_GROUP:
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 380, "");
#endif
					break;
				default:
					//#ifdef TEXTS_IMPROVEMENT
					//							if (item_gets[i]) {
					//								if (dwCounts[i] > 1) {
					//									ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 374, "%d#%s", dwCounts[i], item_gets[i]->GetName());
					//								} else {
					//									ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 375, "%s", item_gets[i]->GetName());
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
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 395, "");
#endif
			return false;
		}
	}
	break;

	case ITEM_SKILLFORGET:
	{
		if (!GetItemSocket(item, 0))
		{
			ITEM_MANAGER::instance().RemoveItem(itemEntity);
			return false;
		}

		uint32_t dwVnum = GetItemSocket(item, 0);

		if (SkillSystem::SkillLevelDown(e, dwVnum)) {
			ITEM_MANAGER::instance().RemoveItem(itemEntity);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 399, "");
#endif
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 400, "");
		}
#endif
	}
	break;

	case ITEM_SKILLBOOK:
	{
		if (GetItemVnum(item) == 55003 || GetItemVnum(item) == 55004 || GetItemVnum(item) == 55005) {
			return false;
		}

		if (AffectSystem::IsPolymorphed(e))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 313, "");
#endif
			return false;
		}

		uint32_t dwVnum = 0;
		if (GetItemVnum(item) == 50300)
		{
			dwVnum = GetItemSocket(item, 0);
		}
		else
		{
			dwVnum = GetItemValue(item, 0);
		}

		dwVnum = GetItemVnum(item) == 50301 || GetItemVnum(item) == 50302 || GetItemVnum(item) == 50303 ? SKILL_LEADERSHIP : dwVnum;

		if (0 == dwVnum)
		{
			ITEM_MANAGER::instance().RemoveItem(itemEntity);

			return false;
		}

		if (dwVnum == SKILL_LEADERSHIP) {
			int lv = SkillSystem::GetSkillLevel(e, SKILL_LEADERSHIP);
			if (lv < GetItemValue(item, 0)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 429, "");
#endif
				return false;
			}

			if (lv >= GetItemValue(item, 1)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 430, "");
#endif
				return false;
			}
		}

		if (true == SkillSystem::LearnSkillByBook(e, dwVnum))
		{
#ifdef ENABLE_BOOKS_STACKFIX
			ConsumeItemEcs(itemEntity);
#else
			ITEM_MANAGER::instance().RemoveItem(itemEntity);
#endif
			int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
			SkillSystem::SetSkillNextReadTime(e, dwVnum, dwVnum == SKILL_LEADERSHIP ? get_global_time() + 18000 : get_global_time() + iReadDelay);
		}
	}
	break;
#ifdef ENABLE_NEW_PET_EDITS
	case ITEM_TYPE_PET:
	{
		if (!ecs::PlayerRuntime::GetNewPetSystem(e))
			return false;

		if (ecs::PlayerRuntime::GetNewPetSystem(e)->IsActivePet()) {
			ecs::PlayerRuntime::GetNewPetSystem(e)->IncreasePetSkillByBook(itemEntity);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 53, "");
		}
#endif
	}
	break;
#endif
	case ITEM_USE:
	{
		if (GetItemVnum(item) > 50800 && GetItemVnum(item) <= 50820)
		{
			if (test_server)
				LOG_INFO("ADD addtional effect : vnum({}) subtype({})", GetItemOriginalVnum(item), GetItemSubType(item));

			int affect_type = AFFECT_EXP_BONUS_EURO_FREE;
			int apply_type = aApplyInfo[GetItemValue(item, 0)].bPointType;
			int apply_value = GetItemValue(item, 2);
			int apply_duration = GetItemValue(item, 1);

			switch (GetItemSubType(item))
			{
			case USE_ABILITY_UP:
				if (AffectSystem::FindAffect(e, affect_type, apply_type))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 442, "");
#endif
					return false;
				}

				{
					switch (GetItemValue(item, 0))
					{
					case APPLY_MOV_SPEED:
						AffectSystem::AddAffect(e, affect_type, apply_type, apply_value, AFF_MOV_SPEED_POTION, apply_duration, 0, true, true);
						break;

					case APPLY_ATT_SPEED:
						AffectSystem::AddAffect(e, affect_type, apply_type, apply_value, AFF_ATT_SPEED_POTION, apply_duration, 0, true, true);
						break;

					case APPLY_STR:
					case APPLY_DEX:
					case APPLY_CON:
					case APPLY_INT:
					case APPLY_CAST_SPEED:
					case APPLY_RESIST_MAGIC:
					case APPLY_ATT_GRADE_BONUS:
					case APPLY_DEF_GRADE_BONUS:
						AffectSystem::AddAffect(e, affect_type, apply_type, apply_value, 0, apply_duration, 0, true, true);
						break;
					}
				}

				if (ecs::SocialSystem::GetWarMap(e))
					ecs::SocialSystem::GetWarMap(e)->UsePotion(e, itemEntity);

				ConsumeItemEcs(itemEntity);
				break;

			case USE_AFFECT:
			{
				if (AffectSystem::FindAffect(e, AFFECT_EXP_BONUS_EURO_FREE, aApplyInfo[GetItemValue(item, 1)].bPointType))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 442, "");
#endif
				}
				else
				{
					// PC_BANG_ITEM_ADD
					if (IsItemPCBangItem(item) == true)
					{
						// PC¹æÀÎÁö Ã¼�
// ©ÇØ¼­ Ã³¸®
						if (CPCBangManager::instance().IsPCBangIP(ecs::PlayerRuntime::GetDesc(e)->GetHostName()) == false)
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 426, "");
#endif
							return false;
						}
					}
					// END_PC_BANG_ITEM_ADD

					AffectSystem::AddAffect(e, AFFECT_EXP_BONUS_EURO_FREE, aApplyInfo[GetItemValue(item, 1)].bPointType, GetItemValue(item, 2), 0, GetItemValue(item, 3), 0, false, true);
					ConsumeItemEcs(itemEntity);
				}
			}
			break;
			case USE_POTION_NODELAY:
			{
				if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
				{
					if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 303, "");
#endif
						return false;
					}

					switch (GetItemVnum(item))
					{
					case 70020:
					case 71018:
					case 71019:
					case 71020:
						if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
						{
							if (ecs::PlayerRuntime::GetPotionLimit(e) <= 0)
							{
#ifdef TEXTS_IMPROVEMENT
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 362, "");
#endif
								return false;
							}
						}
						break;

					default:
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 303, "");
#endif
						return false;
						break;
					}
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && !IsAllowedPotionOnPVP(GetItemVnum(item)))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				bool used = false;

				if (GetItemValue(item, 0) != 0) // HP Àý´ë°ª È¸º¹
				{
					if (ecs::PlayerRuntime::GetHP(e) < ecs::PointSystem::GetMaxHP(e))
					{
						ecs::PointSystem::Change(e, POINT_HP, GetItemValue(item, 0) * (100 + ecs::PointSystem::Get(e, POINT_POTION_BONUS)) / 100);
						NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_HPUP_RED);
						used = true;
					}
				}

				if (GetItemValue(item, 1) != 0)	// SP Àý´ë°ª È¸º¹
				{
					if (ecs::PlayerRuntime::GetSP(e) < ecs::PointSystem::GetMaxSP(e))
					{
						ecs::PointSystem::Change(e, POINT_SP, GetItemValue(item, 1) * (100 + ecs::PointSystem::Get(e, POINT_POTION_BONUS)) / 100);
						NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_SPUP_BLUE);
						used = true;
					}
				}

				if (GetItemValue(item, 3) != 0) // HP % È¸º¹
				{
					if (ecs::PlayerRuntime::GetHP(e) < ecs::PointSystem::GetMaxHP(e))
					{
						ecs::PointSystem::Change(e, POINT_HP, GetItemValue(item, 3) * ecs::PointSystem::GetMaxHP(e) / 100);
						NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_HPUP_RED);
						used = true;
					}
				}

				if (GetItemValue(item, 4) != 0) // SP % È¸º¹
				{
					if (ecs::PlayerRuntime::GetSP(e) < ecs::PointSystem::GetMaxSP(e))
					{
						ecs::PointSystem::Change(e, POINT_SP, GetItemValue(item, 4) * ecs::PointSystem::GetMaxSP(e) / 100);
						NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_SPUP_BLUE);
						used = true;
					}
				}

				if (used)
				{
					if (GetItemVnum(item) == 50085 || GetItemVnum(item) == 50086) {
						ecs::PlayerRuntime::SetUseSeedOrMoonBottleTime(e);
					}

					if (ecs::SocialSystem::GetWarMap(e))
						ecs::SocialSystem::GetWarMap(e)->UsePotion(e, itemEntity);

					ecs::PlayerRuntime::SetPotionLimit(e, ecs::PlayerRuntime::GetPotionLimit(e) - 1);

					//RESTRICT_USE_SEED_OR_MOONBOTTLE
					ConsumeItemEcs(itemEntity);
					//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
				}
			}
			break;
			}

			return true;
		}


		if (GetItemVnum(item) >= 27863 && GetItemVnum(item) <= 27883)
		{
			if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && !IsAllowedPotionOnPVP(GetItemVnum(item)))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif
		}

		if (test_server)
		{
			LOG_INFO("USE_ITEM {} Type {} SubType {} vnum {}", GetItemName(item), GetItemType(item), GetItemSubType(item), GetItemOriginalVnum(item));
		}

		switch (GetItemSubType(item))
		{
		case USE_FISH:
		{
			CAffect* pAffect = nullptr;
			int type = 0, duration = GetItemValue(item, 0);
			for (int i = 0; i < ITEM_APPLY_MAX_NUM; i++) {
				type = aApplyInfo[GetItemApplyType(item, i)].bPointType;
				if (type != 0) {
					pAffect = AffectSystem::FindAffect(e, AFFECT_FISH_BONUS, type);
				}
			}

			if (pAffect != nullptr) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 893, "");
#endif
				return false;
			}
			else {
				ConsumeItemEcs(itemEntity);

				for (int i = 0; i < ITEM_APPLY_MAX_NUM; i++) {
					type = GetItemApplyType(item, i);
					if (type != 0) {
						AffectSystem::AddAffect(e, AFFECT_FISH_BONUS, aApplyInfo[type].bPointType, GetItemApplyValue(item, i), GetItemID(item), duration, 0, false, false);
					}
				}
			}
			break;
		}
		case USE_TIME_CHARGE_PER:
		{
			const entt::entity pDestItem = GetItem(e, DestCell);
			if (pDestItem == entt::null)
			{
				return false;
			}
			// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
			if (IsDragonSoulItem(pDestItem))
			{
#ifdef ENABLE_DS_POTION_DIFFRENT
				if (GetItemCount(item) > 1) {
					int pos = InventorySystem::GetEmptyInventory(e, GetItemSize(item));
					if (pos == -1) {
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 366, "");
#endif
						return false;
					}

					ConsumeItemEcs(itemEntity);
					const entt::entity item2 = ITEM_MANAGER::instance().CreateItem(GetItemVnum(item), 1);
					if (!IsValidItem(item2))
						return false;

					InventorySystem::AddToCharacter(item2, e, TItemPos(INVENTORY, pos), false);
					itemEntity = item2;
				}

				if (GetItemSocket(item, 0) <= 0) {
					InventorySystem::RemoveFromCharacter(item);
					return false;
				}
				else {
					uint32_t duration = DSManager::instance().GetDuration(pDestItem);
					uint32_t remain_sec = GetItemSocket(pDestItem, ITEM_SOCKET_REMAIN_SEC);
					if (remain_sec == duration)
						return false;

					uint32_t dwBottlePercent = GetItemSocket(item, 0);
					uint32_t dwOnePercent = duration / 100;
					uint32_t dwRemainPercent = remain_sec / dwOnePercent;
					uint32_t dif = 100 - dwRemainPercent;
					dif = dif > dwBottlePercent ? dwBottlePercent : dif;
					uint32_t add = dwOnePercent * dif;
					if (remain_sec + add >= 86400) {
						SetItemSocket(pDestItem, ITEM_SOCKET_REMAIN_SEC, duration);
					}
					else {
						SetItemSocket(pDestItem, ITEM_SOCKET_REMAIN_SEC, remain_sec + add);
					}

					SetItemSocket(item, 0, dwBottlePercent - dif);
					if (GetItemSocket(item, 0) < 1)
						InventorySystem::RemoveFromCharacter(item);

					return true;
				}
#else
				int ret;
				char buf[128];
				if (GetItemVnum(item) == DRAGON_HEART_VNUM)
				{
					ret = GiveMoreTime_Per(pDestItem, (float)GetItemSocket(item, ITEM_SOCKET_CHARGING_AMOUNT_IDX));
				}
				else
				{
					ret = GiveMoreTime_Per(pDestItem, (float)GetItemValue(item, ITEM_VALUE_CHARGING_AMOUNT_IDX));
				}
				if (ret > 0)
				{
					if (GetItemVnum(item) == DRAGON_HEART_VNUM)
					{
						sprintf(buf, "Inc %ds by item{VN:%d SOC%d:%ld}", ret, GetItemVnum(item), ITEM_SOCKET_CHARGING_AMOUNT_IDX, GetItemSocket(item, ITEM_SOCKET_CHARGING_AMOUNT_IDX));
					}
					else
					{
						sprintf(buf, "Inc %ds by item{VN:%d VAL%d:%ld}", ret, GetItemVnum(item), ITEM_VALUE_CHARGING_AMOUNT_IDX, GetItemValue(item, ITEM_VALUE_CHARGING_AMOUNT_IDX));
					}

#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 670, "%s#%d", GetItemName(pDestItem), ret);
#endif
					ConsumeItemEcs(itemEntity);
					LogManager::instance().ItemLogEntity(e, item, "DS_CHARGING_SUCCESS", buf);
					return true;
				}
				else
				{
					if (GetItemVnum(item) == DRAGON_HEART_VNUM)
					{
						sprintf(buf, "No change by item{VN:%d SOC%d:%ld}", GetItemVnum(item), ITEM_SOCKET_CHARGING_AMOUNT_IDX, GetItemSocket(item, ITEM_SOCKET_CHARGING_AMOUNT_IDX));
					}
					else
					{
						sprintf(buf, "No change by item{VN:%d VAL%d:%ld}", GetItemVnum(item), ITEM_VALUE_CHARGING_AMOUNT_IDX, GetItemValue(item, ITEM_VALUE_CHARGING_AMOUNT_IDX));
					}

#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 671, "%s", GetItemName(pDestItem));
#endif
					LogManager::instance().ItemLogEntity(e, item, "DS_CHARGING_FAILED", buf);
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
			const entt::entity pDestItem = GetItem(e, DestCell);
			if (pDestItem == entt::null)
			{
				return false;
			}
			// ¿ì¼± ¿ëÈ¥¼®¿¡ °üÇØ¼­¸¸ ÇÏµµ·Ï ÇÑ´Ù.
			if (IsDragonSoulItem(pDestItem))
			{
				int ret = GiveMoreTime_Fix(pDestItem, GetItemValue(item, ITEM_VALUE_CHARGING_AMOUNT_IDX));
				char buf[128];
				if (ret)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 670, "%s#%d", GetItemName(pDestItem), ret);
#endif
					sprintf(buf, "Increase %ds by item{VN:%d VAL%d:%ld}", ret, GetItemVnum(item), ITEM_VALUE_CHARGING_AMOUNT_IDX, GetItemValue(item, ITEM_VALUE_CHARGING_AMOUNT_IDX));
					LogManager::instance().ItemLogEntity(e, item, "DS_CHARGING_SUCCESS", buf);
					ConsumeItemEcs(itemEntity);
					return true;
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 671, "%s", GetItemName(pDestItem));
#endif
					sprintf(buf, "No change by item{VN:%d VAL%d:%ld}", GetItemVnum(item), ITEM_VALUE_CHARGING_AMOUNT_IDX, GetItemValue(item, ITEM_VALUE_CHARGING_AMOUNT_IDX));
					LogManager::instance().ItemLogEntity(e, item, "DS_CHARGING_FAILED", buf);
					return false;
				}
			}
			else
				return false;
		}
		break;
#ifdef ENABLE_NEW_USE_POTION
		case USE_NEW_POTIION: {
			uint32_t dwType = GetItemValue(item, 0);
			if (dwType >= AFFECT_NEW_POTION24 && dwType <= AFFECT_NEW_POTION29 && !marriage::CManager::instance().IsMarried(ecs::PlayerRuntime::GetPlayerID(e))) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 891, "");
#endif
				return false;
			}

			if (dwType == AFFECT_NEW_POTION31) {
				LPPARTY party = ecs::SocialSystem::GetParty(e);
				if ((!party) || (party && ecs::PlayerRuntime::GetPlayerID(e) != party->GetLeaderPID())) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 902, "");
#endif
					return false;
				}
			}

			CAffect* pAffect = AffectSystem::FindAffect(e, dwType);
			if (pAffect && GetItemID(item) != pAffect->dwFlag)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 442, "");
#else
				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Már aktiv egy ilyen harmat.");
#endif
				return false;
			}

			if (GetItemCount(item) > 1)
			{
#ifdef ENABLE_EXTRA_INVENTORY
				const bool bFromExtraInventory = (GetItemWindow(item) == EXTRA_INVENTORY);
#else
				const bool bFromExtraInventory = false;
#endif
				int pos = -1;

#ifdef ENABLE_EXTRA_INVENTORY
				if (bFromExtraInventory)
					pos = GetEmptyExtraInventory(e, item);
				else
#endif
					pos = InventorySystem::GetEmptyInventory(e, GetItemSize(item));

				if (pos == -1) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 366, "");
#endif
					break;
				}

				ConsumeItemEcs(itemEntity);
				const entt::entity item2 = ITEM_MANAGER::instance().CreateItem(GetItemVnum(item), 1);
				if (!IsValidItem(item2))
					return true;

#ifdef ENABLE_EXTRA_INVENTORY
				if (bFromExtraInventory)
					InventorySystem::AddToCharacter(item2, e, TItemPos(EXTRA_INVENTORY, pos), false);
				else
#endif
					InventorySystem::AddToCharacter(item2, e, TItemPos(INVENTORY, pos), false);

				itemEntity = item2;
			}

			uint8_t bApplyOn = GetItemApplyType(item, 0);
			int32_t lApplyValue = GetItemApplyValue(item, 0);

			pAffect = AffectSystem::FindAffect(e, dwType);
			if (pAffect) {
				uint32_t dwItemID = pAffect->dwFlag;
				if (GetItemID(item) == dwItemID) {
					LockItem(item, false);
					SetItemSocketEcs(itemEntity, 1, 0);
					AffectSystem::RemoveAffect(e, dwType);
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 28, "%s", GetItemName(item));
#endif
				}
				else {
					const entt::entity pkItem = FindItemByID(e, dwItemID);
					if (pkItem != entt::null) {
						LockItem(pkItem, false);
						SetItemSocketEcs(pkItem, 1, 0);
					}

					AffectSystem::RemoveAffect(e, dwType);
					LockItem(item, true);
					SetItemSocketEcs(itemEntity, 1, 1);
					AffectSystem::AddAffect(e, dwType, aApplyInfo[bApplyOn].bPointType, lApplyValue, GetItemID(item), INFINITE_AFFECT_DURATION, 0, true, false);
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 29, "%s", GetItemName(item));
#endif
				}
			}
			else {
				if (dwType == AFFECT_NEW_POTION19) {
					pAffect = AffectSystem::FindAffect(e, AFFECT_NEW_POTION20);
					if (pAffect) {
						const entt::entity pkItem = FindItemByID(e, pAffect->dwFlag);
						if (pkItem != entt::null) {
							LockItem(pkItem, false);
							SetItemSocketEcs(pkItem, 1, 0);
						}

						AffectSystem::RemoveAffect(e, AFFECT_NEW_POTION20);
					}
				}
				else if (dwType == AFFECT_NEW_POTION20) {
					pAffect = AffectSystem::FindAffect(e, AFFECT_NEW_POTION19);
					if (pAffect) {
						const entt::entity pkItem = FindItemByID(e, pAffect->dwFlag);
						if (pkItem != entt::null) {
							LockItem(pkItem, false);
							SetItemSocketEcs(pkItem, 1, 0);
						}

						AffectSystem::RemoveAffect(e, AFFECT_NEW_POTION19);
					}
				}
				else if (dwType == AFFECT_NEW_POTION21) {
					pAffect = AffectSystem::FindAffect(e, AFFECT_NEW_POTION22);
					if (pAffect) {
						const entt::entity pkItem = FindItemByID(e, pAffect->dwFlag);
						if (pkItem != entt::null) {
							LockItem(pkItem, false);
							SetItemSocketEcs(pkItem, 1, 0);
						}

						AffectSystem::RemoveAffect(e, AFFECT_NEW_POTION22);
					}
				}
				else if (dwType == AFFECT_NEW_POTION22) {
					pAffect = AffectSystem::FindAffect(e, AFFECT_NEW_POTION21);
					if (pAffect) {
						const entt::entity pkItem = FindItemByID(e, pAffect->dwFlag);
						if (pkItem != entt::null) {
							LockItem(pkItem, false);
							SetItemSocketEcs(pkItem, 1, 0);
						}

						AffectSystem::RemoveAffect(e, AFFECT_NEW_POTION21);
					}
				}

				LockItem(item, true);
				SetItemSocketEcs(itemEntity, 1, 1);
				AffectSystem::AddAffect(e, dwType, aApplyInfo[bApplyOn].bPointType, lApplyValue, GetItemID(item), INFINITE_AFFECT_DURATION, 0, true, false);
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 29, "%s", GetItemName(item));
#endif
			}
		}
							break;
#endif
		case USE_SPECIAL:

			switch (GetItemVnum(item))
			{
				//�
// ©¸®½º¸¶½º ¶õÁÖ
			case ITEM_NOG_POCKET:
			{
				/*
				// ¶õÁÖ´É·ÂÄ¡ : item_proto value ÀÇ¹Ì
					// ÀÌµ¿¼Óµµ  value 1
					// °ø°Ý·Â	  value 2
					// °æÇèÄ¡    value 3
					// Áö¼Ó½Ã°£  value 0 (´ÜÀ§ ÃÊ)

				*/
				if (AffectSystem::FindAffect(e, AFFECT_NOG_ABILITY))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 442, "");
#endif
					return false;
				}
				int32_t time = GetItemValue(item, 0);
				int32_t moveSpeedPer = GetItemValue(item, 1);
				int32_t attPer = GetItemValue(item, 2);
				int32_t expPer = GetItemValue(item, 3);
				AffectSystem::AddAffect(e, AFFECT_NOG_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
				AffectSystem::AddAffect(e, AFFECT_NOG_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
				AffectSystem::AddAffect(e, AFFECT_NOG_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
				ConsumeItemEcs(itemEntity);
			}
			break;

			//¶ó¸¶´Ü¿ë »ç�
// Á
			case ITEM_RAMADAN_CANDY:
			{
				/*
				// »ç�
// Á´É·ÂÄ¡ : item_proto value ÀÇ¹Ì
					// ÀÌµ¿¼Óµµ  value 1
					// °ø°Ý·Â	  value 2
					// °æÇèÄ¡    value 3
					// Áö¼Ó½Ã°£  value 0 (´ÜÀ§ ÃÊ)

				*/
				// @fixme147 BEGIN
				if (AffectSystem::FindAffect(e, AFFECT_RAMADAN_ABILITY))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 442, "");
#endif
					return false;
				}
				// @fixme147 END
				int32_t time = GetItemValue(item, 0);
				int32_t moveSpeedPer = GetItemValue(item, 1);
				int32_t attPer = GetItemValue(item, 2);
				int32_t expPer = GetItemValue(item, 3);
				AffectSystem::AddAffect(e, AFFECT_RAMADAN_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
				AffectSystem::AddAffect(e, AFFECT_RAMADAN_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
				AffectSystem::AddAffect(e, AFFECT_RAMADAN_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
				ConsumeItemEcs(itemEntity);
			}
			break;
			case ITEM_MARRIAGE_RING:
			{
				marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(ecs::PlayerRuntime::GetPlayerID(e));
				if (pMarriage)
				{
					if (pMarriage->ch1 != nullptr)
					{
						if (CArenaManager::instance().IsArenaMap(pMarriage->ch1->GetMapIndex()) == true)
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 672, "");
#endif
							break;
						}
					}

					if (pMarriage->ch2 != nullptr)
					{
						if (CArenaManager::instance().IsArenaMap(pMarriage->ch2->GetMapIndex()) == true)
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 672, "");
#endif
							break;
						}
					}

					int consumeSP = CalculateConsumeSP(self);

					if (consumeSP < 0)
						return false;

					ecs::PointSystem::Change(e, POINT_SP, -consumeSP, false);

					ecs::SessionSystem::WarpToPID(e, pMarriage->GetOther(ecs::PlayerRuntime::GetPlayerID(e)));
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 242, "");
				}
#endif
			}
			break;

			//±âÁ¸ ¿ë±âÀÇ ¸Á�
// ä
			case UNIQUE_ITEM_CAPE_OF_COURAGE:
				// {
					// if (ecs::PlayerRuntime::GetMapIndex(e) != 1)
					// {
	// #ifdef TEXTS_IMPROVEMENT
						// ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 489, "");
	// #endif
						// return true;
					// }


				// }
				// break;
			case 70057:
			case REWARD_BOX_UNIQUE_ITEM_CAPE_OF_COURAGE:
#ifdef __EFFETTO_MANTELLO__
				if (ecs::PlayerRuntime::GetMapIndex(e) != 1)
				{
					NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_MANTELLO);
					CombatSystem::AggregateMonster(e);

				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 489, "");
					return false;
#endif
				}
#endif
#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
			case 70030:
#ifdef __EFFETTO_MANTELLO__
				if (ecs::PlayerRuntime::GetMapIndex(e) != 1)
				{
					NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_MANTELLO);
					CombatSystem::AggregateMonsterPlus(e);

				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 489, "");
					return false;
#endif
				}

#endif
#endif
				ConsumeItemEcs(itemEntity);//@Razor93 (batorsag kopi fogyjon)
				//UpdateMountCountOverhead(e);
				break;

			case UNIQUE_ITEM_WHITE_FLAG:
				CombatSystem::ForgetMyAttacker(e);
				ConsumeItemEcs(itemEntity);
				break;

			case UNIQUE_ITEM_TREASURE_BOX:
				break;
#ifdef ENABLE_BATTLE_PASS
#ifdef ENABLE_FREE_PASS_RAZOR93
#ifdef ENABLE_BATTLE_PASS
			case 70611:
			{
				const uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(e);
				if (!bBattlePassId)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 780, "");
#endif
					return false;
				}

				// 1x hasznalhato ugyanarra a BP ID-re
				if (ecs::PlayerRuntime::HasBattlePassBoost(e, bBattlePassId))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 8, "");
#endif
					return false;
				}

				int remain = AffectSystem::GetBattlePassRemainingSeconds(e);
				if (remain <= 0)
					remain = ecs::PlayerRuntime::GetSecondsTillNextMonth();

				 
				AffectSystem::AddAffect(e, AFFECT_BATTLE_PASS_BOOST, POINT_BATTLE_PASS_ID, bBattlePassId, 0, remain, 0, true);

				 
				ecs::PlayerRuntime::ApplyBattlePassBoostRecalc(e, bBattlePassId);

				 
				CBattlePass::instance().BattlePassRequestOpen(self);

				ConsumeItemEcs(itemEntity);
			}
			break;
#endif

#else

			case 70611://79900
			{
				char szQuery[1024];
				snprintf(szQuery, sizeof(szQuery), "SELECT * FROM battle_pass_ranking WHERE player_name = '%s' AND battle_pass_id = %d;", ecs::PlayerRuntime::GetName(e).data(), 1);
				std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(szQuery));
				if (pmsg->Get()->uiNumRows > 0) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 6, "");
#endif
					return false;
				}

				int iSeconds = ecs::PlayerRuntime::GetSecondsTillNextMonth();
				if (iSeconds < 0) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 7, "");
#endif
					return false;
				}

				if (AffectSystem::FindAffect(e, AFFECT_BATTLE_PASS)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 8, "");
#endif
					return false;
				}
				else {
					AffectSystem::SetBattlePassDeadline(e, get_global_time() + iSeconds);

					AffectSystem::AddAffect(e, AFFECT_BATTLE_PASS, POINT_BATTLE_PASS_ID, 1, 0, iSeconds, 0, true);
					ConsumeItemEcs(itemEntity);
				}
			}
			break;
#endif
#endif
			case 27989: // ¿µ¼®°¨Áö±â
			case 76006: // ¼±¹°¿ë ¿µ¼®°¨Áö±â
			{
				LPSECTREE_MAP pMap = ecs::GetMap(ecs::PlayerRuntime::GetMapIndex(e));

				if (pMap != nullptr)
				{
					SetItemSocketEcs(itemEntity, 0, GetItemSocket(item, 0) + 1);

					FFindStone f;

					// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
					pMap->for_each(f);

					if (f.m_mapStone.size() > 0)
					{
						auto stone = f.m_mapStone.begin();

						uint32_t max = UINT_MAX;
						entt::entity pTarget = stone->second;

						while (stone != f.m_mapStone.end())
						{
							uint32_t dist = (uint32_t)DISTANCE_SQRT(
								ecs::PlayerRuntime::GetX(e) - ecs::PlayerRuntime::GetX(stone->second),
								ecs::PlayerRuntime::GetY(e) - ecs::PlayerRuntime::GetY(stone->second));

							if (dist != 0 && max > dist)
							{
								max = dist;
								pTarget = stone->second;
							}
							stone++;
						}

						if (pTarget != entt::null)
						{
							int val = 3;

							if (max < 10000) val = 2;
							else if (max < 70000) val = 1;

							ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "StoneDetect %u %d %d", ecs::PlayerRuntime::GetPacketVID(e), val,
								(int)GetDegreeFromPositionXY(ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(pTarget),
									ecs::PlayerRuntime::GetX(pTarget), ecs::PlayerRuntime::GetY(e)));
						}
#ifdef TEXTS_IMPROVEMENT
						else {
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 673, "");
						}
#endif
					}
#ifdef TEXTS_IMPROVEMENT
					else {
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 673, "");
					}
#endif

					if (GetItemSocket(item, 0) >= 6)
					{
						ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "StoneDetect %u 0 0", ecs::PlayerRuntime::GetPacketVID(e));
						ITEM_MANAGER::instance().RemoveItem(itemEntity);
					}
				}
				break;
			}
			break;

			case 27996: // µ¶º´
				ConsumeItemEcs(itemEntity);
				AffectSystem::ApplyPoison(e, entt::null); // @warme008
				break;

			case 27987: // Á¶°³
				// 50  µ¹Á¶°¢ 47990
				// 30  ²Î
				// 10  ¹éÁøÁÖ 47992
				// 7   Ã»ÁøÁÖ 47993
				// 3   ÇÇÁøÁÖ 47994
			{
				ConsumeItemEcs(itemEntity);

				int r = number(1, 100);

				if (r <= 50)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 458, "");
#endif
					AutoGiveItemEcs(e, 27990);
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
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 457, "");
#endif
					}
					else if (r <= prob_table[1])
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 459, "");
#endif
						AutoGiveItemEcs(e, 27992);
					}
					else if (r <= prob_table[2])
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 460, "");
#endif
						AutoGiveItemEcs(e, 27993);
					}
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 461, "");
#endif
						AutoGiveItemEcs(e, 27994);
					}
				}
			}
			break;

			case 71013: // ÃàÁ¦¿ëÆøÁ×
				CombatSystem::CreateFly(e, number(FLY_FIREWORK1, FLY_FIREWORK6), e);
				ConsumeItemEcs(itemEntity);
				break;

			case 50100: // ÆøÁ×
			case 50101:
			case 50102:
			case 50103:
			case 50104:
			case 50105:
			case 50106:
				CombatSystem::CreateFly(e, GetItemVnum(item) - 50100 + FLY_FIREWORK1, e);
				ConsumeItemEcs(itemEntity);
				break;

			case 50200: // º¸µû¸®
				if (g_bEnableBootaryCheck)
				{
					if (IS_BOTARYABLE_ZONE(ecs::PlayerRuntime::GetMapIndex(e)) == true)
					{
#ifdef KASMIR_PAKET_SYSTEM
						ecs::SocialSystem::SetKasmirPaket(e, false);
#endif
						ecs::SocialSystem::OpenPrivateShop(e, false);
					}
#ifdef TEXTS_IMPROVEMENT
					else {
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 668, "");
					}
#endif
				}
				else
				{
#ifdef KASMIR_PAKET_SYSTEM
					ecs::SocialSystem::SetKasmirPaket(e, false);
#endif
					ecs::SocialSystem::OpenPrivateShop(e, false);
				}
				break;

			case fishing::FISH_MIND_PILL_VNUM:
			{
#ifdef ENABLE_NEW_FISHING_SYSTEM
				if (AffectSystem::FindAffect(e, AFFECT_FISH_MIND_PILL)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 900, "");
#endif
					return false;
				}
#endif

				AffectSystem::AddAffect(e, AFFECT_FISH_MIND_PILL, POINT_NONE, 0, AFF_FISH_MIND, 20 * 60, 0, true);
				ConsumeItemEcs(itemEntity);
			}
			break;

			case 50304: // ¿¬°è±â ¼ö·Ã¼­
			case 50305:
			case 50306:
			{
				if (AffectSystem::IsPolymorphed(e))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				if (SkillSystem::GetSkillLevel(e, SKILL_COMBO) == 0 && ecs::PointSystem::GetLevel(e) < 30)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 322, "");
#endif
					return false;
				}

				if (SkillSystem::GetSkillLevel(e, SKILL_COMBO) == 1 && ecs::PointSystem::GetLevel(e) < 50)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 323, "");
#endif
					return false;
				}

				if (SkillSystem::GetSkillLevel(e, SKILL_COMBO) >= 2)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 324, "");
#endif
					return false;
				}

				int iPct = GetItemValue(item, 0);

				if (SkillSystem::LearnSkillByBook(e, SKILL_COMBO, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					ConsumeItemEcs(itemEntity);
#else
					ITEM_MANAGER::instance().RemoveItem(itemEntity);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SkillSystem::SetSkillNextReadTime(e, SKILL_COMBO, get_global_time() + iReadDelay);
				}
			}
			break;

#ifdef ENABLE_NEW_SECONDARY_SKILLS
			case 50333:
			case 50334:
			case 50335:
			case 50336: {
				if (AffectSystem::IsPolymorphed(e)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}

				uint32_t dwSkillVnum = GetItemValue(item, 0);
				if (SkillSystem::GetSkillLevel(e, dwSkillVnum) >= 10) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 439, "");
#endif
					return false;
				}

				if (SkillSystem::LearnSkillByBook(e, dwSkillVnum, 0)) {
					ConsumeItemEcs(itemEntity);
					SkillSystem::SetSkillNextReadTime(e, dwSkillVnum, get_global_time() + 10800);
				}
			}
					  break;
#endif

			case 50311: // ¾ð¾î ¼ö·Ã¼­
			case 50312:
			case 50313:
			{
				if (AffectSystem::IsPolymorphed(e))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = GetItemValue(item, 0);
				int iPct = MINMAX(0, GetItemValue(item, 1), 100);
				if (SkillSystem::GetSkillLevel(e, dwSkillVnum) >= 20 || dwSkillVnum - SKILL_LANGUAGE1 + 1 == ecs::PlayerRuntime::GetEmpire(e))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 439, "");
#endif
					return false;
				}

				if (SkillSystem::LearnSkillByBook(e, dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					ConsumeItemEcs(itemEntity);
#else
					ITEM_MANAGER::instance().RemoveItem(itemEntity);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SkillSystem::SetSkillNextReadTime(e, dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;

			case 50061: // ÀÏº» ¸» ¼ÒÈ¯ ½º�
// ³ ¼ö·Ã¼­
			{
				if (AffectSystem::IsPolymorphed(e))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = GetItemValue(item, 0);
				int iPct = MINMAX(0, GetItemValue(item, 1), 100);

				if (SkillSystem::GetSkillLevel(e, dwSkillVnum) >= 10)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (SkillSystem::LearnSkillByBook(e, dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					ConsumeItemEcs(itemEntity);
#else
					ITEM_MANAGER::instance().RemoveItem(itemEntity);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SkillSystem::SetSkillNextReadTime(e, dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;

			case 50314: case 50315: case 50316: // º¯½�
 // ¼ö·Ã¼­
			case 50323: case 50324: // ÁõÇ÷ ¼ö·Ã¼­
			case 50325: case 50326: // Ã¶�
// ë ¼ö·Ã¼­
			{
				if (AffectSystem::IsPolymorphed(e) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 313, "");
#endif
					return false;
				}

				int iSkillLevelLowLimit = GetItemValue(item, 0);
				int iSkillLevelHighLimit = GetItemValue(item, 1);
				int iPct = MINMAX(0, GetItemValue(item, 2), 100);
				int iLevelLimit = GetItemValue(item, 3);
				uint32_t dwSkillVnum = 0;

				switch (GetItemVnum(item))
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

				if (ecs::PointSystem::GetLevel(e) < iLevelLimit)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 431, "%d", iLevelLimit);
#endif
					return false;
				}

				if (SkillSystem::GetSkillLevel(e, dwSkillVnum) >= 40)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (SkillSystem::GetSkillLevel(e, dwSkillVnum) < iSkillLevelLowLimit)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 429, "");
#endif
					return false;
				}

				if (SkillSystem::GetSkillLevel(e, dwSkillVnum) >= iSkillLevelHighLimit)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (SkillSystem::LearnSkillByBook(e, dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					ConsumeItemEcs(itemEntity);
#else
					ITEM_MANAGER::instance().RemoveItem(itemEntity);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SkillSystem::SetSkillNextReadTime(e, dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;

			case 50902:
			case 50903:
			case 50904:
			{
				if (AffectSystem::IsPolymorphed(e))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = SKILL_CREATE;
				int iPct = MINMAX(0, GetItemValue(item, 1), 100);

				if (SkillSystem::GetSkillLevel(e, dwSkillVnum) >= 40)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (SkillSystem::LearnSkillByBook(e, dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					ConsumeItemEcs(itemEntity);
#else
					ITEM_MANAGER::instance().RemoveItem(itemEntity);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SkillSystem::SetSkillNextReadTime(e, dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;
			// MINING
			case ITEM_MINING_SKILL_TRAIN_BOOK:
			{
				if (AffectSystem::IsPolymorphed(e))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = SKILL_MINING;
				int iPct = MINMAX(0, GetItemValue(item, 1), 100);

				if (SkillSystem::GetSkillLevel(e, dwSkillVnum) >= 40)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 306, "");
#endif
					return false;
				}

				if (SkillSystem::LearnSkillByBook(e, dwSkillVnum, iPct))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					ConsumeItemEcs(itemEntity);
#else
					ITEM_MANAGER::instance().RemoveItem(itemEntity);
#endif

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					SkillSystem::SetSkillNextReadTime(e, dwSkillVnum, get_global_time() + iReadDelay);
				}
			}
			break;
			// END_OF_MINING

			case ITEM_HORSE_SKILL_TRAIN_BOOK:
			{
				if (AffectSystem::IsPolymorphed(e))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 313, "");
#endif
					return false;

				}
				uint32_t dwSkillVnum = SKILL_HORSE;
				int iPct = MINMAX(0, GetItemValue(item, 1), 100);

				if (ecs::PointSystem::GetLevel(e) < 50)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 404, "%d", 50);
#endif
					return false;
				}

				if (!test_server && get_global_time() < SkillSystem::GetSkillNextReadTime(e, dwSkillVnum))
				{
					if (AffectSystem::FindAffect(e, AFFECT_SKILL_NO_BOOK_DELAY))
					{
						// ÁÖ¾È¼ú¼­ »ç¿ëÁß¿¡´Â ½Ã°£ Á¦ÇÑ ¹«½Ã
						AffectSystem::RemoveAffect(e, AFFECT_SKILL_NO_BOOK_DELAY);
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 465, "");
#endif
					}
					else
					{
						SkillSystem::SkillLearnWaitMoreTimeMessage(e, SkillSystem::GetSkillNextReadTime(e, dwSkillVnum) - get_global_time());
						return false;
					}
				}

				if (ecs::PointSystem::Get(e, POINT_HORSE_SKILL) >= 20 ||
					SkillSystem::GetSkillLevel(e, SKILL_HORSE_WILDATTACK) + SkillSystem::GetSkillLevel(e, SKILL_HORSE_CHARGE) + SkillSystem::GetSkillLevel(e, SKILL_HORSE_ESCAPE) >= 60 ||
					SkillSystem::GetSkillLevel(e, SKILL_HORSE_WILDATTACK_RANGE) + SkillSystem::GetSkillLevel(e, SKILL_HORSE_CHARGE) + SkillSystem::GetSkillLevel(e, SKILL_HORSE_ESCAPE) >= 60)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 307, "");
#endif
					return false;
				}

				if (number(1, 100) <= iPct)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 394, "");
#endif
					ecs::PointSystem::Change(e, POINT_HORSE_SKILL, 1);

					int iReadDelay = number(SKILLBOOK_DELAY_MIN, SKILLBOOK_DELAY_MAX);
					if (!test_server)
						SkillSystem::SetSkillNextReadTime(e, dwSkillVnum, get_global_time() + iReadDelay);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 393, "");
				}
#endif
#ifdef ENABLE_BOOKS_STACKFIX
				ConsumeItemEcs(itemEntity);
#else
				ITEM_MANAGER::instance().RemoveItem(itemEntity);
#endif
			}
			break;


			case 70102: // Zenbab
			{

				uint32_t max_limit = 2500000;
				uint32_t current = CombatSystem::GetAlignment(e);

				if (current >= max_limit)
				{
					ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Max  250.000 with e item!");
					return false;
				}

				uint32_t add_value = GetItemValue(item, 0); 
				uint32_t remaining = max_limit - current;

				uint32_t real_add = std::min(add_value, remaining);

				CombatSystem::UpdateAlignment(e, real_add);
				ConsumeItemEcs(itemEntity);

				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, " +500 point.");
			}
			break;


			case 70100:
			{
				const uint32_t max_limit = 25000000;
				const uint32_t current = CombatSystem::GetAlignment(e);

				if (current >= max_limit)
				{
					ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Max 2.500.000 with e item!");
					return false;
				}

				const uint32_t add_value = GetItemValue(item, 0);
				const uint32_t real_add = std::min(add_value, max_limit - current);

				CombatSystem::UpdateAlignment(e, real_add);
				ConsumeItemEcs(itemEntity);

				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "+5.000 point.");
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
				const uint32_t count = GetItemCount(item);
				if (count == 0)
					break;

				const uint64_t unitPrice = (uint64_t)GetItemShopBuyPrice(item);
				if (unitPrice == 0)
					break;

				const uint64_t total = unitPrice * (uint64_t)count;

				if (ecs::PlayerRuntime::GetDesc(e) == nullptr)
					break;

				const uint32_t curCoins = ecs::PlayerRuntime::GetDragonCoin(e);
				const uint64_t maxCoins = 0xFFFFFFFFULL; // uint32 max

				if ((uint64_t)curCoins >= maxCoins)
					break;

				const uint64_t canAdd = ((uint64_t)curCoins + total > maxCoins) ? (maxCoins - (uint64_t)curCoins) : total;
				if (canAdd == 0)
					break;

				ecs::PlayerRuntime::SetDragonCoin(e, curCoins + (uint32_t)canAdd);

				ConsumeItemEcs(itemEntity, count); // teljes stack felhasznalasa
				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Kaptal %u Sarkanyermet.", (uint32_t)canAdd);
#else
				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "ItemShop ki van kapcsolva.");
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
				const uint32_t current = CombatSystem::GetAlignment(e);

				if (current >= max_limit)
				{
					ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Max 2.500.000 with e item!");
					return false;
				}

				const uint32_t add_value = GetItemValue(item, 0);
				const uint32_t real_add = std::min(add_value, max_limit - current);

				CombatSystem::UpdateAlignment(e, real_add);
				ConsumeItemEcs(itemEntity);

				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "+2000 point");
			}
			break;

			case 72101:
			{
				const uint32_t min_limit = 25000000; // 2.500.000 lathato rang
				const uint32_t max_limit = 50000000; // 5.000.000 lathato rang
				const uint32_t current = CombatSystem::GetAlignment(e);

				if (current < min_limit)
				{
					ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Min point: 2.500.000 ");
					return false;
				}

				if (current >= max_limit)
				{
					ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Max 5.000.000!");
					return false;
				}

				// 10.000  rang = 100.000 belso alignment
				const uint32_t add_value = 300000;
				const uint32_t real_add = std::min(add_value, max_limit - current);

				CombatSystem::UpdateAlignment(e, real_add);
				ConsumeItemEcs(itemEntity);

				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "+30.000 point addaed.");
			}
			break;
			// --------------------------------------------------------------
			// Arany Gyümölcs – +10000 RP (1 óránként használható)
			// --------------------------------------------------------------
			case 72100:
			{
				const uint32_t max_limit = 25000000;
				const uint32_t current = CombatSystem::GetAlignment(e);

				if (current >= max_limit)
				{
					ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "Max 2.500.000!");
					return false;
				}

				const uint32_t add_value = GetItemValue(item, 0);
				const uint32_t real_add = std::min(add_value, max_limit - current);

				CombatSystem::UpdateAlignment(e, real_add);
				ConsumeItemEcs(itemEntity);

				ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "+10.000.");
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
				const uint32_t count = GetItemCount(item);
				if (count == 0)
					break;

				const uint64_t unitPrice = (uint64_t)GetItemShopBuyPrice(item);
				if (unitPrice == 0)
					break;

				const uint64_t curGold = (uint64_t)ecs::PointSystem::GetGold(e);
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

				 
				GiveGold(e, (long long)canAdd);

				 
				ConsumeItemEcs(itemEntity, canUse);
			}
			break;


			//case 71107: // Ãµµµº¹¼þ¾Æ
//			{
//				uint32_t val = GetItemValue(item, 0);
//				int interval = GetItemValue(item, 1);
//				quest::PC* pPC = quest::CQuestManager::instance().GetPC(ecs::PlayerRuntime::GetPlayerID(e));
//				int last_use_time = pPC->GetFlag("mythical_peach.last_use_time");
//
//				if (get_global_time() - last_use_time < interval * 60 * 60)
//				{
//#ifdef TEXTS_IMPROVEMENT
//					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 508, "");
//#endif
//					return false;
//				}
//
//				if (CombatSystem::GetAlignment(e) == 25000000)
//				{
//#ifdef TEXTS_IMPROVEMENT
//					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 674, "%d", 25000000);
//#endif
//					return false;
//				}
//
//				if (25000000 - CombatSystem::GetAlignment(e) < val * 10)
//				{
//					val = (25000000 - CombatSystem::GetAlignment(e)) / 10;
//				}
//
//				uint32_t old_alignment = CombatSystem::GetAlignment(e) / 10;
//
//				CombatSystem::UpdateAlignment(e, val * 10);
//
//				ConsumeItemEcs(itemEntity);
//				pPC->SetFlag("mythical_peach.last_use_time", get_global_time());
//
//#ifdef TEXTS_IMPROVEMENT
//				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 327, "%d", val);
//#endif
//
//				char buf[256 + 1];
//				snprintf(buf, sizeof(buf), "%u %u", old_alignment, CombatSystem::GetAlignment(e) / 10);
//				LogManager::instance().CharLog(e, val, "MYTHICAL_PEACH", buf);
//			}
//			break;

			case 71109: // �
// »¼®¼­
			case 72719:
			{
				entt::entity item2 = entt::null;

				if (!InventorySystem::IsValidItemPosition(e, DestCell) || (item2 = GetItem(e, DestCell)) == entt::null)
					return false;

				if (IsItemExchanging(item2) || IsItemEquipped(item2)) // @fixme114
					return false;

				if (GetItemSocketCount(item2) == 0)
					return false;

#ifdef ENABLE_BUG_FIXES
				if (IsItemEquipped(item2))
					return false;
#endif

				switch (GetItemType(item2))
				{
				case ITEM_WEAPON:
					break;
				case ITEM_ARMOR:
					switch (GetItemSubType(item2))
					{
					case ARMOR_EAR:
					case ARMOR_WRIST:
					case ARMOR_NECK:
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 675, "%s", GetItemName(item));
#endif
						return false;
					}
					break;

				default:
					return false;
				}

				std::stack<int32_t> socket;

				for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
					socket.push(GetItemSocket(item2, i));

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
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 675, "%s", GetItemName(item2));
#endif
					return false;
				}

				const entt::entity pItemReward = AutoGiveItemEcs(e, socket.top());

				if (IsValidItem(pItemReward))
				{
					SetItemSocketEcs(item2, idx, 1);

					char buf[256 + 1];
					snprintf(buf, sizeof(buf), "%s(%u) %s(%u)",
						GetItemName(item2), GetItemID(item2), GetItemName(pItemReward), GetItemID(pItemReward));
					LogManager::instance().ItemLogEntity(e, item, "USE_DETACHMENT_ONE", buf);

					ConsumeItemEcs(itemEntity);
				}
			}
			break;

			case 70201:   // �
// »»öÁ¦
			case 70202:   // ¿°»ö¾à(Èò»ö)
			case 70203:   // ¿°»ö¾à(±Ý»ö)
			case 70204:   // ¿°»ö¾à(»¡°£»ö)
			case 70205:   // ¿°»ö¾à(°¥»ö)
			case 70206:   // ¿°»ö¾à(°ËÀº»ö)
			{
				if (ecs::PlayerRuntime::GetPart(e, PART_HAIR) < 1001)
				{
					quest::CQuestManager& q = quest::CQuestManager::instance();
					quest::PC* pPC = q.GetPC(ecs::PlayerRuntime::GetPlayerID(e));

					if (pPC)
					{
						int last_dye_level = pPC->GetFlag("dyeing_hair.last_dye_level");

						if (last_dye_level == 0 ||
							last_dye_level + 3 <= ecs::PointSystem::GetLevel(e) ||
							GetItemVnum(item) == 70201)
						{
							ecs::PlayerRuntime::SetPart(e, PART_HAIR, GetItemVnum(item) - 70201);

							if (GetItemVnum(item) == 70201)
								pPC->SetFlag("dyeing_hair.last_dye_level", 0);
							else
								pPC->SetFlag("dyeing_hair.last_dye_level", ecs::PointSystem::GetLevel(e));

							ConsumeItemEcs(itemEntity);
							NetworkSyncSystem::UpdatePacket(e);
						}
#ifdef TEXTS_IMPROVEMENT
						else {
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 97, "%d", last_dye_level + 3);
						}
#endif
					}
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 491, "");
				}
#endif
			}
			break;

			case ITEM_NEW_YEAR_GREETING_VNUM:
			{
				uint32_t dwBoxVnum = ITEM_NEW_YEAR_GREETING_VNUM;
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector<entt::entity> item_gets;
				int count = 0;

				if (self->GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
				{
#ifdef TEXTS_IMPROVEMENT
					for (int i = 0; i < count; i++) {
						if (dwVnums[i] == CSpecialItemGroup::GOLD) {
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 102, "%d", dwCounts[i]);
						}
					}
#endif
					ConsumeItemEcs(itemEntity);
				}
			}
			break;

			case ITEM_VALENTINE_ROSE:
			case ITEM_VALENTINE_CHOCOLATE:
			{
				uint32_t dwBoxVnum = GetItemVnum(item);
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector<entt::entity> item_gets;
				int count = 0;

				if (GetItemVnum(item) == ITEM_VALENTINE_ROSE && SEX_MALE == GET_SEX(self)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 383, "");
#endif
					return false;
				}
				else if (GetItemVnum(item) == ITEM_VALENTINE_CHOCOLATE && SEX_FEMALE == GET_SEX(self)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 382, "");
#endif
					return false;
				}

				if (self->GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
					ConsumeItemEcs(itemEntity);
			}
			break;

			case ITEM_WHITEDAY_CANDY:
			case ITEM_WHITEDAY_ROSE:
			{
				uint32_t dwBoxVnum = GetItemVnum(item);
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector<entt::entity> item_gets;
				int count = 0;

				if (GetItemVnum(item) == ITEM_WHITEDAY_ROSE && SEX_MALE == GET_SEX(self)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 383, "");
#endif
					return false;
				}
				else if (GetItemVnum(item) == ITEM_WHITEDAY_CANDY && SEX_FEMALE == GET_SEX(self)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 382, "");
#endif
					return false;
				}

				if (self->GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
					ConsumeItemEcs(itemEntity);
			}
			break;

			case 50011: // ¿ù±¤º¸ÇÕ
			{
				uint32_t dwBoxVnum = 50011;
				std::vector <uint32_t> dwVnums;
				std::vector <uint32_t> dwCounts;
				std::vector<entt::entity> item_gets;
				int count = 0;

				if (self->GiveItemFromSpecialItemGroup(dwBoxVnum, dwVnums, dwCounts, item_gets, count))
				{
					for (int i = 0; i < count; i++)
					{
						char buf[50 + 1];
						snprintf(buf, sizeof(buf), "%u %u", dwVnums[i], dwCounts[i]);
						LogManager::instance().ItemLogEntity(e, item, "MOONLIGHT_GET", buf);

						//ITEM_MANAGER::instance().RemoveItem(item);
						ConsumeItemEcs(itemEntity);

						switch (dwVnums[i])
						{
						case CSpecialItemGroup::GOLD:
							break;
						case CSpecialItemGroup::EXP:
							break;

						case CSpecialItemGroup::MOB:
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 378, "");
#endif
							break;

						case CSpecialItemGroup::SLOW:
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 377, "");
#endif
							break;

						case CSpecialItemGroup::DRAIN_HP:
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 373, "");
#endif
							break;

						case CSpecialItemGroup::POISON:
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 376, "");
#endif
							break;
						case CSpecialItemGroup::MOB_GROUP:
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 380, "");
#endif
							break;

						default:
							//#ifdef TEXTS_IMPROVEMENT
							//												if (item_gets[i]) {
							//													if (dwCounts[i] > 1) {
							//														ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 374, "%d#%s", dwCounts[i], item_gets[i]->GetName());
							//													} else {
							//														ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 375, "%s", item_gets[i]->GetName());
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
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 395, "");
#endif
					return false;
				}
			}
			break;

			case ITEM_GIVE_STAT_RESET_COUNT_VNUM:
			{
				//ecs::PointSystem::Change(e, POINT_GOLD, -iCost);
				ecs::PointSystem::Change(e, POINT_STAT_RESET_COUNT, 1);
				ConsumeItemEcs(itemEntity);
			}
			break;

			case 50107:
			{
				if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && !IsAllowedPotionOnPVP(GetItemVnum(item)))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_CHINA_FIREWORK);
#ifdef ENABLE_FIREWORK_STUN
				// ½º�
// Ï °ø°ÝÀ» ¿Ã·ÁÁØ´Ù
				AffectSystem::AddAffect(e, AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5 * 60, 0, true);
#endif
				ConsumeItemEcs(itemEntity);
			}
			break;

			case 50108:
			{
				if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && !IsAllowedPotionOnPVP(GetItemVnum(item)))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_SPIN_TOP);
#ifdef ENABLE_FIREWORK_STUN
				// ½º�
// Ï °ø°ÝÀ» ¿Ã·ÁÁØ´Ù
				AffectSystem::AddAffect(e, AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5 * 60, 0, true);
#endif
				ConsumeItemEcs(itemEntity);
			}
			break;

			case ITEM_WONSO_BEAN_VNUM:
				ecs::PointSystem::Change(e, POINT_HP, ecs::PointSystem::GetMaxHP(e) - ecs::PlayerRuntime::GetHP(e));
				ConsumeItemEcs(itemEntity);
				break;

			case ITEM_WONSO_SUGAR_VNUM:
				ecs::PointSystem::Change(e, POINT_SP, ecs::PointSystem::GetMaxSP(e) - ecs::PlayerRuntime::GetSP(e));
				ConsumeItemEcs(itemEntity);
				break;

			case ITEM_WONSO_FRUIT_VNUM:
				ecs::PointSystem::Change(e, POINT_STAMINA, ecs::PlayerRuntime::GetMaxStamina(e) - ecs::PlayerRuntime::GetStamina(e));
				ConsumeItemEcs(itemEntity);
				break;

			case ITEM_ELK_VNUM: // µ·²Ù·¯¹Ì
			{
				int iGold = GetItemSocket(item, 0);
				ITEM_MANAGER::instance().RemoveItem(itemEntity);
				ecs::PointSystem::Change(e, POINT_GOLD, iGold);
			}
			break;
			case 27995:
			{
			}
			break;

			case 71092: // º¯½�
 // ÇØÃ¼ºÎ ÀÓ½Ã
			{
				const entt::entity selectedTarget =
					CombatSystem::GetSelectedTarget(e);
				if (selectedTarget != entt::null)
				{
					if (AffectSystem::IsPolymorphed(selectedTarget))
					{
						AffectSystem::SetPolymorph(selectedTarget, 0, false);
						AffectSystem::RemoveAffect(selectedTarget, AFFECT_POLYMORPH);
					}
				}
				else
				{
					if (AffectSystem::IsPolymorphed(e))
					{
						AffectSystem::SetPolymorph(e, 0);
						AffectSystem::RemoveAffect(e, AFFECT_POLYMORPH);
					}
				}
			}
			break;

			case 30617: // ÁøÀç°¡
			{
				// À¯·´, ½Ì°¡Æú, º£Æ®³² ÁøÀç°¡ »ç¿ë±ÝÁö
				const entt::entity item2 = GetItem(e, DestCell);

				if (!InventorySystem::IsValidItemPosition(e, DestCell) || !IsValidItem(item2))
					return false;

				if (ITEM_COSTUME == GetItemType(item2))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (IsItemExchanging(item2) || IsItemEquipped(item2)) // @fixme114
					return false;

				if (GetItemAttributeSetIndex(item2) == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (AddItemRareAttributeEcs(item2) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 389, "");
#endif
					int iAddedIdx = GetItemRareAttributeCount(item2) + 4;
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", GetItemID(item2));

					LogManager::instance().ItemLog(
						ecs::PlayerRuntime::GetPlayerID(e),
						GetItemAttributeType(item2, iAddedIdx),
						GetItemAttributeValue(item2, iAddedIdx),
						GetItemID(itemEntity),
						"ADD_RARE_ATTR",
						buf,
						ecs::PlayerRuntime::GetDesc(e)->GetHostName(),
						GetItemOriginalVnum(itemEntity));

					ConsumeItemEcs(itemEntity);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 308, "");
				}
#endif
			}
			break;

			case 30618: // ÁøÀç°æ
			{
				// À¯·´, ½Ì°¡Æú, º£Æ®³² ÁøÀç°¡ »ç¿ë±ÝÁö
				const entt::entity item2 = GetItem(e, DestCell);

				if (!InventorySystem::IsValidItemPosition(e, DestCell) || !IsValidItem(item2))
					return false;

				if (ITEM_COSTUME == GetItemType(item2)) // @fixme124
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (IsItemExchanging(item2) || IsItemEquipped(item2)) // @fixme114
					return false;

				if (GetItemAttributeSetIndex(item2) == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (ChangeItemRareAttributeEcs(item2) == true)
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
					LogManager::instance().ItemLogEntity(e, itemEntity, "CHANGE_RARE_ATTR", buf);

					ConsumeItemEcs(itemEntity);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 354, "");
				}
#endif
			}
			break;
#ifdef ENABLE_CHANGE_NORMAL_HIT_RAZOR93
			case 70251: // ÁøÀç°æ
			{
				// À¯·´, ½Ì°¡Æú, º£Æ®³² ÁøÀç°¡ »ç¿ë±ÝÁö
				entt::entity item2 = entt::null;

				if (!InventorySystem::IsValidItemPosition(e, DestCell) || (item2 = GetItem(e, DestCell)) == entt::null)
					return false;

				if (ITEM_COSTUME == GetItemType(item2)) // @fixme124
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (IsItemExchanging(item2) || IsItemEquipped(item2)) // @fixme114
					return false;

				if (GetItemAttributeSetIndex(item2) == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				// The attribute roll behind this scroll is still CItem work; it reaches
				// AddAttr4, which is its own migration.
				if (LegacyItemBoundary(item2)->ChangeKKAK() == true)
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
					LogManager::instance().ItemLogEntity(e, item, "CHANGE_RARE_ATTR21", buf);

					ConsumeItemEcs(itemEntity);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 354, "");
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
				// ±×·¡¼­ ±×³É ÇÏµå ÄÚµù. ¼±¹° »óÀÚ¿ë ÀÚµ¿¹°¾à ¾ÆÀÌ�
// Ûµé.
			case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
			case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
			case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
			case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
			{
				if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#ifdef ENABLE_NEWSTUFF
				else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && !IsAllowedPotionOnPVP(GetItemVnum(item)))
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
					return false;
				}
#endif

				EAffectTypes type = AFFECT_NONE;
				bool isSpecialPotion = false;

				switch (GetItemVnum(item))
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

				if (GetItemCount(item) > 1)
				{
#ifdef ENABLE_EXTRA_INVENTORY
					const bool bFromExtraInventory = (GetItemWindow(item) == EXTRA_INVENTORY);
#else
					const bool bFromExtraInventory = false;
#endif
					int pos = -1;

#ifdef ENABLE_EXTRA_INVENTORY
					if (bFromExtraInventory)
						pos = GetEmptyExtraInventory(e, item);
					else
#endif
						pos = InventorySystem::GetEmptyInventory(e, GetItemSize(item));

					if (-1 == pos)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 366, "");
#endif
						break;
					}

					ConsumeItemEcs(itemEntity);

					const entt::entity item2 = ITEM_MANAGER::instance().CreateItem(GetItemVnum(item), 1);

#ifdef ENABLE_EXTRA_INVENTORY
					if (bFromExtraInventory)
						InventorySystem::AddToCharacter(item2, e, TItemPos(EXTRA_INVENTORY, pos));
					else
#endif
						InventorySystem::AddToCharacter(item2, e, TItemPos(INVENTORY, pos));

					if (GetItemSocket(item, 1) != 0)
					{
						SetItemSocketEcs(item2, 1, GetItemSocket(item, 1));
					}

					if (AffectSystem::FindAffect(e, type))
						return true;
					else if (isSpecialPotion) {
						EAffectTypes eType = type == AFFECT_AUTO_HP_RECOVERY ? AFFECT_AUTO_HP_RECOVERY2 : AFFECT_AUTO_SP_RECOVERY2;
						if (AffectSystem::FindAffect(e, eType))
							return true;
					}

					itemEntity = item2;
				}

#ifdef ENABLE_NEW_USE_POTION
				EAffectTypes type2 = AFFECT_NONE;
				CAffect* pAffect2 = nullptr;
#endif
				CAffect* pAffect = AffectSystem::FindAffect(e, type);

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

					pAffect2 = AffectSystem::FindAffect(e, type2);
					if (pAffect2) {
						if (GetItemID(item) == pAffect2->dwFlag)
						{
							AffectSystem::RemoveAffect(e, pAffect2);
							LockItem(item, false);
							SetItemSocketEcs(itemEntity, 0, false);
						}
						else
						{
							const entt::entity old = FindItemByID(e, pAffect2->dwFlag);
							if (old != entt::null)
							{
								LockItem(old, false);
								SetItemSocketEcs(old, 0, false);
							}

							AffectSystem::RemoveAffect(e, pAffect2);
						}
					}
					else if (isSpecialPotion == true) {
						pAffect2 = AffectSystem::FindAffect(e, type);
						if (pAffect2) {
							if (GetItemID(item) == pAffect2->dwFlag)
							{
								AffectSystem::RemoveAffect(e, pAffect2);
								LockItem(item, false);
								SetItemSocketEcs(itemEntity, 0, false);
								return true;
							}
							else {
								const entt::entity old = FindItemByID(e, pAffect2->dwFlag);
								if (old != entt::null)
								{
									LockItem(old, false);
									SetItemSocketEcs(old, 0, false);
								}
							}
						}
					}
#endif

					AffectSystem::AddAffect(e, type, bonus, 4, GetItemID(item), INFINITE_AFFECT_DURATION, 0, true, false);
					LockItem(item, true);
					SetItemSocketEcs(itemEntity, 0, true);
					AutoRecoveryItemProcess(e, type);
				}
				else
				{
					if (GetItemID(item) == pAffect->dwFlag)
					{
						AffectSystem::RemoveAffect(e, pAffect);

						LockItem(item, false);
						SetItemSocketEcs(itemEntity, 0, false);
					}
					else
					{
						const entt::entity old = FindItemByID(e, pAffect->dwFlag);

						if (old != entt::null)
						{
							LockItem(old, false);
							SetItemSocketEcs(old, 0, false);
						}

						AffectSystem::RemoveAffect(e, pAffect);

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

						pAffect2 = AffectSystem::FindAffect(e, type2);
						if (pAffect2) {
							if (GetItemID(item) == pAffect2->dwFlag)
							{
								AffectSystem::RemoveAffect(e, pAffect2);
								LockItem(item, false);
								SetItemSocketEcs(itemEntity, 0, false);
							}
							else
							{
								const entt::entity old = FindItemByID(e, pAffect2->dwFlag);
								if (old != entt::null)
								{
									LockItem(old, false);
									SetItemSocketEcs(old, 0, false);
								}

								AffectSystem::RemoveAffect(e, pAffect2);
							}
						}
#endif

						AffectSystem::AddAffect(e, type, bonus, 4, GetItemID(item), INFINITE_AFFECT_DURATION, 0, true, false);

						LockItem(item, true);
						SetItemSocketEcs(itemEntity, 0, true);

						AutoRecoveryItemProcess(e, type);
					}
				}
			}
			break;
			}
			break;

		case USE_CLEAR:
		{
			switch (GetItemVnum(item))
			{
			case 27874: // Grilled Perch
			default:
				AffectSystem::RemoveBadAffects(e);
				break;
			}
			ConsumeItem(itemEntity);
		}
		break;

		case USE_INVISIBILITY:
		{
			if (GetItemVnum(item) == 70026)
			{
				quest::CQuestManager& q = quest::CQuestManager::instance();
				quest::PC* pPC = q.GetPC(ecs::PlayerRuntime::GetPlayerID(e));

				if (pPC != nullptr)
				{
					int last_use_time = pPC->GetFlag("mirror_of_disapper.last_use_time");

					if (get_global_time() - last_use_time < 10 * 60)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 508, "");
#endif
						return false;
					}

					pPC->SetFlag("mirror_of_disapper.last_use_time", get_global_time());
				}
			}

			AffectSystem::AddAffect(e, AFFECT_INVISIBILITY, POINT_NONE, 0, AFF_INVISIBILITY, 300, 0, true);
			ConsumeItemEcs(itemEntity);
		}
		break;

		case USE_POTION_NODELAY:
		{
			if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
			{
				if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 303, "");
#endif
					return false;
				}

				switch (GetItemVnum(item))
				{
				case 70020:
				case 71018:
				case 71019:
				case 71020:
					if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
					{
						if (ecs::PlayerRuntime::GetPotionLimit(e) <= 0)
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 362, "");
#endif
							return false;
						}
					}
					break;

				default:
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 303, "");
#endif
					return false;
				}
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && !IsAllowedPotionOnPVP(GetItemVnum(item)))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif

			bool used = false;

			if (GetItemValue(item, 0) != 0) // HP Àý´ë°ª È¸º¹
			{
				if (ecs::PlayerRuntime::GetHP(e) < ecs::PointSystem::GetMaxHP(e))
				{
					ecs::PointSystem::Change(e, POINT_HP, GetItemValue(item, 0) * (100 + ecs::PointSystem::Get(e, POINT_POTION_BONUS)) / 100);
					NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_HPUP_RED);
					used = true;
				}
			}

			if (GetItemValue(item, 1) != 0)	// SP Àý´ë°ª È¸º¹
			{
				if (ecs::PlayerRuntime::GetSP(e) < ecs::PointSystem::GetMaxSP(e))
				{
					ecs::PointSystem::Change(e, POINT_SP, GetItemValue(item, 1) * (100 + ecs::PointSystem::Get(e, POINT_POTION_BONUS)) / 100);
					NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_SPUP_BLUE);
					used = true;
				}
			}

			if (GetItemValue(item, 3) != 0) // HP % È¸º¹
			{
				if (ecs::PlayerRuntime::GetHP(e) < ecs::PointSystem::GetMaxHP(e))
				{
					ecs::PointSystem::Change(e, POINT_HP, GetItemValue(item, 3) * ecs::PointSystem::GetMaxHP(e) / 100);
					NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_HPUP_RED);
					used = true;
				}
			}

			if (GetItemValue(item, 4) != 0) // SP % È¸º¹
			{
				if (ecs::PlayerRuntime::GetSP(e) < ecs::PointSystem::GetMaxSP(e))
				{
					ecs::PointSystem::Change(e, POINT_SP, GetItemValue(item, 4) * ecs::PointSystem::GetMaxSP(e) / 100);
					NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_SPUP_BLUE);
					used = true;
				}
			}

			if (used)
			{
				if (GetItemVnum(item) == 50085 || GetItemVnum(item) == 50086) {
					ecs::PlayerRuntime::SetUseSeedOrMoonBottleTime(e);
				}

				if (ecs::SocialSystem::GetWarMap(e))
					ecs::SocialSystem::GetWarMap(e)->UsePotion(e, itemEntity);

				ecs::PlayerRuntime::SetPotionLimit(e, ecs::PlayerRuntime::GetPotionLimit(e) - 1);

				//RESTRICT_USE_SEED_OR_MOONBOTTLE
				ConsumeItemEcs(itemEntity);
				//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
			}
		}
		break;

		case USE_POTION:
			if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
			{
				if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 303, "");
#endif
					return false;
				}
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && !IsAllowedPotionOnPVP(GetItemVnum(item)))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif

			if (GetItemValue(item, 1) != 0)
			{
				if (ecs::PointSystem::Get(e, POINT_SP_RECOVERY) + ecs::PlayerRuntime::GetSP(e) >= ecs::PointSystem::GetMaxSP(e))
				{
					return false;
				}

				ecs::PointSystem::Change(e, POINT_SP_RECOVERY, GetItemValue(item, 1) * std::min((int64_t)200, (100 + ecs::PointSystem::Get(e, POINT_POTION_BONUS))) / 100);
				AffectSystem::StartAffectEvent(e);
				NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_SPUP_BLUE);
			}

			if (GetItemValue(item, 0) != 0)
			{
				if (ecs::PointSystem::Get(e, POINT_HP_RECOVERY) + ecs::PlayerRuntime::GetHP(e) >= ecs::PointSystem::GetMaxHP(e))
				{
					return false;
				}

				ecs::PointSystem::Change(e, POINT_HP_RECOVERY, GetItemValue(item, 0) * std::min((int64_t)200, (100 + ecs::PointSystem::Get(e, POINT_POTION_BONUS))) / 100);
				AffectSystem::StartAffectEvent(e);
				NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_HPUP_RED);
			}

			if (ecs::SocialSystem::GetWarMap(e))
				ecs::SocialSystem::GetWarMap(e)->UsePotion(e, itemEntity);

			ConsumeItem(itemEntity);
			ecs::PlayerRuntime::SetPotionLimit(e, ecs::PlayerRuntime::GetPotionLimit(e) - 1);
			break;

		case USE_POTION_CONTINUE:
		{
			if (GetItemValue(item, 0) != 0)
			{
				AffectSystem::AddAffect(e, AFFECT_HP_RECOVER_CONTINUE, POINT_HP_RECOVER_CONTINUE, GetItemValue(item, 0), 0, GetItemValue(item, 2), 0, true);
			}
			else if (GetItemValue(item, 1) != 0)
			{
				AffectSystem::AddAffect(e, AFFECT_SP_RECOVER_CONTINUE, POINT_SP_RECOVER_CONTINUE, GetItemValue(item, 1), 0, GetItemValue(item, 2), 0, true);
			}
			else
				return false;
		}

		if (ecs::SocialSystem::GetWarMap(e))
			ecs::SocialSystem::GetWarMap(e)->UsePotion(e, itemEntity);

		ConsumeItem(itemEntity);
		break;

		case USE_ABILITY_UP:
		{
			switch (GetItemValue(item, 0))
			{
			case APPLY_MOV_SPEED:
				AffectSystem::AddAffect(e, AFFECT_MOV_SPEED, POINT_MOV_SPEED, GetItemValue(item, 2), AFF_MOV_SPEED_POTION, GetItemValue(item, 1), 0, true);
#ifdef ENABLE_EFFECT_EXTRAPOT
				EffectPacket(SE_DXUP_PURPLE);
#endif
				break;

			case APPLY_ATT_SPEED:
				AffectSystem::AddAffect(e, AFFECT_ATT_SPEED, POINT_ATT_SPEED, GetItemValue(item, 2), AFF_ATT_SPEED_POTION, GetItemValue(item, 1), 0, true);
#ifdef ENABLE_EFFECT_EXTRAPOT
				EffectPacket(SE_SPEEDUP_GREEN);
#endif
				break;

			case APPLY_STR:
				AffectSystem::AddAffect(e, AFFECT_STR, POINT_ST, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true);
				break;

			case APPLY_DEX:
				AffectSystem::AddAffect(e, AFFECT_DEX, POINT_DX, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true);
				break;

			case APPLY_CON:
				AffectSystem::AddAffect(e, AFFECT_CON, POINT_HT, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true);
				break;

			case APPLY_INT:
				AffectSystem::AddAffect(e, AFFECT_INT, POINT_IQ, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true);
				break;

			case APPLY_CAST_SPEED:
				AffectSystem::AddAffect(e, AFFECT_CAST_SPEED, POINT_CASTING_SPEED, GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true);
				break;

			case APPLY_ATT_GRADE_BONUS:
				AffectSystem::AddAffect(e, AFFECT_ATT_GRADE, POINT_ATT_GRADE_BONUS,
					GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true);
				break;

			case APPLY_DEF_GRADE_BONUS:
				AffectSystem::AddAffect(e, AFFECT_DEF_GRADE, POINT_DEF_GRADE_BONUS,
					GetItemValue(item, 2), 0, GetItemValue(item, 1), 0, true);
				break;
			}
		}

		if (ecs::SocialSystem::GetWarMap(e))
			ecs::SocialSystem::GetWarMap(e)->UsePotion(e, itemEntity);

		ConsumeItem(itemEntity);
		break;

		case USE_TALISMAN:
		{
			const int TOWN_PORTAL = 1;
			const int MEMORY_PORTAL = 2;


			// gm_guild_build, oxevent ¸Ê¿¡¼­ ±ÍÈ¯ºÎ ±ÍÈ¯±â¾ïºÎ ¸¦ »ç¿ë¸øÇÏ°Ô ¸·À½
			if (ecs::PlayerRuntime::GetMapIndex(e) == 200 || ecs::PlayerRuntime::GetMapIndex(e) == 113)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 489, "");
#endif
				return false;
			}

			if (CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e)) == true)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#ifdef ENABLE_NEWSTUFF
			else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && !IsAllowedPotionOnPVP(GetItemVnum(item)))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 667, "");
#endif
				return false;
			}
#endif

			if (ecs::PlayerRuntime::IsWarping(e))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 434, "");
#endif
				return false;
			}

			// CONSUME_LIFE_WHEN_USE_WARP_ITEM
			int consumeLife = CalculateConsume(self);

			if (consumeLife < 0)
				return false;
			// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

			if (GetItemValue(item, 0) == TOWN_PORTAL) // ±ÍÈ¯ºÎ
			{
				if (GetItemSocket(item, 0) == 0)
				{
					if (!ecs::SocialSystem::GetDungeon(e))
						if (!GiveRecallItem(e, item))
							return false;

					PIXEL_POSITION posWarp;

					if (ecs::GetRecallPosition(ecs::PlayerRuntime::GetMapIndex(e), ecs::PlayerRuntime::GetEmpire(e), posWarp))
					{
						// CONSUME_LIFE_WHEN_USE_WARP_ITEM
						ecs::PointSystem::Change(e, POINT_HP, -consumeLife, false);
						// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

						ecs::MovementSystem::WarpSet(e, posWarp.x, posWarp.y);
					}
					else
					{
						LOG_ERROR("CHARACTER::UseItem : cannot find spawn position (name {}, {} x {})", ecs::PlayerRuntime::GetName(e).data(), ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e));
					}
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					if (test_server) {
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 415, "");
					}
#endif
					ProcessRecallItem(e, item);
				}
			}
			else if (GetItemValue(item, 0) == MEMORY_PORTAL) // ±ÍÈ¯±â¾ïºÎ
			{
				if (GetItemSocket(item, 0) == 0)
				{
					if (ecs::SocialSystem::GetDungeon(e))
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 310, "%s", GetItemName(item));
#endif
						return false;
					}

					if (!GiveRecallItem(e, item))
						return false;
				}
				else
				{
					// CONSUME_LIFE_WHEN_USE_WARP_ITEM
					ecs::PointSystem::Change(e, POINT_HP, -consumeLife, false);
					// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM

					ProcessRecallItem(e, item);
				}
			}
		}
		break;
#ifdef ENABLE_ATTR_COSTUMES
		case USE_CHANGE_ATTR_COSTUME:
		case USE_ADD_ATTR_COSTUME1:
		case USE_ADD_ATTR_COSTUME2:
		case USE_REMOVE_ATTR_COSTUME:
		{
			if (!InventorySystem::IsValidItemPosition(e, DestCell))
				return false;
			const entt::entity character = e;
			const entt::entity target = GetItem(character, DestCell);
			const auto subtype = GetItemSubType(itemEntity);
			// The last material may be destroyed during the transaction.
			const uint32_t materialID = GetItemID(itemEntity);
			const uint32_t materialVnum = GetItemOriginalVnum(itemEntity);
			using Result = CostumeAttributeResult;
			const auto result = UseCostumeAttributeItem(character, target, itemEntity);
			if (result != Result::Success)
			{
#ifdef TEXTS_IMPROVEMENT
				uint32_t message = 0;
				switch (result)
				{
				case Result::InvalidTarget: message = 396; break;
				case Result::NoAttributes: message = 354; break;
				case Result::SlotsFull: message = 87; break;
				case Result::DuplicateAttribute: message = 88; break;
				case Result::NoRareAttributes: message = 89; break;
				default: break;
				}
				if (message)
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, message, "");
#endif
				return false;
			}
			const char* action = subtype == USE_CHANGE_ATTR_COSTUME ? "CHANGE_COSTUME_ATTR"
				: subtype == USE_REMOVE_ATTR_COSTUME ? "REMOVE_COSTUME_ATTR" : "ADD_COSTUME_ATTR";
			char buf[21];
			snprintf(buf, sizeof(buf), "%u", GetItemID(target));
			auto* desc = ecs::PlayerRuntime::GetDesc(character);
			LogManager::instance().ItemLog(ecs::PlayerRuntime::GetPlayerID(character),
				ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character),
				materialID, action, buf, desc ? desc->GetHostName() : "", materialVnum);
#ifdef TEXTS_IMPROVEMENT
			const uint32_t message = subtype == USE_CHANGE_ATTR_COSTUME ? 392
				: subtype == USE_REMOVE_ATTR_COSTUME ? 90 : 677;
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, message, "");
#endif
			break;
		}
#endif
#ifdef ENABLE_STOLE_COSTUME
		case USE_ENCHANT_STOLE: {
			if (!InventorySystem::IsValidItemPosition(e, DestCell))
				return false;
			const entt::entity character = e;
			const entt::entity target = GetItem(character, DestCell);
			if (!IsValidItem(target))
				return false;
			if (GetItemType(target) != ITEM_COSTUME || GetItemSubType(target) != COSTUME_STOLE) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 22, "%s", GetItemName(itemEntity));
#endif
				return false;
			}
			if (!EnchantStoleWithItemCost(character, target, itemEntity))
				return false;
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 21, "%s", GetItemName(target));
#endif
			break;
		}
#endif
#ifdef ENABLE_DS_ENCHANT
		case USE_DS_ENCHANT: {
			if (!InventorySystem::IsValidItemPosition(e, DestCell))
				return false;
			const entt::entity character = e;
			const entt::entity target = GetItem(character, DestCell);
			const uint32_t materialID = GetItemID(itemEntity);
			const uint32_t materialVnum = GetItemOriginalVnum(itemEntity);
			using Result = DSManager::EnchantResult;
			const auto result = DSManager::instance().EnchantWithItemCost(character, target, itemEntity);
			if (result != Result::Success) {
#ifdef TEXTS_IMPROVEMENT
				uint32_t message = 0;
				if (result == Result::InvalidTarget) message = 73;
				else if (result == Result::Active) message = 76;
				else if (result == Result::InvalidGrade) message = 75;
				if (message)
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, message, "");
#endif
				return false;
			}
			char buf[21];
			snprintf(buf, sizeof(buf), "%u", GetItemID(target));
			auto* desc = ecs::PlayerRuntime::GetDesc(character);
			LogManager::instance().ItemLog(ecs::PlayerRuntime::GetPlayerID(character),
				ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), materialID,
				"USE_DS_ENCHANT", buf, desc ? desc->GetHostName() : "", materialVnum);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 74, "");
#endif
			break;
		}
#endif
#ifdef ENABLE_REMOTE_ATTR_SASH_REMOVE
		case USE_ATTR_SASH_REMOVE: {
			entt::entity item2 = entt::null;
			if ((!InventorySystem::IsValidItemPosition(e, DestCell)) || ((item2 = GetItem(e, DestCell)) == entt::null))
				return false;

			if ((IsItemExchanging(item2)) || (IsItemEquipped(item2)))
				return false;

			if ((GetItemType(item2) == ITEM_COSTUME) && (GetItemSubType(item2) == COSTUME_ACCE)) {
				if (GetItemSocket(item2, ACCE_ABSORBED_SOCKET) <= 0) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 71, "");
#endif
					return false;
				}

				bool bClean = CleanAcceAttr(e, itemEntity, item2);
				if (bClean) {
					{
						char buf[21];
						snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
						LogManager::instance().ItemLogEntity(e, item, "USE_ATTR_SASH_REMOVE", buf);
					}

#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 72, "");
#endif
				}

				return bClean;
			}
			else {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 70, "");
#endif
				return false;
			}
		}
#endif
#ifdef ENABLE_NEW_PET_EDITS
		case USE_PET_REVIVE: {
			entt::entity item2 = entt::null;
			if ((!InventorySystem::IsValidItemPosition(e, DestCell)) || ((item2 = GetItem(e, DestCell)) == entt::null))
				return false;

			if ((GetItemVnum(item2) < 55701) || (GetItemVnum(item2) > 55711)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 66, "");
#endif
				return false;
			}

			if (GetItemSocket(item2, 0) != 0) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 67, "");
#endif
				return false;
			}

			if (GetItemSocket(item2, 2) == 0) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 64, "");
#endif
				return false;
			}

			if (IsItemExchanging(item2) || IsItemEquipped(item2)) // ENABLE_BUG_FIXES
				return false;

			if (GetItemSocket(item2, 1) > int(1440 * 365)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 69, "");
#endif
				return false;
			}

			int iLimit = int(1440 * 365);
			int iValue = GetItemValue(item, 0);
			int iNewDuration = iValue == 0 ? 1440 * 366 : 1440 * iValue;
			iNewDuration += GetItemSocket(item2, 1);
			if ((iNewDuration >= iLimit) && (GetItemVnum(item) != 86074)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 68, "");
#endif
				return false;
			}

			iNewDuration = iNewDuration > iLimit ? iLimit : iNewDuration;
			if (GetItemVnum(item) == 86074)
				iNewDuration = 1440 * 366;

			SetItemSocket(item2, 1, iNewDuration);
			SetItemSocket(item2, 2, iNewDuration);
			std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE player.new_petsystem SET duration = %d, tduration = %d WHERE id = %lu ", iNewDuration, iNewDuration, GetItemID(item2)));

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
				LogManager::instance().ItemLogEntity(e, item, "USE_PET_REVIVE", buf);
			}

#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 65, "");
#endif
			ConsumeItemEcs(itemEntity);
			break;
		}
		case USE_PET_ENCHANT: {
			entt::entity item2 = entt::null;
			if ((!InventorySystem::IsValidItemPosition(e, DestCell)) || ((item2 = GetItem(e, DestCell)) == entt::null))
				return false;

			if ((GetItemVnum(item2) < 55701) || (GetItemVnum(item2) > 55711)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 66, "");
#endif
				return false;
			}

			if (GetItemSocket(item2, 0) != 0) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 67, "");
#endif
				return false;
			}

			if (IsItemExchanging(item2) || IsItemEquipped(item2)) // ENABLE_BUG_FIXES
				return false;

			int idx = ecs::PlayerRuntime::GetPetEnchant(e);
			if ((idx < 0) || (idx > 2))
				return false;

			int iValue = GetItemAttributeValue(item2, idx);
			if ((idx == 0) && (iValue >= 150)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 63, "");
#endif
				return false;
			}

			if ((idx == 1) && (iValue >= 100)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 63, "");
#endif
				return false;
			}

			if ((idx == 2) && (iValue >= 100)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 63, "");
#endif
				return false;
			}

			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
				LogManager::instance().ItemLogEntity(e, item, "USE_PET_ENCHANT", buf);
			}

			if (number(1, 100) > 70) {
				int iMax;
				if (idx == 0)
					iMax = iValue + 5 > 150 ? 150 : iValue + 5;
				else
					iMax = iValue + 5 > 100 ? 100 : iValue + 5;

				SetItemForceAttributeEcs(item2, idx, 1, iMax);
				std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("UPDATE player.new_petsystem SET bonus%d = %d WHERE id = %lu ", idx, iMax, GetItemID(item2)));

#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 61, "");
#endif
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 62, "");
			}
#endif

			ConsumeItemEcs(itemEntity);
			break;
		}
#endif
		case USE_TUNING:
		case USE_DETACHMENT:
		{
			entt::entity item2 = entt::null;

			if (!InventorySystem::IsValidItemPosition(e, DestCell) || (item2 = GetItem(e, DestCell)) == entt::null)
				return false;

			if (IsItemExchanging(item2) || IsItemEquipped(item2)) // @fixme114
				return false;

			if (GetItemVnum(item2) >= 28330 && GetItemVnum(item2) <= 28343) // ¿µ¼®+3
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 678, "%s", GetItemName(item));
#endif
				return false;
			}

#ifdef ENABLE_BUG_FIXES
			if (IsItemEquipped(item2))
				return false;
#endif

#ifdef ENABLE_ACCE_SYSTEM
			if (GetItemValue(item, 0) == ACCE_CLEAN_ATTR_VALUE0)
			{
				if (!CleanAcceAttr(e, itemEntity, item2))
					return false;

				return true;
			}
#endif
			if (GetItemVnum(item2) >= 28430 && GetItemVnum(item2) <= 28443)  // ¿µ¼®+4
			{
				if (GetItemVnum(item) == 71056) // Ã»·æÀÇ¼û°á
				{
					RefineItem(e, item, item2);
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 679, "%s", GetItemName(item));
				}
#endif
			}
			else
			{
				RefineItem(e, item, item2);
			}
		}
		break;
#ifdef ATTR_LOCK						
		case USE_ADD_ATTRIBUTE_LOCK:
		case USE_CHANGE_ATTRIBUTE_LOCK:
		case USE_DELETE_ATTRIBUTE_LOCK:
		{
			if (!InventorySystem::IsValidItemPosition(e, DestCell))
				return false;
			const entt::entity character = e;
			const entt::entity target = GetItem(character, DestCell);
			const auto operation = GetItemSubType(itemEntity);
			using Result = AttributeLockResult;
			const auto result = UseItemAttributeLock(character, target, itemEntity);
			if (result != Result::Success)
			{
#ifdef TEXTS_IMPROVEMENT
				uint32_t message = 0;
				switch (result)
				{
				case Result::InvalidTarget:
					message = operation == USE_DELETE_ATTRIBUTE_LOCK ? 680 : 791;
					break;
				case Result::NotEnoughAttributes: message = 792; break;
				case Result::AlreadyLocked: message = 793; break;
				case Result::NotLocked: message = 795; break;
				default: break;
				}
				if (message)
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, message, "");
#endif
				return false;
			}
			break;
		}
#endif
		case USE_CHANGE_COSTUME_ATTR:
		case USE_RESET_COSTUME_ATTR:
		{
			const entt::entity item2 = GetItem(e, DestCell);
			if (!InventorySystem::IsValidItemPosition(e, DestCell) || !IsValidItem(item2))
				return false;

			if (ITEM_COSTUME != GetItemType(item2))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			{
				uint8_t bSubType = GetItemSubType(item2);
#ifdef ENABLE_ACCE_SYSTEM
				if (bSubType == COSTUME_ACCE)
					return false;
#endif

#ifdef ENABLE_STOLE_COSTUME
				if (bSubType == COSTUME_STOLE)
					return false;
#endif
			}

			if (IsItemExchanging(item2) || IsItemEquipped(item2)) // @fixme114
				return false;

			if (GetItemAttributeSetIndex(item2) == -1)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if (GetItemAttributeCount(item2) == 0)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 354, "");
#endif
				return false;
			}

			const uint32_t materialID = GetItemID(itemEntity);
			const uint32_t materialVnum = GetItemOriginalVnum(itemEntity);
			switch (GetItemSubType(itemEntity))
			{
			case USE_CHANGE_COSTUME_ATTR:
				if (!ChangeItemAttributeWithItemCost(item2, itemEntity))
					return false;
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
					LogManager::instance().ItemLog(ecs::PlayerRuntime::GetPlayerID(e), ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), materialID, "CHANGE_COSTUME_ATTR", buf, ecs::PlayerRuntime::GetDesc(e)->GetHostName(), materialVnum);
				}
				break;
			case USE_RESET_COSTUME_ATTR:
				if (!ResetCostumeAttributesWithItemCost(item2, itemEntity))
					return false;
				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
					LogManager::instance().ItemLog(ecs::PlayerRuntime::GetPlayerID(e), ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), materialID, "RESET_COSTUME_ATTR", buf, ecs::PlayerRuntime::GetDesc(e)->GetHostName(), materialVnum);
				}
				break;
			}

#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 392, "");
#endif
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
			const entt::entity item2 = GetItem(e, DestCell);
			if (!InventorySystem::IsValidItemPosition(e, DestCell) || !IsValidItem(item2))
				return false;


			// [NOTE] ÄÚ½ºÆ¬ ¾ÆÀÌ�
// Û¿¡´Â ¾ÆÀÌ�
// Û ÃÖÃÊ »ý¼º½Ã ·£´ý ¼Ó¼ºÀ» ºÎ¿©ÇÏµÇ, Àç°æÀç°¡ µîµîÀº ¸·¾Æ´Þ¶ó´Â ¿äÃ»ÀÌ ÀÖ¾úÀ½.
			// ¿ø·¡ ANTI_CHANGE_ATTRIBUTE °°Àº ¾ÆÀÌ�
// Û Flag¸¦ Ãß°¡ÇÏ¿© ±âÈ¹ ·¹º§¿¡¼­ À¯¿¬ÇÏ°Ô ÄÁÆ®·Ñ ÇÒ ¼ö ÀÖµµ·Ï ÇÒ ¿¹Á¤ÀÌ¾úÀ¸³ª
			// ±×µý°�
 // ÇÊ¿ä¾øÀ¸´Ï ´ÚÄ¡°í »¡¸® ÇØ´Þ·¡¼­ ±×³É ¿©±â¼­ ¸·À½... -_-
			if (ITEM_COSTUME == GetItemType(item2))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
				return false;
			}

			if (IsItemExchanging(item2) || IsItemEquipped(item2)) // @fixme114
				return false;

			switch (GetItemSubType(itemEntity))
			{
			case USE_CLEAN_SOCKET:
			{
				int i;
				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				{
					if (GetItemSocket(item2, i) == ITEM_BROKEN_METIN_VNUM)
						break;
				}

				if (i == ITEM_SOCKET_MAX_NUM)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 480, "");
#endif
					return false;
				}

				int j = 0;

				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				{
					if (GetItemSocket(item2, i) != ITEM_BROKEN_METIN_VNUM && GetItemSocket(item2, i) != 0)
						SetItemSocketEcs(item2, j++, GetItemSocket(item2, i));
				}

				for (; j < ITEM_SOCKET_MAX_NUM; ++j)
				{
					if (GetItemSocket(item2, j) > 0)
						SetItemSocketEcs(item2, j, 1);
				}

				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
					LogManager::instance().ItemLogEntity(e, itemEntity, "CLEAN_SOCKET", buf);
				}

				ConsumeItemEcs(itemEntity);

			}
			break;

			case USE_CHANGE_ATTRIBUTE:
			case USE_CHANGE_ATTRIBUTE2: // @fixme123
				if (GetItemAttributeSetIndex(item2) == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (GetItemAttributeCount(item2) == 0)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 354, "");
#endif
					return false;
				}

				if ((GM_PLAYER == ecs::PlayerRuntime::GetGMLevel(e)) && (false == test_server) && (g_dwItemBonusChangeTime > 0))
				{
					//
					// Event Flag ¸¦ �
// ëÇØ ÀÌÀü¿¡ ¾ÆÀÌ�
// Û ¼Ó¼º º¯°æÀ» ÇÑ ½Ã°£À¸·Î ºÎ�
// Í ÃæºÐÇÑ ½Ã°£ÀÌ Èê·¶´ÂÁö °Ë»çÇÏ°í
					// ½Ã°£ÀÌ ÃæºÐÈ÷ Èê·¶´Ù¸é ÇöÀç ¼Ó¼ºº¯°æ¿¡ ´ëÇÑ ½Ã°£À» ¼³Á¤ÇØ ÁØ´Ù.
					//

					// uint32_t dwChangeItemAttrCycle = quest::CQuestManager::instance().GetEventFlag(msc_szChangeItemAttrCycleFlag);
					// if (dwChangeItemAttrCycle < msc_dwDefaultChangeItemAttrCycle)
						// dwChangeItemAttrCycle = msc_dwDefaultChangeItemAttrCycle;
					uint32_t dwChangeItemAttrCycle = g_dwItemBonusChangeTime;

					quest::PC* pPC = quest::CQuestManager::instance().GetPC(ecs::PlayerRuntime::GetPlayerID(e));

					if (pPC)
					{
						uint32_t dwNowSec = get_global_time();

						uint32_t dwLastChangeItemAttrSec = pPC->GetFlag(CHARACTER::msc_szLastChangeItemAttrFlag);

						if (dwLastChangeItemAttrSec + dwChangeItemAttrCycle > dwNowSec)
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 391, "%d#%d", dwChangeItemAttrCycle, dwChangeItemAttrCycle - (dwNowSec - dwLastChangeItemAttrSec));
#endif
							return false;
						}

						pPC->SetFlag(CHARACTER::msc_szLastChangeItemAttrFlag, dwNowSec);
					}
				}

#ifdef ENABLE_CHANGE_ATTRIBUTE_RULES
				{
					uint32_t dwTargetVnum = GetItemVnum(item2);
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
						((dwTargetVnum >= 8640) && (dwTargetVnum <= 8739)))
						? true : false;


					if (GetItemVnum(itemEntity) != 86060) {
						if (bZodiacItem) {
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 10, "%s", GetItemName(item2));
#endif
							return false;
						}
					}
					else if (!bZodiacItem) {
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 9, "%s", GetItemName(item2));
#endif
						return false;
					}
#endif
				}

#endif

#ifdef ENABLE_TALISMAN_ATTR
				if (GetItemVnum(itemEntity) == 86051 || GetItemVnum(itemEntity) == 88965)
				{
					if (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_PENDANT)
					{
						if (!ChangeItemAttributeEcs(item2))
						    return false;
						ConsumeItemEcs(itemEntity);
#ifdef ENABLE_RANKING
						ecs::PlayerRuntime::SetRankPoints(e, 13, ecs::PlayerRuntime::GetRankPoints(e, 13) + 1);
#endif
						return true;
					}
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 681, "");
#endif
						return false;
					}
				}
				else if (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_PENDANT)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 850, "");
#endif
					return false;
				}
#endif

				if (GetItemSubType(itemEntity) == USE_CHANGE_ATTRIBUTE2)
				{
					int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
					{
						0, 0, 30, 40, 3
					};

					if (!ChangeItemAttributeEcs(item2, aiChangeProb))
					    return false;
				}
				else if (GetItemVnum(itemEntity) == 76014)
				{
					int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
					{
						0, 10, 50, 39, 1
					};

					if (!ChangeItemAttributeEcs(item2, aiChangeProb))
					    return false;
				}
				else
				{
					// ¿¬Àç°æ Æ¯¼öÃ³¸®
					// Àý´ë·Î ¿¬Àç°¡ Ãß°¡ ¾ÈµÉ°�
// ¶ó ÇÏ¿© ÇÏµå ÄÚµùÇÔ.

					if (GetItemVnum(itemEntity) == 71151 || GetItemVnum(itemEntity) == 76023)
					{
						if ((GetItemType(item2) == ITEM_WEAPON)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_BODY)
#ifdef __USE_ADD_WITH_ALL_ITEMS__
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_HEAD)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_SHIELD)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_WRIST)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_FOOTS)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_NECK)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_EAR)
#endif
							)
						{
							bool bCanUse = true;
							for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
							{
#ifdef __ENABLE_GREEN_ITEM_LVL_30__
								if (GetItemLimitType(item2, i) == LIMIT_LEVEL && GetItemLimitValue(item2, i) > 30)
#else
								if (GetItemLimitType(item2, i) == LIMIT_LEVEL && GetItemLimitValue(item2, i) > 40)
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
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 682, "%d", iLimit);
#endif
								break;
							}
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 683, "");
#endif
							break;
						}
					}


#ifdef ENABLE_BATTLE_PASS
					uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(e);
					if (bBattlePassId)
					{
						uint32_t dwItemVnum, dwUseCount;
						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM, &dwItemVnum, &dwUseCount))
						{
							if (dwItemVnum == GetItemVnum(itemEntity) && ecs::PlayerRuntime::GetMissionProgress(e, USE_ITEM, bBattlePassId) < dwUseCount)
								ecs::PlayerRuntime::UpdateMissionProgress(e, USE_ITEM, bBattlePassId, 1, dwUseCount);
						}

						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM1, &dwItemVnum, &dwUseCount))
						{
							if (dwItemVnum == GetItemVnum(itemEntity) && ecs::PlayerRuntime::GetMissionProgress(e, USE_ITEM1, bBattlePassId) < dwUseCount)
								ecs::PlayerRuntime::UpdateMissionProgress(e, USE_ITEM1, bBattlePassId, 1, dwUseCount);
						}

						if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, USE_ITEM2, &dwItemVnum, &dwUseCount))
						{
							if (dwItemVnum == GetItemVnum(itemEntity) && ecs::PlayerRuntime::GetMissionProgress(e, USE_ITEM2, bBattlePassId) < dwUseCount)
								ecs::PlayerRuntime::UpdateMissionProgress(e, USE_ITEM2, bBattlePassId, 1, dwUseCount);
						}
					}
#endif
					if (!ChangeItemAttributeEcs(item2))
					    return false;
				}

#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 392, "");
#endif

				{
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
					LogManager::instance().ItemLogEntity(e, itemEntity, "CHANGE_ATTRIBUTE", buf);
				}

				ConsumeItemEcs(itemEntity);
#ifdef ENABLE_RANKING
				if (GetItemVnum(itemEntity) == 86051 || GetItemVnum(itemEntity) == 88965)
					ecs::PlayerRuntime::SetRankPoints(e, 13, ecs::PlayerRuntime::GetRankPoints(e, 13) + 1);
				else
					ecs::PlayerRuntime::SetRankPoints(e, 12, ecs::PlayerRuntime::GetRankPoints(e, 12) + 1);
#endif
				break;

			case USE_ADD_ATTRIBUTE:
				if (GetItemAttributeSetIndex(item2) == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				if (GetItemAttributeCount(item2) < 5)
				{
#ifdef ENABLE_TALISMAN_ATTR
					if (GetItemVnum(itemEntity) == 86050 || GetItemVnum(itemEntity) == 88966) {
						if (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_PENDANT)
						{
#if defined(ENABLE_BUG_FIXES)
							if (GetItemAttributeCount(item2) == 4)
							{
#if defined(TEXTS_IMPROVEMENT)
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 1359, "");
#endif
								return false;
							}
#endif

							AddItemAttributeEcs(item2);
							ConsumeItemEcs(itemEntity);
							return true;
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 681, "");
#endif
							return false;
						}
					}
					else if (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_PENDANT)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 684, "");
#endif
						return false;
					}
#endif

					// ¿¬Àç°¡ Æ¯¼öÃ³¸®
					// Àý´ë·Î ¿¬Àç°¡ Ãß°¡ ¾ÈµÉ°�
// ¶ó ÇÏ¿© ÇÏµå ÄÚµùÇÔ.
					if (GetItemVnum(itemEntity) == 71152 || GetItemVnum(itemEntity) == 76024)
					{
						if ((GetItemType(item2) == ITEM_WEAPON)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_BODY)
#ifdef __USE_ADD_WITH_ALL_ITEMS__
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_HEAD)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_SHIELD)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_WRIST)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_FOOTS)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_NECK)
							|| (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_EAR)
#endif
							)
						{
							bool bCanUse = true;
							for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
							{
#ifdef __ENABLE_GREEN_ITEM_LVL_30__
								if (GetItemLimitType(item2, i) == LIMIT_LEVEL && GetItemLimitValue(item2, i) > 30)
#else
								if (GetItemLimitType(item2, i) == LIMIT_LEVEL && GetItemLimitValue(item2, i) > 40)
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
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 682, "%d", iLimit);
#endif
								break;
							}
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 683, "");
#endif
							break;
						}
					}
					char buf[21];
					snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
#ifndef ENABLE_ENCHANT_CHANGES
					if (number(1, 100) <= aiItemAttributeAddPercent[GetItemAttributeCount(item2)])
#endif
					{
#ifdef ENABLE_MAX_ADD_ATTRIBUTE
						short AttributeCount = abs(1 - GetItemAttributeCount(itemEntity));//1 bonuszt ad hozz?a z?d er?
						for (int i = 0; i < AttributeCount; i++)
							AddItemAttributeEcs(item2);
						ConsumeItemEcs(itemEntity);// elvesz 1 db ot
#else
						AddItemAttributeEcs(item2);
#endif
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 389, "");
#endif
						int iAddedIdx = GetItemAttributeCount(item2) - 1;
						LogManager::instance().ItemLog(
							ecs::PlayerRuntime::GetPlayerID(e),
							GetItemAttributeType(item2, iAddedIdx),
							GetItemAttributeValue(item2, iAddedIdx),
							GetItemID(itemEntity),
							"ADD_ATTRIBUTE_SUCCESS",
							buf,
							ecs::PlayerRuntime::GetDesc(e)->GetHostName(),
							GetItemOriginalVnum(itemEntity));
					}
#ifndef ENABLE_ENCHANT_CHANGES
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 390, "");
#endif
						LogManager::instance().ItemLogEntity(e, itemEntity, "ADD_ATTRIBUTE_FAIL", buf);
					}

					ConsumeItemEcs(itemEntity);
#endif
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 308, "");
				}
#endif
				break;

			case USE_ADD_ATTRIBUTE2:
				// Ãàº¹ÀÇ ±¸½½
				// Àç°¡ºñ¼­¸¦ �
// ëÇØ ¼Ó¼ºÀ» 4°³ Ãß°¡ ½Ã�
// ² ¾ÆÀÌ�
// Û¿¡ ´ëÇØ¼­ ÇÏ³ªÀÇ ¼Ó¼ºÀ» ´õ ºÙ¿©ÁØ´Ù.
				if (GetItemAttributeSetIndex(item2) == -1)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 396, "");
#endif
					return false;
				}

				// ¼Ó¼ºÀÌ ÀÌ¹Ì 4°³ Ãß°¡ µÇ¾úÀ» ¶§¸¸ ¼Ó¼ºÀ» Ãß°¡ °¡´ÉÇÏ´Ù.
				if (GetItemAttributeCount(item2) == 4)
				{
#ifdef ENABLE_TALISMAN_ATTR
					if (GetItemVnum(itemEntity) == 86052 || GetItemVnum(itemEntity) == 88964)
					{
						if (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_PENDANT)
						{
							if (number(1, 100) <= 75) // % Successo di inserimeno Sfera Benedetta 75%
							{
								AddItemAttributeEcs(item2);
								ConsumeItemEcs(itemEntity);
								return true;
							}
							else
							{
								ConsumeItemEcs(itemEntity);
#ifdef TEXTS_IMPROVEMENT
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 390, "");
#endif
								return false;
							}
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 681, "");
#endif
							return false;
						}
					}
					else if (GetItemType(item2) == ITEM_ARMOR && GetItemSubType(item2) == ARMOR_PENDANT)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 684, "");
#endif
						return false;
					}
#endif


					char buf[21];
					snprintf(buf, sizeof(buf), "%u", GetItemID(item2));

					if (number(1, 100) <= aiItemAttributeAddPercent[GetItemAttributeCount(item2)])
					{
#ifdef ENABLE_MAX_ADD_ATTRIBUTE
						short AttributeCount = abs(1 - GetItemAttributeCount(itemEntity));
						for (int i = 0; i < AttributeCount; i++)
							AddItemAttributeEcs(item2);
#else
						AddItemAttributeEcs(item2);
#endif
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 389, "");
#endif
						int iAddedIdx = GetItemAttributeCount(item2) - 1;
						LogManager::instance().ItemLog(
							ecs::PlayerRuntime::GetPlayerID(e),
							GetItemAttributeType(item2, iAddedIdx),
							GetItemAttributeValue(item2, iAddedIdx),
							GetItemID(itemEntity),
							"ADD_ATTRIBUTE2_SUCCESS",
							buf,
							ecs::PlayerRuntime::GetDesc(e)->GetHostName(),
							GetItemOriginalVnum(itemEntity));
					}
					else
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 390, "");
#endif
						LogManager::instance().ItemLogEntity(e, itemEntity, "ADD_ATTRIBUTE2_FAIL", buf);
					}

					ConsumeItemEcs(itemEntity);
				}
				else if (GetItemAttributeCount(item2) == 5)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 308, "");
#endif
				}
				else if (GetItemAttributeCount(item2) < 4)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 339, "%d#%d#%d", 4, GetItemAttributeCount(item2), 4);
#endif
				}
				else
				{
					// wtf ?!
					LOG_ERROR("ADD_ATTRIBUTE2 : Item has wrong AttributeCount({})", GetItemAttributeCount(item2));
				}
				break;

			case USE_ADD_ACCESSORY_SOCKET:
			{
				char buf[21];
				snprintf(buf, sizeof(buf), "%u", GetItemID(item2));
				if (GetItemType(item2) == ITEM_BELT)
				{
					ecs::ChatSystem::Send(e, CHAT_TYPE_INFO, "You can't add new slot's to belt items");
					return false;
					
				}
				if (IsAccessoryForSocket(item2))
				{
					if (GetItemAccessorySocketMaxGrade(item2) < ITEM_ACCESSORY_SOCKET_MAX_NUM)
					{
#ifdef ENABLE_ADDSTONE_FAILURE
						if (number(1, 100) <= 50)
#else
						if (1)
#endif
						{
							SetItemAccessorySocketMaxGrade(item2, GetItemAccessorySocketMaxGrade(item2) + 1);
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 387, "");
#endif
							LogManager::instance().ItemLogEntity(e, itemEntity, "ADD_SOCKET_SUCCESS", buf);
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 386, "");
#endif
							LogManager::instance().ItemLogEntity(e, itemEntity, "ADD_SOCKET_FAIL", buf);
						}

						ConsumeItemEcs(itemEntity);
					}
#ifdef TEXTS_IMPROVEMENT
					else {
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 428, "");
					}
#endif
				}
				else
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 425, "");
#endif
				}
			}
			break;

			case USE_PUT_INTO_BELT_SOCKET:
			case USE_PUT_INTO_ACCESSORY_SOCKET:
				if (IsAccessoryForSocket(item2))
				{
					if (CanPutInto(itemEntity, item2)) {
#ifdef ENABLE_INFINITE_RAFINES
						if (GetItemSocket(item2, 0) > 86400 || GetItemSocket(item2, 1) > 86400 || GetItemSocket(item2, 2) > 86400) {
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 859, "");
#endif
							return false;
						}
#endif
						char buf[21];
						snprintf(buf, sizeof(buf), "%u", GetItemID(item2));

						if (GetItemAccessorySocketGrade(item2) < GetItemAccessorySocketMaxGrade(item2))
						{
							//if (number(1, 100) <= aiAccessorySocketPutPct[GetItemAccessorySocketGrade(item2)])
							//{
							SetItemAccessorySocketGrade(item2, GetItemAccessorySocketGrade(item2) + 1);
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 452, "");
#endif
							LogManager::instance().ItemLogEntity(e, itemEntity, "PUT_SOCKET_SUCCESS", buf);
							//}
							//else
							//{
//#ifdef TEXTS_IMPROVEMENT
													//ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 453, "");
//#endif
													//LogManager::instance().ItemLogEntity(e, itemEntity, "PUT_SOCKET_FAIL", buf);
												//}

							ConsumeItemEcs(itemEntity);
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							if (GetItemAccessorySocketMaxGrade(item2) == 0 || GetItemAccessorySocketMaxGrade(item2) < ITEM_ACCESSORY_SOCKET_MAX_NUM) {
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 297, "");
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 298, "");
							}
							else {
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 337, "");
							}
#endif
						}
					}
#ifdef ENABLE_INFINITE_RAFINES
					else if (CanPutInto2(itemEntity, item2)) {
						if ((GetItemSocket(item2, 0) > 5 && GetItemSocket(item2, 0) <= 86400) || (GetItemSocket(item2, 1) > 5 && GetItemSocket(item2, 1) <= 86400) || (GetItemSocket(item2, 2) > 5 && GetItemSocket(item2, 2) <= 86400)) {
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 860, "");
#endif
							return false;
						}

						char buf[21];
						snprintf(buf, sizeof(buf), "%u", GetItemID(item2));

						if (GetItemAccessorySocketGrade(item2) < GetItemAccessorySocketMaxGrade(item2))
						{
							bool infinite = GetItemValue(item, 0) == 1 ? true : false;
							if (infinite == true)
							{
								SetItemAccessorySocketGrade(item2, GetItemAccessorySocketGrade(item2) + 1, infinite);
#ifdef TEXTS_IMPROVEMENT
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 452, "");
#endif
								LogManager::instance().ItemLogEntity(e, itemEntity, "PUT_SOCKET_SUCCESS", buf);
							}
							else
							{
#ifdef TEXTS_IMPROVEMENT
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 453, "");
#endif
								LogManager::instance().ItemLogEntity(e, itemEntity, "PUT_SOCKET_FAIL", buf);
							}

							ConsumeItemEcs(itemEntity);
						}
						else
						{
#ifdef TEXTS_IMPROVEMENT
							if (GetItemAccessorySocketMaxGrade(item2) == 0 || GetItemAccessorySocketMaxGrade(item2) < ITEM_ACCESSORY_SOCKET_MAX_NUM) {
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 297, "");
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 298, "");
							}
							else {
								ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 337, "");
							}
#endif
						}
					}
#endif
					else {
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 425, "");
					}
				}
				else {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 425, "");
#endif
				}
				break;
			}

		}
		break;
		//  END_OF_ACCESSORY_REFINE & END_OF_ADD_ATTRIBUTES & END_OF_CHANGE_ATTRIBUTES

		case USE_BAIT:
		{

			if (ecs::PlayerRuntime::GetCharEvent(e, ecs::PlayerRuntime::CharEvent::Fishing)
#ifdef ENABLE_NEW_FISHING_SYSTEM
				|| ActivitySystem::IsFishing(e)
#endif
				)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 277, "");
#endif
				return false;
			}

			const entt::entity weapon = GetWearItem(e, WEAR_WEAPON);

			if (weapon == entt::null || GetItemType(weapon) != ITEM_ROD)
				return false;

#ifdef TEXTS_IMPROVEMENT
			if (GetItemSocket(weapon, 2)) {
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 898, "%s", GetItemName(item));
			}
			else {
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 282, "%s", GetItemName(item));
			}
#endif
			SetItemSocketEcs(weapon, 2, GetItemValue(item, 0));
			ConsumeItemEcs(itemEntity);
		}
		break;

		case USE_MOVE:
		case USE_TREASURE_BOX:
		case USE_MONEYBAG:
			break;

		case USE_AFFECT:
		{
			if (AffectSystem::FindAffect(e, GetItemValue(item, 0), aApplyInfo[GetItemValue(item, 1)].bPointType)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 442, "");
#endif
			}
			else
			{
				// PC_BANG_ITEM_ADD
				if (IsItemPCBangItem(item) == true)
				{
					// PC¹æÀÎÁö Ã¼�
// ©ÇØ¼­ Ã³¸®
					if (CPCBangManager::instance().IsPCBangIP(ecs::PlayerRuntime::GetDesc(e)->GetHostName()) == false)
					{
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 426, "");
#endif
						return false;
					}
				}
				// END_PC_BANG_ITEM_ADD

				AffectSystem::AddAffect(e, GetItemValue(item, 0), aApplyInfo[GetItemValue(item, 1)].bPointType, GetItemValue(item, 2), 0, GetItemValue(item, 3), 0, false);
				ConsumeItemEcs(itemEntity);
			}
		}
		break;

		case USE_CREATE_STONE:
			AutoGiveItemEcs(e, number(28000, 28013));
			ConsumeItemEcs(itemEntity);
			break;

			// ¹°¾à Á¦Á¶ ½º�
// ³¿ë ·¹½ÃÇÇ Ã³¸®
		case USE_RECIPE:
		{
			const entt::entity pSource1 = FindSpecifyItem(e, GetItemValue(item, 1)
#ifdef ENABLE_EXTRA_INVENTORY
				, false
#endif
			);
			int dwSourceCount1 = GetItemValue(item, 2);

			const entt::entity pSource2 = FindSpecifyItem(e, GetItemValue(item, 3)
#ifdef ENABLE_EXTRA_INVENTORY
				, false
#endif
			);
			int dwSourceCount2 = GetItemValue(item, 4);

			if (dwSourceCount1 != 0)
			{
				if (pSource1 == entt::null)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 350, "");
#endif
					return false;
				}
			}

			if (dwSourceCount2 != 0)
			{
				if (pSource2 == entt::null)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 350, "");
#endif
					return false;
				}
			}

			if (pSource1 != entt::null)
			{
				if (GetItemCount(pSource1) < dwSourceCount1)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 454, "%s#%d#%d", GetItemName(pSource1), dwSourceCount1, GetItemCount(pSource1));
#endif
					return false;
				}

				ConsumeItemEcs(pSource1, dwSourceCount1);
			}

			if (pSource2 != entt::null)
			{
				if (GetItemCount(pSource2) < dwSourceCount2)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 454, "%s#%d#%d", GetItemName(pSource2), dwSourceCount1, GetItemCount(pSource2));
#endif
					return false;
				}

				ConsumeItemEcs(pSource2, dwSourceCount2);
			}

			const entt::entity pBottle = FindSpecifyItem(e, 50901
#ifdef ENABLE_EXTRA_INVENTORY
				, false
#endif
			);

			if (pBottle == entt::null || GetItemCount(pBottle) < 1)
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 359, "");
#endif
				return false;
			}

			ConsumeItemEcs(pBottle);

			if (number(1, 100) > GetItemValue(item, 5))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 347, "");
#endif
				return false;
			}

			AutoGiveItemEcs(e, GetItemValue(item, 0));
		}
		break;
		}
	}
	break;
	case ITEM_METIN:
	{
		entt::entity item2 = entt::null;

		if (!InventorySystem::IsValidItemPosition(e, DestCell) || (item2 = GetItem(e, DestCell)) == entt::null)
			return false;

		if (IsItemExchanging(item2) || IsItemEquipped(item2)) // @fixme114
			return false;

		if (GetItemType(item2) == ITEM_PICK) return false;
		if (GetItemType(item2) == ITEM_ROD) return false;

		int i;

		for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			uint32_t dwVnum;

			if ((dwVnum = GetItemSocket(item2, i)) <= 2)
				continue;

			TItemTable* p = ITEM_MANAGER::instance().GetTable(dwVnum);

			if (!p)
				continue;
#ifdef KET_BONUSZOS_KOVEK
			const int32_t insV5 = GetItemValue(item, 5);
			const int32_t insV4 = GetItemValue(item, 4);

			
				const int32_t exV5 = p->alValues[5];
			const int32_t exV4 = p->alValues[4];

			// Ha barmelyi ko csopi egyezik barmelyikkel, akkor ne lehessen berakni csak ryuganak seggbe
			if ((insV5 && (insV5 == exV5 || insV5 == exV4)) ||
				(insV4 && (insV4 == exV5 || insV4 == exV4)))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 230, "");
#endif
				return false;
			}


#else
			if (GetItemValue(item, 5) == p->alValues[5])
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 230, "");
#endif
				return false;
			}
#endif
		}

		if (GetItemType(item2) == ITEM_ARMOR)
		{
			if (!IS_SET(GetItemWearFlag(item), WEARABLE_BODY) || !IS_SET(GetItemWearFlag(item2), WEARABLE_BODY))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 420, "%s", GetItemName(item));
#endif
				return false;
			}
		}
		else if (GetItemType(item2) == ITEM_WEAPON)
		{
			if (!IS_SET(GetItemWearFlag(item), WEARABLE_WEAPON))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 419, "%s", GetItemName(item));
#endif
				return false;
			}
		}
		else
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 357, "");
#endif
			return false;
		}

		for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			if (GetItemSocket(item2, i) >= 1 && GetItemSocket(item2, i) <= 2 && GetItemSocket(item2, i) >= GetItemValue(item, 2))
			{
				// ¼® È®·ü
#ifdef ENABLE_ADDSTONE_FAILURE
				if (number(1, 100) <= stone_chance) // Erfolgreich
#else
				if (number(1, 100) <= stone_chance) // Erfolgreich
#endif
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 340, "");
#endif
					SetItemSocketEcs(item2, i, GetItemVnum(item));
				}
				else
				{
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 341, "");
					SetItemSocketEcs(item2, i, ITEM_BROKEN_METIN_VNUM);
				}

				LogManager::instance().ItemLogEntity(e, item2, "SOCKET", GetItemName(item));
#ifdef ENABLE_BUG_FIXES
				ConsumeItemEcs(itemEntity);
#else
#ifdef ENABLE_STONE_STACKFIX
				ConsumeItemEcs(itemEntity);
#else
				ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (METIN)");
#endif
#endif
				break;
			}

		if (i == ITEM_SOCKET_MAX_NUM)
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 357, "%s", GetItemName(item2));
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
		if (!IsItemEquipped(item))
			EquipItemEcs(e, item);
	}
	break;

	case ITEM_BLEND:
		// »õ·Î¿î ¾àÃÊµé
		LOG_INFO("ITEM_BLEND!!");
		if (Blend_Item_find(GetItemVnum(item)))
		{
			int		affect_type = AFFECT_BLEND;
			int		apply_type = aApplyInfo[GetItemSocket(item, 0)].bPointType;
			int		apply_value = GetItemSocket(item, 1);
			int		apply_duration = GetItemSocket(item, 2);

			if (AffectSystem::FindAffect(e, affect_type, apply_type)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 442, "");
#endif
			}
			else
			{
				if (AffectSystem::FindAffect(e, AFFECT_EXP_BONUS_EURO_FREE, POINT_RESIST_MAGIC)) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 442, "");
#endif
				}
				else
				{
#ifdef ENABLE_BUG_FIXES
					if (!AffectSystem::IsLoaded(e)) {
						return false;
					}
#endif

					AffectSystem::AddAffect(e, affect_type, apply_type, apply_value, 0, apply_duration, 0, false);
					ConsumeItemEcs(itemEntity);
				}
			}
		}
		break;
	case ITEM_EXTRACT:
	{
		const entt::entity pDestItem = GetItem(e, DestCell);
		if (pDestItem == entt::null)
		{
			return false;
		}
		switch (GetItemSubType(item))
		{
		case EXTRACT_DRAGON_SOUL:
			if (IsDragonSoulItem(pDestItem))
			{
				entt::entity destItemEntity = pDestItem;
				return DSManager::instance().PullOutEcs(e, NPOS, destItemEntity, itemEntity);
			}
			return false;
		case EXTRACT_DRAGON_HEART:
			if (IsDragonSoulItem(pDestItem))
			{
				return DSManager::instance().ExtractDragonHeartEcs(e, pDestItem, itemEntity);
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
		int iCurrentMinutes = (GetItemSocket(item, 2) / 10000);
		int iCurrentStrike = (GetItemSocket(item, 2) % 10000);

		if (iCurrentMinutes < 60)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 685, "");
#endif
			return false;
		}

		if (iCurrentStrike <= 0)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 686, "");
#endif
			return false;
		}

		uint8_t bSoulType = GetItemSubType(item);
		if (bSoulType >= SOUL_MAX_NUM)
			return false;

		int iAffectID = AFFECT_SOUL_RED + bSoulType;
		int iAffID = AFF_SOUL_RED + bSoulType;

		bool blockUse = false;
		const CAffect* pAffect = AffectSystem::FindAffect(e, iAffectID);
		if (pAffect)
		{
			uint32_t dwSPCost = pAffect->lSPCost;
			if (GetItemID(item) == dwSPCost)
			{
				blockUse = true;
			}

			const entt::entity currentItem = FindItemByID(e, pAffect->lSPCost);
			if (currentItem != entt::null)
			{
				LockItem(currentItem, false);
				SetItemSocketEcs(currentItem, 1, false);
			}

			AffectSystem::RemoveAffect(e, const_cast<CAffect*>(pAffect));
		}

		if (!blockUse)
		{
			LockItem(item, true);
			SetItemSocketEcs(itemEntity, 1, true);

			AffectSystem::AddAffect(e, iAffectID, APPLY_NONE, 0, iAffID, INFINITE_AFFECT_DURATION, GetItemID(item), true, false);
		}
	}
	break;
#endif

	case ITEM_NONE:
		LOG_ERROR("Item type NONE {}", GetItemName(item));
		break;

	default:
		LOG_INFO("UseItemEx: Unknown type {} {}", GetItemName(item), GetItemType(item));
		return false;
	}

	return true;
}

} // namespace ItemSystem

namespace ItemSystem {

// Changing hair with a hair item.
bool ItemProcess_Hair(entt::entity e, entt::entity item, int iDestCell)
{
	if (CheckItemUseLevel(item, ecs::PointSystem::GetLevel(e)) == false)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 405, "");
#endif
		return false;
	}

	uint32_t hair = GetItemVnum(item);

	switch (ecs::PlayerRuntime::GetJob(e))
	{
	case JOB_WARRIOR:
		hair -= 72000; // 73001 - 72000 = 1001 ºÎ�
// Í Çì¾î ¹øÈ£ ½ÃÀÛ
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
	default:
		return false;
		break;
	}

	if (hair == ecs::PlayerRuntime::GetPart(e, PART_HAIR))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 311, "");
#endif
		return true;
	}

	ConsumeItemEcs(item);

	ecs::PlayerRuntime::SetPart(e, PART_HAIR, hair);
	NetworkSyncSystem::UpdatePacket(e);

	return true;
}

// Turning into a monster with a polymorph ball or book.
bool ItemProcess_Polymorph(entt::entity e, entt::entity item)
{
#ifdef ENABLE_PVP_ADVANCED
	if ((ecs::PlayerRuntime::GetDuelOption(e, "BlockPoly")))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 516, "");
#endif
		return false;
	}
#endif

	if (AffectSystem::IsPolymorphed(e)) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 437, "");
#endif
		return false;
	}

	if (true == MountSystem::IsRiding(e))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 741, "");
#endif
		return false;
	}

	uint32_t dwVnum = GetItemSocket(item, 0);

	if (dwVnum == 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 450, "");
#endif
		ConsumeItemEcs(item);
		return false;
	}

	const CMob* pMob = CMobManager::instance().Get(dwVnum);

	if (pMob == nullptr)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 451, "");
#endif
		ConsumeItemEcs(item);
		return false;
	}

	switch (GetItemVnum(item))
	{
	case 70104:
	case 70105:
	case 70106:
	case 70107:
	case 71093:
	{
		// µÐ°©±¸ Ã³¸®
		LOG_INFO("USE_POLYMORPH_BALL PID({}) vnum({})", ecs::PlayerRuntime::GetPlayerID(e), dwVnum);

		// ·¹º§ Á¦ÇÑ Ã¼�
// ©
		int iPolymorphLevelLimit = std::max(0, 20 - ecs::PointSystem::GetLevel(e) * 3 / 10);
		if (pMob->m_table.bLevel >= ecs::PointSystem::GetLevel(e) + iPolymorphLevelLimit)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 275, "");
#endif
			return false;
		}

		int iDuration = SkillSystem::GetSkillLevel(e, POLYMORPH_SKILL_ID) == 0 ? 5 : (5 + (5 + SkillSystem::GetSkillLevel(e, POLYMORPH_SKILL_ID) / 40 * 25));
		iDuration *= 60;

		uint32_t dwBonus = 0;

		dwBonus = (2 + SkillSystem::GetSkillLevel(e, POLYMORPH_SKILL_ID) / 40) * 100;

		AffectSystem::AddAffect(e, AFFECT_POLYMORPH, POINT_POLYMORPH, dwVnum, AFF_POLYMORPH, iDuration, 0, true);
		AffectSystem::AddAffect(e, AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonus, AFF_POLYMORPH, iDuration, 0, false);

		ConsumeItemEcs(item);
	}
	break;

	case 50322:
	{
		// º¸·ù

		// µÐ°©¼­ Ã³¸®
		// ¼ÒÄÏ0                ¼ÒÄÏ1           ¼ÒÄÏ2
		// µÐ°©ÇÒ ¸ó½º�
// Í ¹øÈ£   ¼ö·ÃÁ¤µµ        µÐ°©¼­ ·¹º§
		LOG_INFO("USE_POLYMORPH_BOOK: {}({}) vnum({})", ecs::PlayerRuntime::GetName(e).data(), ecs::PlayerRuntime::GetPlayerID(e), dwVnum);

		const entt::entity polymorphItem = item;
		if (CPolymorphUtils::instance().PolymorphCharacter(e, polymorphItem, pMob) == true)
		{
			CPolymorphUtils::instance().UpdateBookPracticeGrade(e, polymorphItem);
		}
		else
		{
		}
	}
	break;

	default:
		LOG_ERROR("POLYMORPH invalid item passed PID({}) vnum({})", ecs::PlayerRuntime::GetPlayerID(e), GetItemOriginalVnum(item));
		return false;
	}

	return true;
}

} // namespace ItemSystem

namespace ItemSystem {

// Stamping a recall scroll with where its owner is standing.
bool GiveRecallItem(entt::entity e, entt::entity item)
{
	int idx = ecs::PlayerRuntime::GetMapIndex(e);
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

	if (iEmpireByMapIndex && ecs::PlayerRuntime::GetEmpire(e) != iEmpireByMapIndex)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 270, "");
#endif
		return false;
	}

	int pos;

	if (GetItemCount(item) == 1)	// ¾ÆÀÌ�
// ÛÀÌ ÇÏ³ª¶ó¸é ±×³É ¼ÂÆÃ.
	{
		SetItemSocket(item, 0, ecs::PlayerRuntime::GetX(e));
		SetItemSocket(item, 1, ecs::PlayerRuntime::GetY(e));
	}
	else if ((pos = InventorySystem::GetEmptyInventory(e, GetItemSize(item))) != -1) // ±×·¸Áö ¾Ê´Ù¸é ´Ù¸¥ ÀÎº¥�
// ä¸® ½½·ÔÀ» Ã£´Â´Ù.
	{
		const entt::entity item2 = ITEM_MANAGER::instance().CreateItem(GetItemVnum(item), 1);

		if (IsValidItem(item2))
		{
			SetItemSocket(item2, 0, ecs::PlayerRuntime::GetX(e));
			SetItemSocket(item2, 1, ecs::PlayerRuntime::GetY(e));
			InventorySystem::AddToCharacter(item2, e, TItemPos(INVENTORY, pos));

			ConsumeItemEcs(item);
		}
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 366, "");
#endif
		return false;
	}

	return true;
}

// Warping to where a recall scroll was stamped.
void ProcessRecallItem(entt::entity e, entt::entity item)
{
	int idx;

	if ((idx = ecs::MapIndexAt(GetItemSocket(item, 0), GetItemSocket(item, 1))) == 0)
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
		if (ecs::PointSystem::GetLevel(e) < 90)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 325, "%d", 90);
#endif
			return;
		}
		else
			break;
	}

	if (iEmpireByMapIndex && ecs::PlayerRuntime::GetEmpire(e) != iEmpireByMapIndex)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 270, "");
#endif
		SetItemSocket(item, 0, 0);
		SetItemSocket(item, 1, 0);
	}
	else
	{
		LOG_INFO("Recall: {} {} {} -> {} {}", ecs::PlayerRuntime::GetName(e).data(), ecs::PlayerRuntime::GetX(e), ecs::PlayerRuntime::GetY(e), GetItemSocket(item, 0), GetItemSocket(item, 1));
		ecs::MovementSystem::WarpSet(e, GetItemSocket(item, 0), GetItemSocket(item, 1));
		ConsumeItemEcs(item);
	}
}

// Handing gold to a character, and crediting the farm-yang mission.
void GiveGold(entt::entity e, int64_t iAmount)
{
	if (iAmount <= 0)
		return;

	LOG_INFO("GIVE_GOLD: {} {}", ecs::PlayerRuntime::GetName(e).data(), iAmount);
	//#ifdef TEXTS_IMPROVEMENT
	//	ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 3, "%lld", iAmount);
	//#endif

#ifdef ENABLE_BATTLE_PASS
	uint8_t bBattlePassId = ecs::PlayerRuntime::GetBattlePassId(e);
	if (bBattlePassId)
	{
		uint32_t dwYangCount, dwNotUsed;
		if (CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, FARM_YANG, &dwNotUsed, &dwYangCount))
		{
			if (ecs::PlayerRuntime::GetMissionProgress(e, FARM_YANG, bBattlePassId) < dwYangCount)
				ecs::PlayerRuntime::UpdateMissionProgress(e, FARM_YANG, bBattlePassId, iAmount, dwYangCount);
		}
	}
#endif

	/*
	// PARTY GOLD SPLIT -  kikommentelve


	if (ecs::SocialSystem::GetParty(e))
	{
		LPPARTY pParty = ecs::SocialSystem::GetParty(e);

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

		ecs::PointSystem::Change(e, POINT_GOLD, dwMyAmount, true);

		if (dwMyAmount > 1000)
		{
			LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(e, dwMyAmount, "GET_GOLD", ""));
		}
	}
	else
	{
		ecs::PointSystem::Change(e, POINT_GOLD, iAmount, true);

		if (iAmount > 1000)
		{
			LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(e, iAmount, "GET_GOLD", ""));
		}
	}
	*/

	// Mindig csak az kapja a goldot, akihez a ItemSystem::GiveGold(GetEntityHandle(), ) meghivodik
	ecs::PointSystem::Change(e, POINT_GOLD, iAmount, true);

	//if (iAmount > 1000)
	//{
	//	LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(e, iAmount, "GET_GOLD", ""));
	//}
}

// The automatic potion tick: top the character up while an auto-recovery
// affect is running, and stop when the bottle runs dry.
void AutoRecoveryItemProcess(entt::entity e, int type)
{
	if (true == CombatSystem::IsDead(e) || true == CombatSystem::IsStun(e))
		return;

	if (false == ecs::PlayerRuntime::IsPC(e))
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
		&& (ecs::PlayerRuntime::GetDuelOption(e, "BlockPotion")))
		return;
#endif

	if ((type != AFFECT_AUTO_HP_RECOVERY) && (type != AFFECT_AUTO_SP_RECOVERY)
#ifdef ENABLE_NEW_USE_POTION
		&& (type != AFFECT_AUTO_HP_RECOVERY2) && (type != AFFECT_AUTO_SP_RECOVERY2)
#endif
		)
		return;

	if (nullptr != AffectSystem::FindAffect(e, AFFECT_STUN))
		return;

	{
		const uint32_t stunSkills[] = { SKILL_TANHWAN, SKILL_GEOMPUNG, SKILL_BYEURAK, SKILL_GIGUNG };

		for (size_t i = 0; i < sizeof(stunSkills) / sizeof(uint32_t); ++i)
		{
			const CAffect* p = AffectSystem::FindAffect(e, stunSkills[i]);

			if (nullptr != p && AFF_STUN == p->dwFlag)
				return;
		}
	}

	const CAffect* pAffect = AffectSystem::FindAffect(e, type);
	const size_t idx_of_amount_of_used = 1;
	const size_t idx_of_amount_of_full = 2;

	if (nullptr != pAffect)
	{
		const entt::entity pItem = FindItemByID(e, pAffect->dwFlag);

		if (pItem != entt::null && true == GetItemSocket(pItem, 0))
		{
			if (!CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(e))
#ifdef ENABLE_NEWSTUFF
				&& !(g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(ecs::PlayerRuntime::GetPlayerID(e)) && !IsAllowedPotionOnPVP(GetItemVnum(pItem)))
#endif
				)
			{
				const int32_t amount_of_used = GetItemSocket(pItem, idx_of_amount_of_used);
				const int32_t amount_of_full = GetItemSocket(pItem, idx_of_amount_of_full);

				const int32_t avail = amount_of_full - amount_of_used;

				int32_t amount = 0;
#ifdef ENABLE_NEW_USE_POTION
				if ((type == AFFECT_AUTO_HP_RECOVERY) || (type == AFFECT_AUTO_HP_RECOVERY2))
#else
				if (AFFECT_AUTO_HP_RECOVERY == type)
#endif
				{
					amount = ecs::PointSystem::GetMaxHP(e) - (ecs::PlayerRuntime::GetHP(e) + ecs::PointSystem::Get(e, POINT_HP_RECOVERY));
				}
#ifdef ENABLE_NEW_USE_POTION
				else if ((type == AFFECT_AUTO_SP_RECOVERY) || (type == AFFECT_AUTO_SP_RECOVERY2))
#else
				else if (AFFECT_AUTO_SP_RECOVERY == type)
#endif
				{
					amount = ecs::PointSystem::GetMaxSP(e) - (ecs::PlayerRuntime::GetSP(e) + ecs::PointSystem::Get(e, POINT_SP_RECOVERY));
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
						if (GetItemVnum(pItem) != ITEM_AUTO_HP_RECOVERY_X && GetItemVnum(pItem) != ITEM_AUTO_SP_RECOVERY_X)
							SetItemSocket(pItem, idx_of_amount_of_used, amount_of_used + amount);
#else
						SetItemSocket(pItem, idx_of_amount_of_used, amount_of_used + amount, bLog);
#endif
					}
					else if (GetItemVnum(pItem) != ITEM_AUTO_HP_RECOVERY_X && GetItemVnum(pItem) != ITEM_AUTO_SP_RECOVERY_X)
					{
						amount = avail;

						DestroyItemEntityEcs(pItem, "AUTO_RECOVERY_USED_UP");
					}

#ifdef ENABLE_NEW_USE_POTION
					if ((type == AFFECT_AUTO_HP_RECOVERY) || (type == AFFECT_AUTO_HP_RECOVERY2))
#else
					if (AFFECT_AUTO_HP_RECOVERY == type)
#endif
					{
						ecs::PointSystem::Change(e, POINT_HP_RECOVERY, amount);
						NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_AUTO_HPUP);
					}
#ifdef ENABLE_NEW_USE_POTION
					else if ((type == AFFECT_AUTO_SP_RECOVERY) || (type == AFFECT_AUTO_SP_RECOVERY2))
#else
					else if (AFFECT_AUTO_SP_RECOVERY == type)
#endif
					{
						ecs::PointSystem::Change(e, POINT_SP_RECOVERY, amount);
						NetworkSyncSystem::BroadcastEffect(g_registry, e, SE_AUTO_SPUP);
					}
				}
			}
			else
			{
				LockItem(pItem, false);
				SetItemSocketEcs(pItem, 0, false);
				AffectSystem::RemoveAffect(e, const_cast<CAffect*>(pAffect));
			}
		}
		else
		{
			AffectSystem::RemoveAffect(e, const_cast<CAffect*>(pAffect));
		}
	}
}

} // namespace ItemSystem

namespace ecs::SocialSystem {

// Telling the client to open the personal shop window, if what this
// character is wearing allows it.
void OpenPrivateShop(entt::entity e, bool bKasmir)
{
#ifdef ENABLE_OPEN_SHOP_WITH_ARMOR
#ifdef KASMIR_PAKET_SYSTEM
	if (bKasmir) {
		ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "OpenPrivateShopKasmir");
		return;
	}
#endif
	ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "OpenPrivateShop");
#else
	unsigned bodyPart = ecs::PlayerRuntime::GetPart(e, PART_MAIN);
	switch (bodyPart)
	{
	case 0:
	case 1:
	case 2: {
#ifdef KASMIR_PAKET_SYSTEM
		if (bKasmir) {
			ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "OpenPrivateShopKasmir");
			break;
		}
#endif

		ecs::ChatSystem::Send(e, CHAT_TYPE_COMMAND, "OpenPrivateShop");
	}
		  break;
	default:
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 503, "");
#endif
		break;
	}
#endif
}

// Opening the personal shop, fetching the saved price list first if it
// has not been fetched this session.
void UseSilkBotary(entt::entity e)
{
	if (GetNoOpenedShop(e)) {
		uint32_t dwPlayerID = ecs::PlayerRuntime::GetPlayerID(e);
		db_clientdesc->DBPacket(HEADER_GD_MYSHOP_PRICELIST_REQ, ecs::PlayerRuntime::GetDesc(e)->GetHandle(), &dwPlayerID, sizeof(uint32_t));
		SetNoOpenedShop(e, false);
	}
	else {
#ifdef KASMIR_PAKET_SYSTEM
		OpenPrivateShop(e, GetKasmirPaket(e));
#else
		OpenPrivateShop(e, false);
#endif
	}
}

} // namespace ecs::SocialSystem
