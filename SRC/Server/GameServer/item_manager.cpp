#include "stdafx.h"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/systems/InventorySystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/components/item_components.hpp"
#include "ecs/AIHelpers.hpp"
#include "utils.h"
#include "config.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "desc_client.h"
#include "db.h"
#include "log.h"
#include "skill.h"
#include "text_file_loader.h"
#include "priv_manager.h"
#include "questmanager.h"
#include "unique_item.h"
#include "safebox.h"
#include "blend_item.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include "locale_service.h"
#include "item.h"
#include "item_manager.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/ItemRegistry.hpp"

#include <common/VnumHelper.h>
#include "DragonSoul.h"
#ifndef ENABLE_CUBE_RENEWAL_WORLDARD
#include "cube.h"
#else
#include "cuberenewal.h"
#endif
#ifdef ENABLE_STOLE_COSTUME
#include <common/stole_length.h>
#endif
#ifdef __INGAME_WIKI__
#include "refine.h"
#endif

namespace
{
LPITEM ResolveManagedItem(entt::entity item)
{
	if (item == entt::null || !g_registry.valid(item))
		return nullptr;

	const auto* legacy = g_registry.try_get<ecs::LegacyItemPtr>(item);
	return legacy ? legacy->ptr : nullptr;
}

template<class Function>
struct ScopeExit
{
    Function function;
    ~ScopeExit() noexcept
    {
        try { function(); }
        catch (...)
        {
            // Preserve an initializer's original exception. The destruction
            // path retains the live item's indexes when retirement fails.
            try { LOG_ERROR("Item creation rollback threw; retained item requires cleanup"); }
            catch (...) {}
        }
    }
};

struct ItemDestructionGuard
{
	std::unordered_set<entt::entity>& active;
	entt::entity item;
	~ItemDestructionGuard() { active.erase(item); }
};
}
ITEM_MANAGER::ITEM_MANAGER()
	: m_iTopOfTable(0), m_dwVIDCount(0), m_dwCurrentID(0)
{
	m_ItemIDRange.dwMin = m_ItemIDRange.dwMax = m_ItemIDRange.dwUsableItemIDMin = 0;
	m_ItemIDSpareRange.dwMin = m_ItemIDSpareRange.dwMax = m_ItemIDSpareRange.dwUsableItemIDMin = 0;
}

ITEM_MANAGER::~ITEM_MANAGER()
{
	Destroy();
}

void ITEM_MANAGER::Destroy()
{
	for (const auto& [vid, itemEntity] : m_VIDMap)
	{
		LPITEM item = ResolveManagedItem(itemEntity);
		if (!item)
			continue;

		EntityFactory::DestroyItemEntity(g_registry, itemEntity);
#ifdef M2_USE_POOL
		pool_.Destroy(item);
#else
		M2_DELETE(item);
#endif
	}

	m_VIDMap.clear();
	m_map_pkItemByID.clear();
	m_set_pkItemForDelayedSave.clear();
}

void ITEM_MANAGER::GracefulShutdown()
{
    ItemSystem::ProcessPendingItemConsumptions();
	for (const entt::entity itemEntity : m_set_pkItemForDelayedSave)
		SaveSingleItem(itemEntity);
	m_set_pkItemForDelayedSave.clear();
}

bool ITEM_MANAGER::Initialize(TItemTable* table, int size)
{
	if (!m_vec_prototype.empty())
		m_vec_prototype.clear();

	int	i;

	m_vec_prototype.resize(size);
	memcpy(m_vec_prototype.data(), table, sizeof(TItemTable) * size);
	for (int i = 0; i < size; i++)
	{
		if (0 != m_vec_prototype[i].dwVnumRange)
		{
			m_vec_item_vnum_range_info.push_back(&m_vec_prototype[i]);
		}
	}

	m_map_ItemRefineFrom.clear();
	for (i = 0; i < size; ++i)
	{

		if (m_vec_prototype[i].dwRefinedVnum)
			m_map_ItemRefineFrom.insert(std::make_pair(m_vec_prototype[i].dwRefinedVnum, m_vec_prototype[i].dwVnum));

		// NOTE : QUEST_GIVE ÇÃ·¡±×´Â npc ÀÌº¥Æ®·Î ¹ß»ý.
		if (m_vec_prototype[i].bType == ITEM_QUEST || IS_SET(m_vec_prototype[i].dwFlags, ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE)
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
			|| (m_vec_prototype[i].bType == ITEM_COSTUME && m_vec_prototype[i].bSubType == COSTUME_MOUNT)
#endif
			)
			quest::CQuestManager::instance().RegisterNPCVnum(m_vec_prototype[i].dwVnum);

		m_map_vid.insert(std::map<uint32_t, TItemTable>::value_type(m_vec_prototype[i].dwVnum, m_vec_prototype[i]));
		if (test_server)
			LOG_INFO("ITEM_INFO {} {} ", m_vec_prototype[i].dwVnum, m_vec_prototype[i].szName);
	}

	int len = 0, len2;
	char buf[512];

	for (i = 0; i < size; ++i)
	{
#ifdef ENABLE_MULTI_NAMES
		len2 = snprintf(buf + len, sizeof(buf) - len, "%5u %-16s", m_vec_prototype[i].dwVnum, m_vec_prototype[i].szLocaleName[DEFAULT_LANGUAGE]);
#else
		len2 = snprintf(buf + len, sizeof(buf) - len, "%5u %-16s", m_vec_prototype[i].dwVnum, m_vec_prototype[i].szLocaleName);
#endif

		if (len2 < 0 || len2 >= (int)sizeof(buf) - len)
			len += (sizeof(buf) - len) - 1;
		else
			len += len2;

		if (!((i + 1) % 4))
		{
			if (!test_server)
				LOG_INFO("{}", buf);
			len = 0;
		}
		else
		{
			buf[len++] = '\t';
			buf[len] = '\0';
		}
	}

	if ((i + 1) % 4)
	{
		if (!test_server)
			LOG_INFO("{}", buf);
	}

	auto it = m_VIDMap.begin();

	LOG_INFO("ITEM_VID_MAP {}", m_VIDMap.size());

	while (it != m_VIDMap.end())
	{
		const entt::entity itemEntity = it->second;
		++it;
		LPITEM item = ResolveManagedItem(itemEntity);
		if (!item)
			continue;

		const TItemTable* tableInfo = GetTable(item->GetOriginalVnum());

		if (nullptr == tableInfo)
		{
			LOG_ERROR("cannot reset item table");
			item->SetProto(nullptr);
		}

		item->SetProto(tableInfo);
	}

	return true;
}


#ifdef ENABLE_ITEM_EXTRA_PROTO
bool ITEM_MANAGER::InitializeExtraProto(TItemExtraProto* table, uint32_t count)
{
	if (m_map_ExtraProto.empty() == false)
		LOG_INFO("RELOADING ITEM EXTRA PROTO.");

	m_map_ExtraProto.clear();
	auto& map = m_map_ExtraProto;

	//todebug
	//"FINDME : count of extra protos %u ", count);

	for (uint32_t i = 0; i < count; ++i, ++table) {
		map[table->dwVnum] = *table;

		//todebug
		//"FINDME : loading table vnum(%u) rarity (%d) ", table->dwVnum, table->iRarity);
	}

	auto it = m_VIDMap.begin();
	while (it != m_VIDMap.end())
	{
		const entt::entity itemEntity = it->second;
		++it;
		LPITEM item = ResolveManagedItem(itemEntity);
		if (!item)
			continue;

		auto extra_it = map.find(item->GetOriginalVnum());
		if (extra_it != map.end()) {
			ItemSystem::SetItemExtraProto(item->GetEntityHandle(), &extra_it->second);
			continue;
		}

		ItemSystem::SetItemExtraProto(item->GetEntityHandle(), nullptr);
	}

	return true;
}

TItemExtraProto* ITEM_MANAGER::GetExtraProto(uint32_t vnum)
{
	auto it = this->m_map_ExtraProto.find(vnum);
	if (it != m_map_ExtraProto.end())
		return &it->second;
	return nullptr;
}
#endif



// Skill-book selection is shared by item creation and quest rewards. Preserve
// the old conditional distribution, but enumerate candidates instead of retrying
// forever when a job or the entire skill table has no usable entries.
const uint32_t GetRandomSkillVnum(uint8_t job)
{
    static constexpr uint32_t skills[JOB_MAX_NUM][SKILL_GROUP_MAX_NUM][6] = {
        {{1, 2, 3, 4, 5, 6}, {16, 17, 18, 19, 20, 21}},
        {{31, 32, 33, 34, 35, 36}, {46, 47, 48, 49, 50, 51}},
        {{61, 62, 63, 64, 65, 66}, {76, 77, 78, 79, 80, 81}},
        {{91, 92, 93, 94, 95, 96}, {106, 107, 108, 109, 110, 111}},
    };
    std::array<uint32_t, JOB_MAX_NUM * SKILL_GROUP_MAX_NUM * 6> available {};
    size_t size = 0;
    const int firstJob = job == JOB_MAX_NUM ? 0 : std::min<int>(job, JOB_MAX_NUM - 1);
    const int endJob = job == JOB_MAX_NUM ? JOB_MAX_NUM : firstJob + 1;
    for (int current = firstJob; current < endJob; ++current)
        for (const auto& group : skills[current])
            for (const uint32_t skill : group)
                if (skill && CSkillManager::instance().Get(skill)) available[size++] = skill;
    return size ? available[number(0, static_cast<int>(size) - 1)] : 0;
}

entt::entity ITEM_MANAGER::CreateItem(uint32_t vnum, uint32_t count, uint32_t id, bool bTryMagic, int iRarePct, bool bSkipSave)
{
    const TItemTable* table = vnum ? GetTable(vnum) : nullptr;
    if (!table || table->bSize == 0) return entt::null;
    const TItemTable proto = *table; // Never retain a borrowed table across initialization callbacks.
    const bool isNew = id == 0;
    const bool gold = proto.bType == ITEM_ELK;
    if ((!gold && g_bItemCountLimit <= 0) || (gold && count == 0)) return entt::null;
    if (gold) count = std::min(count, uint32_t(INT_MAX));
    else if (proto.dwFlags & ITEM_FLAG_STACKABLE)
    {
        count = std::clamp(count, uint32_t(1), uint32_t(g_bItemCountLimit));
        if (bTryMagic && count == 1 && (proto.dwFlags & ITEM_FLAG_MAKECOUNT))
        {
            if (proto.alValues[1] <= 0) return entt::null;
            count = std::min(uint32_t(proto.alValues[1]), uint32_t(g_bItemCountLimit));
        }
    }
    else count = 1;

    const uint32_t itemID = gold ? 0 : (isNew ? GetNewID() : id);
    if (!gold && !itemID) return entt::null;
    const auto occupied = [&](const auto& index, uint32_t key) {
        const auto it = index.find(key);
        return it != index.end() && ItemSystem::IsValidItem(it->second);
    };
    if (itemID && occupied(m_map_pkItemByID, itemID))
    {
        LOG_ERROR("ITEM_ID_DUP: id={} vnum={}", itemID, vnum);
        return entt::null;
    }
    if (m_dwVIDCount == UINT32_MAX) return entt::null; // Never wrap over a live VID.
    const uint32_t itemVID = ++m_dwVIDCount;
    if (occupied(m_VIDMap, itemVID)) return entt::null;
    const uint32_t mask = GetMaskVnum(vnum);
    const uint32_t displayVnum = mask ? mask : vnum;

    // Temporary allocation boundary: unmigrated consumers still require CItem.
    // No CItem pointer is used by the initialization pipeline below this block.
    const entt::entity item = [&] {
#ifdef M2_USE_POOL
        LPITEM allocation = pool_.Construct();
#else
        LPITEM allocation = M2_NEW CItem(vnum);
#endif
        allocation->Initialize();
        allocation->SetProto(table);
        allocation->SetMaskVnum(mask);
        allocation->SetID(itemID);
        allocation->SetVID(itemVID);
        const auto entity = EntityFactory::CreateItemEntity(g_registry, allocation);
        if (!ItemSystem::IsValidItem(entity))
        {
#ifdef M2_USE_POOL
            pool_.Destroy(allocation);
#else
            M2_DELETE(allocation);
#endif
        }
        return entity;
    }();
    if (!ItemSystem::IsValidItem(item)) return entt::null;
    bool committed = false;
    const auto* initialFlags = g_registry.try_get<ecs::ItemFlags>(item);
    const bool originalSkipSave = initialFlags && initialFlags->skipSave;
    const auto cleanup = [&] {
        if (committed) return;
        if (!ItemSystem::IsValidItem(item)) { ForgetItem(item, itemID, itemVID); return; }
        const auto* owner = g_registry.try_get<ecs::ItemOwner>(item);
        const auto* location = g_registry.try_get<ecs::ItemLocation>(item);
        // An external callback may have taken ownership. Never destroy its item.
        if (!owner || owner->owner != entt::null || !location || location->window != RESERVED_WINDOW ||
            g_registry.any_of<ecs::SpatialEntity, ecs::SectorPlacement>(item))
        {
            if (auto* flags = g_registry.try_get<ecs::ItemFlags>(item)) flags->skipSave = originalSkipSave;
            LOG_ERROR("ITEM_CREATE aborted: item {} moved during initialization", itemID);
            return;
        }
        if (auto* flags = g_registry.try_get<ecs::ItemFlags>(item)) flags->skipSave = true;
        const ScopeExit restoreTransferredSavePolicy {[&] {
            if (!ItemSystem::IsValidItem(item)) return;
            const auto* currentOwner = g_registry.try_get<ecs::ItemOwner>(item);
            const auto* currentLocation = g_registry.try_get<ecs::ItemLocation>(item);
            if ((currentOwner && currentOwner->owner != entt::null) ||
                (currentLocation && currentLocation->window != RESERVED_WINDOW) ||
                g_registry.any_of<ecs::SpatialEntity, ecs::SectorPlacement>(item))
                if (auto* flags = g_registry.try_get<ecs::ItemFlags>(item)) flags->skipSave = originalSkipSave;
        }};
#ifdef DEBUG_ALLOC
        DestroyItem(item, __FILE__, __LINE__);
#else
        DestroyItem(item);
#endif
    };
    const ScopeExit creationGuard {cleanup};
    const auto live = [&] {
        if (!ItemSystem::IsValidItem(item) ||
            !g_registry.all_of<ecs::ItemIdentity, ecs::ItemCount, ecs::ItemOwner, ecs::ItemLocation,
                ecs::ItemFlags, ecs::ItemEquipped, ecs::ItemSockets, ecs::ItemAttributes>(item) ||
            g_registry.any_of<ecs::SpatialEntity, ecs::SectorPlacement>(item)) return false;
        const auto& identity = g_registry.get<ecs::ItemIdentity>(item);
        return identity.id == itemID && identity.vid == itemVID && identity.originalVnum == vnum &&
            identity.vnum == displayVnum && g_registry.get<ecs::ItemOwner>(item).owner == entt::null &&
            g_registry.get<ecs::ItemLocation>(item).window == RESERVED_WINDOW &&
            !g_registry.get<ecs::ItemEquipped>(item).equipped &&
            !g_registry.get<ecs::ItemFlags>(item).exchanging && !g_registry.get<ecs::ItemFlags>(item).isLocked &&
            !ItemSystem::IsItemConsumptionPending(item);
    };
    if (!live()) return entt::null;
    g_registry.get<ecs::ItemFlags>(item).skipSave = true;
    if (!bSkipSave)
    {
        // Purge stale entries only. Nested creation must never lose its indexes.
        if (auto it = m_VIDMap.find(itemVID); it != m_VIDMap.end() && !ItemSystem::IsValidItem(it->second)) m_VIDMap.erase(it);
        if (itemID)
            if (auto it = m_map_pkItemByID.find(itemID); it != m_map_pkItemByID.end() && !ItemSystem::IsValidItem(it->second)) m_map_pkItemByID.erase(it);
        if (!m_VIDMap.try_emplace(itemVID, item).second ||
            (itemID && !m_map_pkItemByID.try_emplace(itemID, item).second)) return entt::null;
    }
#ifdef ENABLE_ITEM_EXTRA_PROTO
    ItemSystem::SetItemExtraProto(item, GetExtraProto(vnum));
    if (!live()) return entt::null;
#endif
    if (!ItemSystem::SetItemCountEcs(item, count) || !live() || ItemSystem::GetItemCount(item) != count) return entt::null;

    // Creation writes existing socket components without interim save/packets.
    // The completed item is saved once, after every initializer has succeeded.
    const auto socket = [&](int index, int64_t value) {
        g_registry.get<ecs::ItemSockets>(item).sockets[index] =
            static_cast<int32_t>(std::clamp<int64_t>(value, INT32_MIN, INT32_MAX));
    };
    if (isNew && proto.bType == ITEM_UNIQUE)
        socket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, proto.alValues[2] == 0 ? int64_t(proto.alValues[0]) :
            int64_t(get_global_time()) + proto.alValues[0]);

    switch (displayVnum)
    {
    case ITEM_AUTO_HP_RECOVERY_S: case ITEM_AUTO_HP_RECOVERY_M:
    case ITEM_AUTO_HP_RECOVERY_L: case ITEM_AUTO_HP_RECOVERY_X:
    case ITEM_AUTO_SP_RECOVERY_S: case ITEM_AUTO_SP_RECOVERY_M:
    case ITEM_AUTO_SP_RECOVERY_L: case ITEM_AUTO_SP_RECOVERY_X:
    case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS: case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
    case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS: case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
        socket(2, proto.alValues[0]); break;
    }
    bool realTime = false, soulTimer = false;
    for (const auto& limit : proto.aLimits)
    {
        if (limit.bType == LIMIT_REAL_TIME)
        {
            socket(0, int64_t(time(nullptr)) + (limit.lValue ? int64_t(limit.lValue) : 60 * 60 * 24 * 7));
            realTime = true;
        }
        else if (limit.bType == LIMIT_TIMER_BASED_ON_WEAR && isNew)
        {
            const auto duration = g_registry.get<ecs::ItemSockets>(item).sockets[0];
            socket(0, duration ? duration : limit.lValue ? limit.lValue : 60 * 60 * 10);
        }
    }
#ifdef ENABLE_DS_EDITS
    if (displayVnum == 100000 || displayVnum == 100001 || displayVnum == 100002)
        socket(ITEM_SOCKET_CHARGING_AMOUNT_IDX, proto.alValues[0]);
#endif
#ifdef ENABLE_SOUL_SYSTEM
    if (proto.bType == ITEM_SOUL) { socket(2, proto.alValues[2]); soulTimer = true; }
#endif
    const bool blend = isNew && proto.bType == ITEM_BLEND && Blend_Item_find(displayVnum);
    if (blend)
    {
        if (!Blend_Item_set_value(item) || !live()) return entt::null;
    }
    else
    {
        if (isNew)
        {
            if (proto.sAddonType)
            {
                ItemSystem::ApplyItemAddon(item, proto.sAddonType);
                if (!live()) return entt::null;
            }
            if (bTryMagic)
            {
                const int rare = iRarePct == -1 ? proto.bAlterToMagicItemPct : iRarePct;
                if (number(1, 100) <= rare)
                {
                    ItemSystem::AlterItemToMagicItem(item);
                    if (!live()) return entt::null;
                }
            }
            for (int index = 0; index < std::min<int>(proto.bGainSocketPct, ITEM_SOCKET_MAX_NUM); ++index) socket(index, 1);
            if (vnum == 50300 || vnum == ITEM_SKILLFORGET_VNUM)
            {
                const uint32_t skill = GetRandomSkillVnum();
                if (!skill || !live()) return entt::null;
                socket(0, skill);
            }
            else if (vnum == ITEM_SKILLFORGET2_VNUM)
            {
                std::array<uint32_t, 8> available {};
                size_t size = 0;
                for (uint32_t skill = 112; skill <= 119; ++skill)
                    if (CSkillManager::instance().Get(skill)) available[size++] = skill;
                if (!size || !live()) return entt::null;
                socket(0, available[number(0, static_cast<int>(size) - 1)]);
            }
        }
        else if (proto.bAlterToMagicItemPct == 100 && ItemSystem::GetItemAttributeCount(item) == 0)
        {
            ItemSystem::AlterItemToMagicItem(item);
            if (!live()) return entt::null;
        }

        uint32_t sig = 0;
        if (proto.bType == ITEM_QUEST)
        {
            for (const auto& [groupID, group] : m_map_pkQuestItemGroup)
                if (group && group->m_bType == CSpecialItemGroup::QUEST && group->Contains(vnum)) sig = groupID;
        }
        else if (proto.bType == ITEM_UNIQUE || proto.bSubType == COSTUME_MOUNT)
        {
            for (const auto& [groupID, group] : m_map_pkSpecialItemGroup)
                if (group && group->m_bType == CSpecialItemGroup::SPECIAL && group->Contains(vnum)) sig = groupID;
        }
#ifdef ENABLE_ATTR_COSTUMES
        else if (proto.bType == ITEM_USE && (proto.bSubType == USE_ADD_ATTR_COSTUME1 || proto.bSubType == USE_ADD_ATTR_COSTUME2))
        {
            constexpr int bonuses[] = {APPLY_ATTBONUS_MONSTER, APPLY_ATTBONUS_BOSS, APPLY_ATTBONUS_METIN,
                APPLY_ATTBONUS_HUMAN, APPLY_RESIST_MEZZIUOMINI};
            socket(0, bonuses[number(0, 4)]);
            socket(1, proto.bSubType == USE_ADD_ATTR_COSTUME1 ? 5 : 10);
        }
#endif
        g_registry.get<ecs::ItemIdentity>(item).sigVnum = sig;
        if (isNew && ItemSystem::IsDragonSoulItem(item))
        {
            if (!DSManager::instance().DragonSoulItemInitialize(item) || !live()) return entt::null;
        }
#ifdef ENABLE_RUNE_SYSTEM
        if (isNew && (!ItemSystem::InitializeRuneItem(item) || !live())) return entt::null;
#endif
#ifdef ENABLE_NEW_USE_POTION
        if (isNew && proto.bType == ITEM_USE && proto.bSubType == USE_NEW_POTIION)
        { socket(0, proto.aLimits[0].lValue); socket(1, 0); }
#endif
#ifdef ENABLE_STOLE_COSTUME
        if (isNew && proto.bType == ITEM_COSTUME && proto.bSubType == COSTUME_STOLE)
        {
            const int grade = std::clamp<int>(proto.alValues[0], 0, 4);
            if (grade)
                for (int index = 0; index < MAX_ATTR; ++index)
                {
                    if (!ItemSystem::SetItemForceAttributeEcs(item, index, stoleInfoTable[index][0],
                        stoleInfoTable[index][number(grade * 4 - 3, grade * 4)]) || !live()) return entt::null;
                }
        }
#endif
#ifdef ENABLE_DS_POTION_DIFFRENT
        if (isNew && proto.bType == ITEM_USE && proto.bSubType == USE_TIME_CHARGE_PER) socket(0, proto.alValues[0]);
#endif
    }

    // Start events only after all socket/attribute state is initialized.
    if (proto.bType == ITEM_UNIQUE && proto.alValues[2] != 0)
    {
        ItemSystem::StartUniqueExpireEvent(item);
        if (!live()) return entt::null;
    }
    if (realTime && (!ItemSystem::StartRealTimeExpireEventEcs(item) || !live())) return entt::null;
#ifdef ENABLE_SOUL_SYSTEM
    // A fully charged soul intentionally has no growth timer. That is not an
    // initialization failure (the event service also returns false for it).
    if (soulTimer && static_cast<int>(uint32_t(g_registry.get<ecs::ItemSockets>(item).sockets[2]) / 10000) < proto.aLimits[1].lValue &&
        (!ItemSystem::StartSoulItemEventEcs(item) || !live())) return entt::null;
#endif
    if (!live() || ItemSystem::GetItemCount(item) != count) return entt::null;
    g_registry.get<ecs::ItemFlags>(item).skipSave = false;
    committed = true;
    ItemSystem::SaveItem(item);
    return live() && ItemSystem::GetItemCount(item) == count ? item : entt::entity(entt::null);
}


void ITEM_MANAGER::DelayedSave(entt::entity itemEntity)
{
	if (!ItemSystem::IsValidItem(itemEntity) || ItemSystem::GetItemID(itemEntity) == 0)
		return;

	m_set_pkItemForDelayedSave.insert(itemEntity);
}
void ITEM_MANAGER::FlushDelayedSave(entt::entity itemEntity)
{
	const auto it = m_set_pkItemForDelayedSave.find(itemEntity);
	if (it == m_set_pkItemForDelayedSave.end())
		return;

	m_set_pkItemForDelayedSave.erase(it);
	SaveSingleItem(itemEntity);
}

void ITEM_MANAGER::FlushDelayedSaveByOwner(entt::entity owner)
{
	if (!ecs::PlayerRuntime::IsValid(owner))
		return;

	auto it = m_set_pkItemForDelayedSave.begin();
	while (it != m_set_pkItemForDelayedSave.end())
	{
		const entt::entity item = *it;

		if (!ItemSystem::IsValidItem(item))
		{
			it = m_set_pkItemForDelayedSave.erase(it);
			continue;
		}

		if (ItemSystem::GetItemOwner(item) == owner)
		{
			it = m_set_pkItemForDelayedSave.erase(it);
			SaveSingleItem(item);
			continue;
		}
		++it;
	}
}

void ITEM_MANAGER::SaveSingleItem(entt::entity item)
{
	if (!ItemSystem::IsValidItem(item))
		return;

	if (ItemSystem::GetItemOwner(item) == entt::null ||
        (ItemSystem::IsItemConsumptionPending(item) && ItemSystem::GetItemCount(item) == 0))
	{
		uint32_t dwID = ItemSystem::GetItemID(item);
		uint32_t dwOwnerID = ItemSystem::GetItemLastOwnerPID(item);

		db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_DESTROY, 0, sizeof(uint32_t) + sizeof(uint32_t));
		db_clientdesc->Packet(&dwID, sizeof(uint32_t));
		db_clientdesc->Packet(&dwOwnerID, sizeof(uint32_t));

		LOG_INFO("ITEM_DELETE {}:{}", ItemSystem::GetItemName(item), dwID);
		return;
	}

	LOG_INFO("ITEM_SAVE {} in window {}", ItemSystem::GetItemID(item), ItemSystem::GetItemWindow(item));

	TPlayerItem t{};

	t.id = ItemSystem::GetItemID(item);
	t.window = ItemSystem::GetItemWindow(item);
#ifdef ATTR_LOCK
	t.lockedattr = ItemSystem::GetItemLockedAttr(item);
#endif
	switch (t.window)
	{
	case EQUIPMENT:
		t.pos = ItemSystem::GetItemCell(item) - INVENTORY_MAX_NUM;
		break;
#ifdef ENABLE_BELT_INVENTORY_EX
	case INVENTORY:
		if (BELT_INVENTORY_SLOT_START <= ItemSystem::GetItemCell(item) && BELT_INVENTORY_SLOT_END > ItemSystem::GetItemCell(item))
		{
			t.window = BELT_INVENTORY;
			t.pos = ItemSystem::GetItemCell(item) - BELT_INVENTORY_SLOT_START;
			break;
		}
#endif
	default:
		t.pos = ItemSystem::GetItemCell(item);
		break;
	}
	t.count = ItemSystem::GetItemCount(item);
	t.vnum = ItemSystem::GetItemOriginalVnum(item);
	const entt::entity owner = ItemSystem::GetItemOwner(item);
	if (t.window == SAFEBOX || t.window == MALL)
	{
		LPDESC desc = ecs::PlayerRuntime::GetDesc(owner);
		if (!desc) {
			LOG_ERROR("ITEM_SAVE failed: owner has no descriptor (item_id={})", t.id);
			return;
		}
		t.owner = desc->GetAccountTable().id;
	}
	else
		t.owner = ecs::PlayerRuntime::GetPlayerID(owner);
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		t.alSockets[i] = ItemSystem::GetItemSocket(item, i);
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		t.aAttr[i] = ItemSystem::GetItemAttribute(item, i);

	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_SAVE, 0, sizeof(TPlayerItem));
	db_clientdesc->Packet(&t, sizeof(TPlayerItem));
}

void ITEM_MANAGER::Update()
{
    ItemSystem::ProcessPendingItemConsumptions();
	auto it = m_set_pkItemForDelayedSave.begin();
	while (it != m_set_pkItemForDelayedSave.end())
	{
		const entt::entity item = *it;

		if (!ItemSystem::IsValidItem(item))
		{
			it = m_set_pkItemForDelayedSave.erase(it);
			continue;
		}

		if (ItemSystem::GetItemOwner(item) != entt::null && IS_SET(ItemSystem::GetItemFlags(item), ITEM_FLAG_SLOW_QUERY))
		{
			++it;
			continue;
		}

		SaveSingleItem(item);
		it = m_set_pkItemForDelayedSave.erase(it);
	}
}

void ITEM_MANAGER::RemoveItem(entt::entity itemEntity, const char* reason)
{
	if (!ItemSystem::IsValidItem(itemEntity) || m_itemsBeingDestroyed.contains(itemEntity))
		return;

	const uint32_t id = ItemSystem::GetItemID(itemEntity);
	const uint32_t vid = ItemSystem::GetItemVID(itemEntity);
	if (id && !ItemSystem::GetItemSkipSave(itemEntity) && !db_clientdesc)
	{
		LOG_ERROR("ITEM_REMOVE deferred: no DB descriptor for item {}", id);
		return;
	}

	m_itemsBeingDestroyed.insert(itemEntity);
	ItemDestructionGuard guard {m_itemsBeingDestroyed, itemEntity};
	const auto live = [&] {
		if (ItemSystem::IsValidItem(itemEntity)) return true;
		ForgetItem(itemEntity, id, vid);
		return false;
	};
	const auto detached = [&] {
		if (!live()) return false;
		const auto* ownership = g_registry.try_get<ecs::ItemOwner>(itemEntity);
		return ownership && ownership->owner == entt::null &&
			ItemSystem::GetItemWindow(itemEntity) == RESERVED_WINDOW;
	};
	const auto owner = ItemSystem::GetItemOwner(itemEntity);
	if (owner != entt::null)
	{
		const auto window = ItemSystem::GetItemWindow(itemEntity);
		const auto cell = ItemSystem::GetItemCell(itemEntity);
		const auto stillOwned = [&] {
			return live() && g_registry.valid(owner) && ItemSystem::GetItemOwner(itemEntity) == owner &&
				ItemSystem::GetItemWindow(itemEntity) == window && ItemSystem::GetItemCell(itemEntity) == cell;
		};
		const TItemPos position(window == EQUIPMENT ? INVENTORY : window, cell);
		const auto recordedSlot = [&]() -> entt::entity {
			if (window == SAFEBOX || window == MALL)
			{
				const auto storage = SafeboxSystem::Get(owner, window);
				return storage ? storage->Get(cell) : entt::null;
			}
			if (window == MOUNT_INVENTORY) return MountSystem::GetMountInventoryItem(owner, cell);
			return ItemSystem::GetItem(owner, position);
		};
		// Never clear a foreign occupant (or its quickslot) from a stale location.
		if (recordedSlot() != itemEntity)
		{
			LOG_ERROR("ITEM_REMOVE deferred: item {} is not in its recorded owner slot", id);
			return;
		}

		char hint[64];
		snprintf(hint, sizeof(hint), "%s %u ", ItemSystem::GetItemName(itemEntity), ItemSystem::GetItemCount(itemEntity));
		LogManager::instance().ItemLogEntity(owner, itemEntity, reason ? reason : "REMOVE", hint);
		if (!stillOwned())
		{
			// Closing a storage during logging can already detach this item.
			if (detached()) DestroyItemNow(itemEntity, __FILE__, __LINE__);
			return;
		}

		if (window == SAFEBOX || window == MALL)
		{
			// Keep the container alive if a removal callback closes its session.
			const auto storage = SafeboxSystem::Get(owner, window);
			if (!storage || storage->Get(cell) != itemEntity) return;
			storage->Remove(cell);
			// Close/replacement callbacks can suppress the result/packet even
			// after a successful detach. Verify the entity's final state below.
		}
		else
		{
			if (recordedSlot() != itemEntity) return;
			if (window == INVENTORY)
				InventorySystem::SyncQuickslot(owner, QUICKSLOT_TYPE_ITEM, cell, 255);
#ifdef ENABLE_EXTRA_INVENTORY
			else if (window == EXTRA_INVENTORY)
				InventorySystem::SyncQuickslot(owner, QUICKSLOT_TYPE_ITEM_EXTRA, cell, 255);
#endif
			if (!stillOwned())
			{
				if (detached()) DestroyItemNow(itemEntity, __FILE__, __LINE__);
				return;
			}
			if (recordedSlot() != itemEntity) return;
			InventorySystem::RemoveFromCharacter(itemEntity);
			if (!live()) return;
		}

		if (!detached()) return;

		if (window == MOUNT_INVENTORY)
		{
			if (g_registry.valid(owner)) MountSystem::SendMountInventory(owner);
			if (g_registry.valid(owner)) ecs::PointSystem::Compute(owner);
			if (g_registry.valid(owner)) NetworkSyncSystem::PointsPacket(owner);
#ifdef ENABLE_FAKE_SHOP_HEADER
			if (g_registry.valid(owner)) MountSystem::UpdateMountCountOverheadToViewers(owner);
#endif
			if (!detached()) return;
		}
	}

	DestroyItemNow(itemEntity, __FILE__, __LINE__);
}

void ITEM_MANAGER::ForgetItem(entt::entity item, uint32_t id, uint32_t vid)
{
	m_set_pkItemForDelayedSave.erase(item);
	if (auto it = m_map_pkItemByID.find(id); it != m_map_pkItemByID.end() && it->second == item)
		m_map_pkItemByID.erase(it);
	if (auto it = m_VIDMap.find(vid); it != m_VIDMap.end() && it->second == item)
		m_VIDMap.erase(it);
}
#ifndef DEBUG_ALLOC
void ITEM_MANAGER::DestroyItem(entt::entity itemEntity)
#else
void ITEM_MANAGER::DestroyItem(entt::entity itemEntity, const char* file, size_t line)
#endif
{
	if (!ItemSystem::IsValidItem(itemEntity) || m_itemsBeingDestroyed.contains(itemEntity))
		return;

	m_itemsBeingDestroyed.insert(itemEntity);
	ItemDestructionGuard guard {m_itemsBeingDestroyed, itemEntity};
#ifdef DEBUG_ALLOC
	DestroyItemNow(itemEntity, file, line);
#else
	DestroyItemNow(itemEntity, __FILE__, __LINE__);
#endif
}

void ITEM_MANAGER::DestroyItemNow(entt::entity itemEntity, const char* file, size_t line)
{
	const uint32_t id = ItemSystem::GetItemID(itemEntity);
	const uint32_t vid = ItemSystem::GetItemVID(itemEntity);
	if (id && !ItemSystem::GetItemSkipSave(itemEntity) && !db_clientdesc)
	{
		LOG_ERROR("ITEM_DESTROY deferred: no DB descriptor for item {}", id);
		return;
	}

	const auto forget = [&] {
		ForgetItem(itemEntity, id, vid);
	};

	// These services take entity identities. Never retain a CItem or owner
	// pointer across ground removal, unequipping, event or packet callbacks.
	const auto* initialOwnership = g_registry.try_get<ecs::ItemOwner>(itemEntity);
	const entt::entity initialOwner = initialOwnership ? initialOwnership->owner : entt::null;
	const auto initialWindow = ItemSystem::GetItemWindow(itemEntity);
	const auto initialCell = ItemSystem::GetItemCell(itemEntity);
	InventorySystem::RemoveFromGround(itemEntity);
	if (!ItemSystem::IsValidItem(itemEntity))
	{
		forget();
		return;
	}
	if (const auto* ownership = g_registry.try_get<ecs::ItemOwner>(itemEntity);
		ownership && ownership->owner != entt::null &&
		(ownership->owner != initialOwner || ItemSystem::GetItemWindow(itemEntity) != initialWindow ||
			ItemSystem::GetItemCell(itemEntity) != initialCell))
	{
		LOG_ERROR("ITEM_DESTROY deferred: item {} moved during ground removal", id);
		return;
	}

	if (const auto owner = ItemSystem::GetItemOwner(itemEntity); owner != entt::null)
	{
		const auto window = ItemSystem::GetItemWindow(itemEntity);
		const auto cell = ItemSystem::GetItemCell(itemEntity);
		// Equipment locations store absolute inventory cells; GetItem's
		// EQUIPMENT input takes a relative wear slot, so query INVENTORY here.
		const TItemPos position(window == EQUIPMENT ? INVENTORY : window, cell);
		if (window == SAFEBOX || window == MALL || window == MOUNT_INVENTORY ||
			ItemSystem::GetItem(owner, position) == itemEntity)
			InventorySystem::RemoveFromCharacter(itemEntity);
		else
		{
			LOG_ERROR("ITEM_DESTROY: item {} is not in its recorded owner slot; preserving that slot", id);
			ItemSystem::SetItemOwnerEntity(itemEntity, entt::null);
		}
	}
	else
		ItemSystem::SetItemOwnerEntity(itemEntity, entt::null);

	if (!ItemSystem::IsValidItem(itemEntity))
	{
		forget();
		return;
	}
	// A callback can move a still-live item. Do not delete its new ownership.
	const auto* remainingOwner = g_registry.try_get<ecs::ItemOwner>(itemEntity);
	if (remainingOwner && remainingOwner->owner != entt::null)
	{
		LOG_ERROR("ITEM_DESTROY deferred: item {} still has an owner after detachment", id);
		return;
	}

	// Validate the allocation boundary before requesting persistent deletion.
	// DBPacket writes/flushes bytes; it does not dispatch gameplay callbacks.
	LPITEM allocation = ResolveManagedItem(itemEntity);
	if (allocation && allocation->GetEntityHandle() != itemEntity)
	{
		LOG_ERROR("ITEM_DESTROY: mismatched legacy allocation for item {}", id);
		return;
	}
	LOG_INFO("ITEM_DESTROY {}:{}", ItemSystem::GetItemName(itemEntity), id);
	if (!ItemSystem::GetItemSkipSave(itemEntity) && id)
	{
		if (!db_clientdesc)
			return;
		const std::array<uint32_t, 2> payload {id, ItemSystem::GetItemLastOwnerPID(itemEntity)};
		db_clientdesc->DBPacket(HEADER_GD_ITEM_DESTROY, 0, payload.data(), sizeof(payload));
	}

	// The only legacy boundary left here is releasing an existing allocation.
	// Entity-only items run the same manager/index/factory cleanup without it.
	const bool wasDelayed = m_set_pkItemForDelayedSave.contains(itemEntity);
	const bool wasByID = id && m_map_pkItemByID.contains(id) && m_map_pkItemByID.at(id) == itemEntity;
	const bool wasByVID = m_VIDMap.contains(vid) && m_VIDMap.at(vid) == itemEntity;
	const auto restore = [&] {
		if (!ItemSystem::IsValidItem(itemEntity))
			return;
		// Never overwrite an identity published by a nested callback.
		if (wasByID) m_map_pkItemByID.try_emplace(id, itemEntity);
		if (wasByVID) m_VIDMap.try_emplace(vid, itemEntity);
		if (wasDelayed) m_set_pkItemForDelayedSave.insert(itemEntity);
	};
	forget();
	try
	{
		EntityFactory::DestroyItemEntity(g_registry, itemEntity);
	}
	catch (...)
	{
		restore();
		throw;
	}
	if (g_registry.valid(itemEntity))
	{
		restore();
		LOG_ERROR("ITEM_DESTROY: factory did not retire item {}", id);
		return;
	}

	if (allocation)
	{
#ifdef M2_USE_POOL
		pool_.Destroy(allocation);
#else
#ifndef DEBUG_ALLOC
		M2_DELETE(allocation);
#else
		M2_DELETE_EX(allocation, file, line);
#endif
#endif
	}
}

LPITEM ITEM_MANAGER::Find(uint32_t id)
{
	const auto it = m_map_pkItemByID.find(id);
	if (it == m_map_pkItemByID.end())
		return nullptr;

	return ResolveManagedItem(it->second);
}

LPITEM ITEM_MANAGER::FindByVID(uint32_t vid)
{
	const auto it = m_VIDMap.find(vid);
	if (it == m_VIDMap.end())
		return nullptr;

	return ResolveManagedItem(it->second);
}

TItemTable* ITEM_MANAGER::GetTable(uint32_t vnum)
{
	const int rnum = RealNumber(vnum);

	if (rnum < 0)
	{
		for (const auto p : m_vec_item_vnum_range_info)
		{
			if ((p->dwVnum < vnum) &&
				vnum < (p->dwVnum + p->dwVnumRange))
			{
				return p;
			}
		}

		return nullptr;
	}

	return &m_vec_prototype[rnum];
}

int ITEM_MANAGER::RealNumber(const uint32_t vnum) const
{
	int bot = 0;
	int top = (int)m_vec_prototype.size() - 1; // Cs?kenteni kell 1-gyel a top ?t??

	const TItemTable* pTable = m_vec_prototype.data();

	while (bot <= top) // M?os?ott felt?el
	{
		const int mid = bot + (top - bot) / 2; // Biztons?osabb k??s??t? kisz???a

		if ((pTable + mid)->dwVnum == vnum)
			return mid;

		if ((pTable + mid)->dwVnum < vnum)
			bot = mid + 1;
		else
			top = mid - 1;
	}

	return -1; // Ha nem tal?hat?a vnum, -1-et ad vissza
}

bool ITEM_MANAGER::GetVnum(const char* c_pszName, uint32_t& r_dwVnum)
{
	int len = strlen(c_pszName);

	TItemTable* pTable = m_vec_prototype.data();

	for (uint32_t i = 0; i < m_vec_prototype.size(); ++i, ++pTable)
	{
#ifdef ENABLE_MULTI_NAMES
		if (!strncasecmp(c_pszName, pTable->szLocaleName[DEFAULT_LANGUAGE], len))
#else
		if (!strncasecmp(c_pszName, pTable->szLocaleName, len))
#endif
		{
			r_dwVnum = pTable->dwVnum;
			return true;
		}
	}

	return false;
}

bool ITEM_MANAGER::GetVnumByOriginalName(const char* c_pszName, uint32_t& r_dwVnum)
{
	int len = strlen(c_pszName);

	TItemTable* pTable = m_vec_prototype.data();

	for (uint32_t i = 0; i < m_vec_prototype.size(); ++i, ++pTable)
	{
		if (!strncasecmp(c_pszName, pTable->szName, len))
		{
			r_dwVnum = pTable->dwVnum;
			return true;
		}
	}

	return false;
}

std::set<uint32_t> g_set_lotto;

void load_lotto()
{
	static int bLoaded = false;

	if (bLoaded)
		return;

	bLoaded = true;
	FILE* fp = fopen("lotto.txt", "r");

	if (!fp)
		return;

	char buf[256];

	while (fgets(buf, 256, fp))
	{
		char* psz = strchr(buf, '\n');

		if (nullptr != psz)
			*psz = '\0';

		uint32_t dw = 0;
		str_to_number(dw, buf);
		g_set_lotto.insert(dw);
	}

	fclose(fp);
}

uint32_t lotto()
{
	load_lotto();

	char szBuf[6 + 1];

	do
	{
		for (int i = 0; i < 6; ++i)
			szBuf[i] = 48 + number(1, 9);

		szBuf[6] = '\0';

		uint32_t dw = 0;
		str_to_number(dw, szBuf);

		if (!g_set_lotto.contains(dw))
		{
			FILE* fp = fopen("lotto.txt", "a+");
			if (fp)
			{
				fprintf(fp, "%u\n", dw);
				fclose(fp);
			}
			return dw;
		}
	} while (1);
}


class CItemDropInfo
{
public:
	CItemDropInfo(int iLevelStart, int iLevelEnd, int iPercent, uint32_t dwVnum) :
		m_iLevelStart(iLevelStart), m_iLevelEnd(iLevelEnd), m_iPercent(iPercent), m_dwVnum(dwVnum)
	{
	}

	int	m_iLevelStart;
	int	m_iLevelEnd;
	int	m_iPercent; // 1 ~ 1000
	uint32_t	m_dwVnum;

	friend bool operator < (const CItemDropInfo& l, const CItemDropInfo& r)
	{
		return l.m_iLevelEnd < r.m_iLevelEnd;
	}
};

extern std::vector<CItemDropInfo> g_vec_pkCommonDropItem[MOB_RANK_MAX_NUM];

// 20050503.ipkn.
// iMinimum º¸´Ù ÀÛÀ¸¸é iDefault ¼¼ÆÃ (´Ü, iMinimumÀº 0º¸´Ù Ä¿¾ßÇÔ)
// 1, 0 ½ÄÀ¸·Î ON/OFF µÇ´Â ¹æ½ÄÀ» Áö¿øÇÏ±â À§ÇØ Á¸Àç
int GetDropPerKillPct(int iMinimum, int iDefault, int iDeltaPercent, const char* c_pszFlag)
{
	int iVal = 0;

	if ((iVal = quest::CQuestManager::instance().GetEventFlag(c_pszFlag)))
	{
		if (!test_server)
		{
			if (iVal < iMinimum)
				iVal = iDefault;

			if (iVal < 0)
				iVal = iDefault;
		}
	}

	if (iVal == 0)
		return 0;

	// ±âº» ¼¼ÆÃÀÏ¶§ (iDeltaPercent=100)
	// 40000 iVal ¸¶¸®´ç ÇÏ³ª ´À³¦À» ÁÖ±â À§ÇÑ »ó¼öÀÓ
	return (40000 * iDeltaPercent / iVal);
}

bool ITEM_MANAGER::GetDropPct(LPCHARACTER pkChr, LPCHARACTER pkKiller, OUT int& iDeltaPercent, OUT int& iRandRange)
{
	return GetDropPct(pkChr ? pkChr->GetEntityHandle() : entt::null,
		pkKiller ? pkKiller->GetEntityHandle() : entt::null, iDeltaPercent, iRandRange);
}

bool ITEM_MANAGER::GetDropPct(entt::entity victim, entt::entity killer,
	OUT int& iDeltaPercent, OUT int& iRandRange)
{
	if (victim == entt::null || killer == entt::null ||
		!g_registry.valid(victim) || !g_registry.valid(killer))
		return false;

	const int killerLevel = ecs::PointSystem::GetLevel(killer);
	iDeltaPercent = 100;
	if (!ecs::PlayerRuntime::IsStone(victim) &&
		ecs::PlayerRuntime::GetMobRank(victim) >= MOB_RANK_BOSS)
	{
		iDeltaPercent = PERCENT_LVDELTA_BOSS(
			killerLevel, ecs::PointSystem::GetLevel(victim));
	}
	else
	{
		iDeltaPercent = PERCENT_LVDELTA(
			killerLevel, ecs::PointSystem::GetLevel(victim));
	}

	const uint8_t rank = ecs::PlayerRuntime::GetMobRank(victim);
	if (1 == number(1, 50000))
		iDeltaPercent += 1000;
	else if (1 == number(1, 10000))
		iDeltaPercent += 500;

	LOG_INFO("CreateDropItem for level: {} rank: {} pct: {}", killerLevel, rank, iDeltaPercent);
	iDeltaPercent = iDeltaPercent * CHARACTER_MANAGER::instance().GetMobItemRate(killer) / 100;

	const int itemDropBonus = ecs::PointSystem::Get(killer, POINT_ITEM_DROP_BONUS);
	if (itemDropBonus > 0)
	{
		const int added = std::min((itemDropBonus / 10) * 5, 50);
		iDeltaPercent += added;
	}

#ifdef ENABLE_NEW_COMMON_BONUSES
	if (ecs::PointSystem::Get(killer, APPLY_DOUBLE_DROP_ITEM) > 0)
		iDeltaPercent *= 2;
#endif

	if (ecs::PlayerRuntime::GetPremiumRemainSeconds(killer, PREMIUM_ITEM) > 0 ||
		ItemSystem::IsEquipUniqueGroup(killer, UNIQUE_GROUP_DOUBLE_ITEM))
	{
		iDeltaPercent *= 2;
	}

	if (ecs::PointSystem::Get(killer, POINT_PC_BANG_DROP_BONUS) > 0 &&
		ecs::PlayerRuntime::IsPCBang(killer))
	{
		iDeltaPercent += iDeltaPercent *
			ecs::PointSystem::Get(killer, POINT_PC_BANG_DROP_BONUS) / 100;
	}

	iRandRange = 4000000;
#ifdef ENABLE_EVENT_MANAGER
	int extraDrop = CPrivManager::instance().GetPriv(killer, PRIV_ITEM_DROP) +
		(ItemSystem::IsEquipUniqueItem(killer, UNIQUE_ITEM_DOUBLE_ITEM) ? 100 : 0);
	const auto event = CHARACTER_MANAGER::Instance().CheckEventIsActive(
		ITEM_DROP_EVENT, ecs::PlayerRuntime::GetEmpire(killer));
	if (event != nullptr)
		extraDrop += event->value[0];
	iRandRange = iRandRange * 100 / (100 + extraDrop);
#else
	const int extraDrop = CPrivManager::instance().GetPriv(killer, PRIV_ITEM_DROP) +
		(ItemSystem::IsEquipUniqueItem(killer, UNIQUE_ITEM_DOUBLE_ITEM) ? 100 : 0);
	iRandRange = iRandRange * 100 / (100 + extraDrop);
#endif

	return true;
}
#ifdef __SEND_TARGET_INFO__
bool ITEM_MANAGER::CreateDropItemVector(LPCHARACTER pkChr, LPCHARACTER pkKiller, std::vector<TargetInfoItem>& items)
{
	const entt::entity chr = pkChr ? pkChr->GetEntityHandle() : entt::null;
	const entt::entity killer = pkKiller ? pkKiller->GetEntityHandle() : entt::null;
#ifdef ENABLE_METINSTONE_DROP_BUGFIX_RAZOR9D
	if (pkChr && ecs::PlayerRuntime::IsStone(chr) &&
		!IsRegisteredDropMob(ecs::PlayerRuntime::GetRaceNum(chr)))
	{
		LOG_INFO("[DROP-BLOKK] Metinko {} ({}) nincs mob_drop_item.txt-ben - CreateDropItemVector megszakitva.",
			ecs::PlayerRuntime::GetName(chr).data(),
			ecs::PlayerRuntime::GetRaceNum(chr));
		return false;
	}
#endif
	if (!pkChr || !pkKiller || AffectSystem::IsPolymorphed(chr) || ecs::PlayerRuntime::IsPC(chr))
		return false;

	const int level = ecs::PointSystem::GetLevel(killer);
	const uint8_t rank = ecs::PlayerRuntime::GetMobRank(chr);
	const uint32_t race = ecs::PlayerRuntime::GetRaceNum(chr);
	const bool isStone = ecs::PlayerRuntime::IsStone(chr);

	const auto add = [&items](uint32_t vnum, uint32_t count)
	{
		if (vnum != 0 && count != 0)
			items.push_back({vnum, count});
	};

	for (const CItemDropInfo& info : g_vec_pkCommonDropItem[rank])
	{
		if (isStone || level < info.m_iLevelStart || level > info.m_iLevelEnd)
			continue;

		const TItemTable* table = GetTable(info.m_dwVnum);
		if (!table)
			continue;
		if (table->bType == ITEM_POLYMORPH && info.m_dwVnum != pkChr->GetPolymorphItemVnum())
			continue;
		add(info.m_dwVnum, 1);
	}

	if (!isStone)
	{
		if (const auto it = m_map_pkDropItemGroup.find(race); it != m_map_pkDropItemGroup.end())
		{
			for (const auto& info : it->second->GetVector())
				add(info.dwVnum, info.iCount);
		}
	}

	if (const auto it = m_map_pkMobItemGroup.find(race); it != m_map_pkMobItemGroup.end())
	{
		CMobItemGroup* group = it->second;
		if (group && !group->IsEmpty())
		{
			const auto& info = group->GetOne();
			add(info.dwItemVnum, info.iCount);
		}
	}

	if (!isStone)
	{
		if (const auto it = m_map_pkLevelItemGroup.find(race);
			it != m_map_pkLevelItemGroup.end() && it->second->GetLevelLimit() <= static_cast<uint32_t>(level))
		{
			for (const auto& info : it->second->GetVector())
				add(info.dwVNum, info.iCount);
		}

		const bool hasDoubleDrop = ecs::PlayerRuntime::GetPremiumRemainSeconds(pkKiller->GetEntityHandle(), PREMIUM_ITEM) > 0 ||
			pkKiller->IsEquipUniqueGroup(UNIQUE_GROUP_DOUBLE_ITEM)
#ifdef ENABLE_NEW_COMMON_BONUSES
			|| ecs::PointSystem::Get(killer, APPLY_DOUBLE_DROP_ITEM) > 0
#endif
			;
		if (hasDoubleDrop)
		{
			if (const auto it = m_map_pkGloveItemGroup.find(race); it != m_map_pkGloveItemGroup.end())
			{
				for (const auto& info : it->second->GetVector())
					add(info.dwVnum, info.iCount);
			}
		}
	}

	if (pkChr->GetMobDropItemVnum() && m_map_dwEtcItemDropProb.contains(pkChr->GetMobDropItemVnum()))
		add(pkChr->GetMobDropItemVnum(), 1);

	if (isStone)
	{
		add(CombatSystem::GetDropMetinStoneVnum(pkChr->GetEntityHandle()), 1);
		add(pkChr->GetDropMetinStofaVnum(), 1);
		add(pkChr->GetDropMetinSaccaVnum(), 1);
	}

	return !items.empty();
}
#endif
bool ITEM_MANAGER::CreateDropItem(LPCHARACTER pkChr, LPCHARACTER pkKiller, std::vector<entt::entity>& vec_item)
{
	const entt::entity chr = pkChr ? pkChr->GetEntityHandle() : entt::null;
	const entt::entity killer = pkKiller ? pkKiller->GetEntityHandle() : entt::null;
#ifdef ENABLE_METINSTONE_DROP_BUGFIX_RAZOR9d
	const CMobItemGroup* pGroup = quest::CQuestManager::instance().GetMobDropItem(ecs::PlayerRuntime::GetRaceNum(chr));
	if (!pGroup || pGroup->IsEmpty())
	{
		LOG_INFO("[DROP BLOCKED] Metin VNUM {} nincs regisztr?va mob_drop_item.txt-ben!", ecs::PlayerRuntime::GetRaceNum(chr));
		return false;
	}


#endif
	int iLevel = ecs::PointSystem::GetLevel(killer);

	int iDeltaPercent, iRandRange;
	if (!GetDropPct(pkChr, pkKiller, iDeltaPercent, iRandRange))
		return false;

	uint8_t bRank = ecs::PlayerRuntime::GetMobRank(chr);
	entt::entity item = entt::null;

	// Common Drop Items
	auto it = g_vec_pkCommonDropItem[bRank].begin();

	while (!ecs::PlayerRuntime::IsStone(chr) && it != g_vec_pkCommonDropItem[bRank].end())
	{
		const CItemDropInfo& c_rInfo = *(it++);

		if (iLevel < c_rInfo.m_iLevelStart || iLevel > c_rInfo.m_iLevelEnd)
			continue;

		int iPercent = (c_rInfo.m_iPercent * iDeltaPercent) / 100;
		//		3, "CreateDropItem %d ~ %d %d(%d)", c_rInfo.m_iLevelStart, c_rInfo.m_iLevelEnd, c_rInfo.m_dwVnum, iPercent, c_rInfo.m_iPercent);
		LOG_ERROR("CreateDropItem {} ~ {} {}({})", c_rInfo.m_iLevelStart, c_rInfo.m_iLevelEnd, c_rInfo.m_dwVnum, iPercent, c_rInfo.m_iPercent);

		if (iPercent >= number(1, iRandRange))
		{
			TItemTable* table = GetTable(c_rInfo.m_dwVnum);

			if (!table)
				continue;

			item = entt::null;

			if (table->bType == ITEM_POLYMORPH)
			{
				if (c_rInfo.m_dwVnum == pkChr->GetPolymorphItemVnum())
				{
					item = CreateItem(c_rInfo.m_dwVnum, 1, 0, true);

					if (ItemSystem::IsValidItem(item))
						ItemSystem::SetItemSocket(item, 0, ecs::PlayerRuntime::GetRaceNum(chr));
				}
			}
			else
				item = CreateItem(c_rInfo.m_dwVnum, 1, 0, true);

			if (ItemSystem::IsValidItem(item)) vec_item.push_back(item);
		}
	}

	// Drop Item Group
	{
		if (ecs::PlayerRuntime::GetRaceNum(chr) == 4815)
		{
			LOG_ERROR("VIKING DROP CHECK: killer_lv={} mob_lv={} delta={} rand={}", pkKiller ? ecs::PointSystem::GetLevel(killer) : 0, ecs::PointSystem::GetLevel(chr), iDeltaPercent, iRandRange);
		}
		auto it = m_map_pkDropItemGroup.find(ecs::PlayerRuntime::GetRaceNum(chr));

		if (!ecs::PlayerRuntime::IsStone(chr) && it != m_map_pkDropItemGroup.end())
		{
			auto v = it->second->GetVector();

			for (uint32_t i = 0; i < v.size(); ++i)
			{

				int iPercent = (v[i].dwPct * iDeltaPercent) / 100;

				if (iPercent >= number(1, iRandRange))
				{

					item = CreateItem(v[i].dwVnum, v[i].iCount, 0, true);
					if (ItemSystem::IsValidItem(item))
					{
						if (ItemSystem::GetItemType(item) == ITEM_POLYMORPH)
						{
							if (ItemSystem::GetItemVnum(item) == pkChr->GetPolymorphItemVnum())
							{
								ItemSystem::SetItemSocket(item, 0, ecs::PlayerRuntime::GetRaceNum(chr));
							}
						}
						if (ecs::PlayerRuntime::GetRaceNum(chr) == 4815)
						{
							LOG_ERROR("VIKING DROP ROLL: item={} pct_raw={} final={} count={}", v[i].dwVnum, v[i].dwPct, iPercent, v[i].iCount);
						}
						vec_item.push_back(item);
					}
				}
			}
		}
	}

	// MobDropItem Group
	{
		auto it = m_map_pkMobItemGroup.find(ecs::PlayerRuntime::GetRaceNum(chr));

		if (it != m_map_pkMobItemGroup.end())
		{
			CMobItemGroup* pGroup = it->second;

			// MOB_DROP_ITEM_BUG_FIX
			// 20050805.myevan.MobDropItem ¿¡ ¾ÆÀÌÅÛÀÌ ¾øÀ» °æ¿ì CMobItemGroup::GetOne() Á¢±Ù½Ã ¹®Á¦ ¹ß»ý ¼öÁ¤
			if (pGroup && !pGroup->IsEmpty())
			{
				int iPercent = 40000 * iDeltaPercent / pGroup->GetKillPerDrop();
				if (iPercent >= number(1, iRandRange))
				{

					const CMobItemGroup::SMobItemGroupInfo& info = pGroup->GetOne();
					item = CreateItem(info.dwItemVnum, info.iCount, 0, true, info.iRarePct);
					if (ItemSystem::IsValidItem(item)) vec_item.push_back(item);
				}

			}
			// END_OF_MOB_DROP_ITEM_BUG_FIX
		}
	}

	// Level Item Group
	{
		auto it = m_map_pkLevelItemGroup.find(ecs::PlayerRuntime::GetRaceNum(chr));

		if (it != m_map_pkLevelItemGroup.end())
		{
			if (!ecs::PlayerRuntime::IsStone(chr) && it->second->GetLevelLimit() <= (uint32_t)iLevel)
			{
				auto v = it->second->GetVector();

				for (uint32_t i = 0; i < v.size(); i++)
				{
					if (v[i].dwPct >= (uint32_t)number(1, 1000000/*iRandRange*/))
					{
						uint32_t dwVnum = v[i].dwVNum;
						item = CreateItem(dwVnum, v[i].iCount, 0, true);
						if (ItemSystem::IsValidItem(item)) vec_item.push_back(item);
					}
				}
			}
		}
	}

	{
		if (!ecs::PlayerRuntime::IsStone(chr) && ((ecs::PlayerRuntime::GetPremiumRemainSeconds(pkKiller->GetEntityHandle(), PREMIUM_ITEM) > 0) || (pkKiller->IsEquipUniqueGroup(UNIQUE_GROUP_DOUBLE_ITEM))
#ifdef ENABLE_NEW_COMMON_BONUSES
			|| (ecs::PointSystem::Get(killer, APPLY_DOUBLE_DROP_ITEM) > 0)
#endif
			))
		{
			auto it = m_map_pkGloveItemGroup.find(ecs::PlayerRuntime::GetRaceNum(chr));

			if (it != m_map_pkGloveItemGroup.end())
			{
				auto v = it->second->GetVector();

				for (uint32_t i = 0; i < v.size(); ++i)
				{
					int iPercent = (v[i].dwPct * iDeltaPercent) / 100;

					if (iPercent >= number(1, iRandRange))
					{
						uint32_t dwVnum = v[i].dwVnum;
						item = CreateItem(dwVnum, v[i].iCount, 0, true);
						if (ItemSystem::IsValidItem(item)) vec_item.push_back(item);
					}
				}
			}
		}
	}

	// ÀâÅÛ
	if (pkChr->GetMobDropItemVnum())
	{
		auto it = m_map_dwEtcItemDropProb.find(pkChr->GetMobDropItemVnum());

		if (it != m_map_dwEtcItemDropProb.end())
		{
			int iPercent = (it->second * iDeltaPercent) / 100;

			if (iPercent >= number(1, iRandRange))
			{
				item = CreateItem(pkChr->GetMobDropItemVnum(), 1, 0, true);
				if (ItemSystem::IsValidItem(item)) vec_item.push_back(item);
			}
		}
	}

	if (ecs::PlayerRuntime::IsStone(chr))
	{
		if (CombatSystem::GetDropMetinStoneVnum(pkChr->GetEntityHandle()))
		{
			//if (ecs::PointSystem::GetLevel(((pkKiller) ? (pkKiller)->GetEntityHandle() : entt::null)) - ecs::PointSystem::GetLevel(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null)) >= 30)
			//{
			//	return false;
			//}
			int iPercent = (CombatSystem::GetDropMetinStonePct(pkChr->GetEntityHandle()) * iDeltaPercent) * 400;
			if (iPercent >= number(1, iRandRange))
			{
				item = CreateItem(CombatSystem::GetDropMetinStoneVnum(pkChr->GetEntityHandle()), 1, 0, true);
				if (ItemSystem::IsValidItem(item))
					vec_item.push_back(item);
			}
		}

		//if (pkChr->GetDropMetinStofaVnum())
		//{
		//	int iPercent = (pkChr->GetDropMetinStofaPct() * iDeltaPercent) * 400;
		//	if (iPercent >= number(1, iRandRange))
		//	{
		//		item = CreateItem(pkChr->GetDropMetinStofaVnum(), 1, 0, true);
		//		if (ItemSystem::IsValidItem(item))
		//			vec_item.push_back(item);
		//	}
		//}

		//if (pkChr->GetDropMetinSaccaVnum())
		//{
		//	int iPercent = (pkChr->GetDropMetinSaccaPct() * iDeltaPercent) * 400;
		//	if (iPercent >= number(1, iRandRange))
		//	{
		//		item = CreateItem(pkChr->GetDropMetinSaccaVnum(), 1, 0, true);
		//		if (ItemSystem::IsValidItem(item))
		//			vec_item.push_back(item);
		//	}
		//}
	}

	if (pkKiller->IsHorseRiding() &&
		GetDropPerKillPct(1000, 1000000, iDeltaPercent, "horse_skill_book_drop") >= number(1, iRandRange))
	{
		LOG_INFO("EVENT HORSE_SKILL_BOOK_DROP");

		if (ItemSystem::IsValidItem(item = CreateItem(ITEM_HORSE_SKILL_TRAIN_BOOK, 1, 0, true)))
			vec_item.push_back(item);
	}


	if (GetDropPerKillPct(100, 1000, iDeltaPercent, "lotto_drop") >= number(1, iRandRange))
	{
		uint32_t* pdw = M2_NEW uint32_t[3];

		pdw[0] = 50001;
		pdw[1] = 1;
		pdw[2] = quest::CQuestManager::instance().GetEventFlag("lotto_round");

		// Çà¿îÀÇ ¼­´Â ¼ÒÄÏÀ» ¼³Á¤ÇÑ´Ù
		DBManager::instance().ReturnQuery(QID_LOTTO, ecs::PlayerRuntime::GetPlayerID(killer), pdw,
			"INSERT INTO lotto_list VALUES(0, 'server%s', %u, NOW())",
			get_table_postfix(), ecs::PlayerRuntime::GetPlayerID(killer));
	}

	//
	// ½ºÆä¼È µå·Ó ¾ÆÀÌÅÛ
	//
	//CreateQuestDropItem(pkChr, pkKiller, vec_item, iDeltaPercent, iRandRange);
#ifdef ENABLE_EVENT_MANAGER

	if (ecs::PointSystem::GetLevel(killer) - ecs::PointSystem::GetLevel(chr) >= 30)
	{
		//eridj no
	}
	else
	{

		CHARACTER_MANAGER::Instance().CheckEventForDrop(chr, killer, vec_item);
	}
#endif





	for (const entt::entity item : vec_item)
	{
		if (ItemSystem::IsValidItem(item))
			DBManager::instance().SendMoneyLog(MONEY_LOG_DROP, ItemSystem::GetItemVnum(item), ItemSystem::GetItemCount(item));
	}

	return !vec_item.empty();
}

// ADD_GRANDMASTER_SKILL
int GetThreeSkillLevelAdjust(int level)
{
	if (level < 40)
		return 32;
	if (level < 45)
		return 16;
	if (level < 50)
		return 8;
	if (level < 55)
		return 4;
	if (level < 60)
		return 2;
	return 1;
}
// END_OF_ADD_GRANDMASTER_SKILL

// DROPEVENT_CHARSTONE
// drop_char_stone 1
// drop_char_stone.percent_lv01_10 5
// drop_char_stone.percent_lv11_30 10
// drop_char_stone.percent_lv31_MX 15
// drop_char_stone.level_range	   10
static struct DropEvent_CharStone
{
	int percent_lv01_10;
	int percent_lv11_30;
	int percent_lv31_MX;
	int level_range;
	bool alive;

	DropEvent_CharStone()
	{
		percent_lv01_10 = 100;
		percent_lv11_30 = 200;
		percent_lv31_MX = 300;
		level_range = 10;
		alive = false;
	}
} gs_dropEvent_charStone;

static int __DropEvent_CharStone_GetDropPercent(int killer_level)
{
	int killer_levelStep = (killer_level - 1) / 10;

	switch (killer_levelStep)
	{
	case 0:
		return gs_dropEvent_charStone.percent_lv01_10;

	case 1:
	case 2:
		return gs_dropEvent_charStone.percent_lv11_30;
	}

	return gs_dropEvent_charStone.percent_lv31_MX;
}

static void __DropEvent_CharStone_DropItem(CHARACTER& killer, CHARACTER& victim, ITEM_MANAGER& itemMgr, std::vector<entt::entity>& vec_item)
{
#ifdef ENABLE_METINSTONE_DROP_BUGFIX_RAZOR9D
	if (victim.IsStone())
	{
		const auto victimEntity = ((&victim) ? (&victim)->GetEntityHandle() : entt::null);
		const uint32_t victimRace = ecs::PlayerRuntime::GetRaceNum(victimEntity);
		if (!itemMgr.IsRegisteredDropMob(victimRace))
		{
			LOG_INFO("[DROP-BLOKK-EVENT] Metinko {} ({}) nincs mob_drop_item.txt-ben   event drop letiltva.", victim.GetName(), victimRace);
			return;
		}
	}
#endif
	if (!gs_dropEvent_charStone.alive)
		return;

	int killer_level = ecs::PointSystem::GetLevel(killer.GetEntityHandle());
	int dropPercent = __DropEvent_CharStone_GetDropPercent(killer_level);

	int MaxRange = 10000;

	if (number(1, MaxRange) <= dropPercent)
	{
		int log_level = (test_server || ecs::PlayerRuntime::GetGMLevel(((&killer) ? (&killer)->GetEntityHandle() : entt::null)) >= GM_LOW_WIZARD) ? 0 : 1;
		int victim_level = ecs::PointSystem::GetLevel(victim.GetEntityHandle());
		int level_diff = victim_level - killer_level;

		if (level_diff >= +gs_dropEvent_charStone.level_range || level_diff <= -gs_dropEvent_charStone.level_range)
		{
			LOG_INFO("dropevent.drop_char_stone.level_range_over: killer({}: lv{}), victim({}: lv:{}), level_diff({})", killer.GetName(), ecs::PointSystem::GetLevel(killer.GetEntityHandle()), victim.GetName(), ecs::PointSystem::GetLevel(victim.GetEntityHandle()), level_diff);
			return;
		}

		static const int Stones[] = { 30210, 30211, 30212, 30213, 30214, 30215, 30216, 30217, 30218, 30219, 30258, 30259, 30260, 30261, 30262, 30263 };
		int item_vnum = Stones[number(0, _countof(Stones))];

		entt::entity p_item = entt::null;

		if (ItemSystem::IsValidItem(p_item = itemMgr.CreateItem(item_vnum, 1, 0, true)))
		{
			vec_item.push_back(p_item);

			LOG_INFO("dropevent.drop_char_stone.item_drop: killer({}: lv{}), victim({}: lv:{}), item_name({})", killer.GetName(), ecs::PointSystem::GetLevel(killer.GetEntityHandle()), victim.GetName(), ecs::PointSystem::GetLevel(victim.GetEntityHandle()), ItemSystem::GetItemName(p_item));
		}
	}
}

bool DropEvent_CharStone_SetValue(const std::string& name, int value)
{
	if (name == "drop_char_stone")
	{
		gs_dropEvent_charStone.alive = value;

		if (value)
			LOG_INFO("dropevent.drop_char_stone = on");
		else
			LOG_INFO("dropevent.drop_char_stone = off");

	}
	else if (name == "drop_char_stone.percent_lv01_10")
		gs_dropEvent_charStone.percent_lv01_10 = value;
	else if (name == "drop_char_stone.percent_lv11_30")
		gs_dropEvent_charStone.percent_lv11_30 = value;
	else if (name == "drop_char_stone.percent_lv31_MX")
		gs_dropEvent_charStone.percent_lv31_MX = value;
	else if (name == "drop_char_stone.level_range")
		gs_dropEvent_charStone.level_range = value;
	else
		return false;

	LOG_INFO("dropevent.drop_char_stone: {}", gs_dropEvent_charStone.alive ? true : false);
	LOG_INFO("dropevent.drop_char_stone.percent_lv01_10: {:f}", gs_dropEvent_charStone.percent_lv01_10 / 100.0f);
	LOG_INFO("dropevent.drop_char_stone.percent_lv11_30: {:f}", gs_dropEvent_charStone.percent_lv11_30 / 100.0f);
	LOG_INFO("dropevent.drop_char_stone.percent_lv31_MX: {:f}", gs_dropEvent_charStone.percent_lv31_MX / 100.0f);
	LOG_INFO("dropevent.drop_char_stone.level_range: {}", gs_dropEvent_charStone.level_range);

	return true;
}

// END_OF_DROPEVENT_CHARSTONE

// fixme
// À§ÀÇ °Í°ú ÇÔ²² quest·Î »¬°Í »©º¸ÀÚ.
// ÀÌ°Å ³Ê¹« ´õ·´ÀÝ¾Æ...
// ”?. ÇÏµåÄÚµù ½È´Ù ¤Ì¤Ð
// °è·® ¾ÆÀÌÅÛ º¸»ó ½ÃÀÛ.
// by rtsummit °íÄ¡ÀÚ ÁøÂ¥
static struct DropEvent_RefineBox
{
	int percent_low;
	int low;
	int percent_mid;
	int mid;
	int percent_high;
	//int level_range;
	bool alive;

	DropEvent_RefineBox()
	{
		percent_low = 100;
		low = 20;
		percent_mid = 100;
		mid = 45;
		percent_high = 100;
		//level_range = 10;
		alive = false;
	}
} gs_dropEvent_refineBox;

static entt::entity __DropEvent_RefineBox_GetDropItem(CHARACTER& killer, CHARACTER& victim, ITEM_MANAGER& itemMgr)
{
	static const int lowerBox[] = { 50197, 50198, 50199 };
	static const int lowerBox_range = 3;
	static const int midderBox[] = { 50203, 50204, 50205, 50206 };
	static const int midderBox_range = 4;
	static const int higherBox[] = { 50207, 50208, 50209, 50210, 50211 };
	static const int higherBox_range = 5;

	if (ecs::PlayerRuntime::GetMobRank(victim.GetEntityHandle()) < MOB_RANK_KNIGHT)
		return entt::null;

	int killer_level = ecs::PointSystem::GetLevel(killer.GetEntityHandle());
	//int level_diff = victim_level - killer_level;

	//if (level_diff >= +gs_dropEvent_refineBox.level_range || level_diff <= -gs_dropEvent_refineBox.level_range)
	//{
	//	log_level,
	//		"dropevent.drop_refine_box.level_range_over: killer(%s: lv%d), victim(%s: lv:%d), level_diff(%d)",
	//		killer.GetName(), ecs::PointSystem::GetLevel(killer.GetEntityHandle()), victim.GetName(), ecs::PointSystem::GetLevel(victim.GetEntityHandle()), level_diff);
	//	return NULL;
	//}

	if (killer_level <= gs_dropEvent_refineBox.low)
	{
		if (number(1, gs_dropEvent_refineBox.percent_low) == 1)
		{
			return itemMgr.CreateItem(lowerBox[number(1, lowerBox_range) - 1], 1, 0, true);
		}
	}
	else if (killer_level <= gs_dropEvent_refineBox.mid)
	{
		if (number(1, gs_dropEvent_refineBox.percent_mid) == 1)
		{
			return itemMgr.CreateItem(midderBox[number(1, midderBox_range) - 1], 1, 0, true);
		}
	}
	else
	{
		if (number(1, gs_dropEvent_refineBox.percent_high) == 1)
		{
			return itemMgr.CreateItem(higherBox[number(1, higherBox_range) - 1], 1, 0, true);
		}
	}
	return entt::null;
}

static void __DropEvent_RefineBox_DropItem(CHARACTER& killer, CHARACTER& victim, ITEM_MANAGER& itemMgr, std::vector<entt::entity>& vec_item)
{
	if (!gs_dropEvent_refineBox.alive)
		return;

	int log_level = (test_server || ecs::PlayerRuntime::GetGMLevel(((&killer) ? (&killer)->GetEntityHandle() : entt::null)) >= GM_LOW_WIZARD) ? 0 : 1;

	const entt::entity p_item = __DropEvent_RefineBox_GetDropItem(killer, victim, itemMgr);

	if (ItemSystem::IsValidItem(p_item))
	{
		vec_item.push_back(p_item);

		LOG_INFO("dropevent.drop_refine_box.item_drop: killer({}: lv{}), victim({}: lv:{}), item_name({})", killer.GetName(), ecs::PointSystem::GetLevel(killer.GetEntityHandle()), victim.GetName(), ecs::PointSystem::GetLevel(victim.GetEntityHandle()), ItemSystem::GetItemName(p_item));
	}
}

bool DropEvent_RefineBox_SetValue(const std::string& name, int value)
{
	if (name == "refine_box_drop")
	{
		gs_dropEvent_refineBox.alive = value;

		if (value)
			LOG_INFO("refine_box_drop = on");
		else
			LOG_INFO("refine_box_drop = off");

	}
	else if (name == "refine_box_low")
		gs_dropEvent_refineBox.percent_low = value < 100 ? 100 : value;
	else if (name == "refine_box_mid")
		gs_dropEvent_refineBox.percent_mid = value < 100 ? 100 : value;
	else if (name == "refine_box_high")
		gs_dropEvent_refineBox.percent_high = value < 100 ? 100 : value;
	//else if (name == "refine_box_level_range")
	//	gs_dropEvent_refineBox.level_range = value;
	else
		return false;

	LOG_INFO("refine_box_drop: {}", gs_dropEvent_refineBox.alive ? true : false);
	LOG_INFO("refine_box_low: {}", gs_dropEvent_refineBox.percent_low);
	LOG_INFO("refine_box_mid: {}", gs_dropEvent_refineBox.percent_mid);
	LOG_INFO("refine_box_high: {}", gs_dropEvent_refineBox.percent_high);
	//0, "refine_box_low_level_range: %d", gs_dropEvent_refineBox.level_range);

	return true;
}
// °³·® ¾ÆÀÌÅÛ º¸»ó ³¡.


uint32_t ITEM_MANAGER::GetRefineFromVnum(uint32_t dwVnum)
{
	auto it = m_map_ItemRefineFrom.find(dwVnum);
	if (it != m_map_ItemRefineFrom.end())
		return it->second;
	return 0;
}

const CSpecialItemGroup* ITEM_MANAGER::GetSpecialItemGroup(uint32_t dwVnum)
{
	auto it = m_map_pkSpecialItemGroup.find(dwVnum);
	if (it != m_map_pkSpecialItemGroup.end())
	{
		return it->second;
	}
	return nullptr;
}

const CSpecialAttrGroup* ITEM_MANAGER::GetSpecialAttrGroup(uint32_t dwVnum)
{
	auto it = m_map_pkSpecialAttrGroup.find(dwVnum);
	if (it != m_map_pkSpecialAttrGroup.end())
	{
		return it->second;
	}
	return nullptr;
}

uint32_t ITEM_MANAGER::GetMaskVnum(uint32_t dwVnum)
{
	if (auto it = m_map_new_to_ori.find(dwVnum); it != m_map_new_to_ori.end())
	{
		return it->second;
	}
	else
		return 0;
}



#ifdef ENABLE_EXTRA_INVENTORY
bool ITEM_MANAGER::IsExtraItem(uint32_t vnum)
{
	TItemTable* p = GetTable(vnum);
	if (!p)
		return false;

	switch (vnum) {
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

	switch (p->bType) {
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
		uint8_t subtype = p->bSubType;
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
#endif

#ifdef __INGAME_WIKI__
uint32_t ITEM_MANAGER::GetWikiItemStartRefineVnum(uint32_t dwVnum)
{
	auto baseItemName = GetWikiItemBaseRefineName(dwVnum);
	if (baseItemName.empty())
		return 0;

	uint32_t manage_vnum = dwVnum;
	while (!(strcmp(baseItemName.c_str(), GetWikiItemBaseRefineName(manage_vnum).c_str())))
		--manage_vnum;

	return (manage_vnum + 1);
}

std::string ITEM_MANAGER::GetWikiItemBaseRefineName(uint32_t dwVnum)
{
	auto* tbl = GetTable(dwVnum);
	if (!tbl)
		return "";

#ifdef ENABLE_MULTI_NAMES
	auto* p = const_cast<char*>(strrchr(tbl->szLocaleName[DEFAULT_LANGUAGE], '+'));
#else
	auto* p = const_cast<char*>(strrchr(tbl->szLocaleName, '+'));
#endif
	if (!p)
		return "";

#ifdef ENABLE_MULTI_NAMES
	std::string sFirstItemName(tbl->szLocaleName[DEFAULT_LANGUAGE], (tbl->szLocaleName[DEFAULT_LANGUAGE] + (p - tbl->szLocaleName[DEFAULT_LANGUAGE])));
#else
	std::string sFirstItemName(tbl->szLocaleName, (tbl->szLocaleName + (p - tbl->szLocaleName)));
#endif
	return sFirstItemName;
}

int ITEM_MANAGER::GetWikiMaxRefineLevel(uint32_t dwVnum)
{
	uint32_t manage_vnum = (GetWikiItemStartRefineVnum(dwVnum) + 1);
	if (manage_vnum <= 1)
		return CommonWikiData::MAX_REFINE_COUNT;

	int refine_count = 0;
	std::string firstName, secondName;

	while (GetRefineFromVnum(manage_vnum) != 0)
	{
		firstName = GetWikiItemBaseRefineName(manage_vnum);
		secondName = GetWikiItemBaseRefineName(dwVnum);

		if (strcmp(firstName.c_str(), secondName.c_str()))
			break;

		++manage_vnum;
		++refine_count;
	}

	return MAX(refine_count, CommonWikiData::MAX_REFINE_COUNT);
}

CommonWikiData::TWikiInfoTable* ITEM_MANAGER::GetItemWikiInfo(uint32_t vnum)
{
	auto it = m_wikiInfoMap.find(vnum);
	if (it != m_wikiInfoMap.end())
		return it->second.get();

	auto* tbl = GetTable(vnum);
	if (!tbl)
		return nullptr;

	auto newTable = new CommonWikiData::TWikiInfoTable();
	newTable->is_common = false;

	for (int it = 0; it < MOB_RANK_MAX_NUM && !newTable->is_common; ++it)
		for (auto it2 = g_vec_pkCommonDropItem[it].begin(); it2 != g_vec_pkCommonDropItem[it].end() && !newTable->is_common; ++it2)
			if (it2->m_dwVnum == vnum)
				newTable->is_common = true;

	newTable->origin_vnum = 0;
	newTable->chest_info_count = 0;
	m_wikiInfoMap.insert(std::make_pair(vnum, std::unique_ptr<CommonWikiData::TWikiInfoTable>(newTable)));

	if ((tbl->bType == ITEM_WEAPON || tbl->bType == ITEM_ARMOR || tbl->bType == ITEM_BELT) && vnum % 10 == 0 && tbl->dwRefinedVnum)
		newTable->refine_infos_count = GetWikiMaxRefineLevel(vnum);
	//else if (tbl->bType == ITEM_GIFTBOX || (tbl->bType == ITEM_USE && tbl->bSubType == USE_SPECIAL))
	else if (tbl->bType == ITEM_GIFTBOX || (tbl->dwVnum >= 10960 && tbl->dwVnum <= 10968))
	{
		CSpecialItemGroup* ptr = nullptr;
		auto it = m_map_pkSpecialItemGroup.find(vnum);
		if (it == m_map_pkSpecialItemGroup.end())
		{
			it = m_map_pkQuestItemGroup.find(vnum);
			if (it != m_map_pkQuestItemGroup.end())
				ptr = it->second;
		}
		else
			ptr = it->second;

		if (ptr)
			newTable->chest_info_count = ptr->m_vecItems.size();
	}

	return newTable;
}

std::vector<CommonWikiData::TWikiRefineInfo> ITEM_MANAGER::GetWikiRefineInfo(uint32_t vnum)
{
	std::vector<CommonWikiData::TWikiRefineInfo> _rV;
	_rV.clear();

	auto* tbl = GetTable(vnum);
	if (!tbl)
		return _rV;

	const TRefineTable* refTbl;
	auto* tblTemp = tbl;
	bool success = true;
	const int maxRefineLevelCount = GetWikiMaxRefineLevel(vnum);

	for (uint8_t i = 0; i < maxRefineLevelCount; ++i)
	{
		if (!tblTemp) {
			success = false;
			break;
		}

		refTbl = CRefineManager::instance().GetRefineRecipe(tblTemp->wRefineSet);
		if (!refTbl) {
			success = false;
			break;
		}

		CommonWikiData::TWikiRefineInfo tmpStruct;
		tmpStruct.index = i;
		tmpStruct.mat_count = refTbl->material_count;
		tmpStruct.price = refTbl->cost;

		for (auto j = 0; j < CommonWikiData::REFINE_MATERIAL_MAX_NUM; ++j)
		{
			tmpStruct.materials[j].vnum = refTbl->materials[j].vnum;
			tmpStruct.materials[j].count = refTbl->materials[j].count;
		}

		_rV.emplace_back(tmpStruct);
		tblTemp = GetTable(tblTemp->dwVnum + 1);
	}

	return (success ? _rV : std::vector<CommonWikiData::TWikiRefineInfo>());
}
#ifdef ENABLE_METINSTONE_DROP_BUGFIX_RAZOR9d
bool ITEM_MANAGER::IsRegisteredDropMob(uint32_t dwMobVnum) const
{
	return m_map_pkMobItemGroup.find(dwMobVnum) != m_map_pkMobItemGroup.end();
}

#endif
std::vector<CSpecialItemGroup::CSpecialItemInfo> ITEM_MANAGER::GetWikiChestInfo(uint32_t vnum)
{
	std::vector<CSpecialItemGroup::CSpecialItemInfo> _rV;
	_rV.clear();

	auto* tbl = GetTable(vnum);
	if (!tbl)
		return _rV;

	//if (tbl->bType == ITEM_GIFTBOX || (tbl->bType == ITEM_USE && tbl->bSubType == USE_SPECIAL))
	if (tbl->bType == ITEM_GIFTBOX || (tbl->dwVnum >= 10960 && tbl->dwVnum <= 10968))
	{
		CSpecialItemGroup* ptr = nullptr;
		auto it = m_map_pkSpecialItemGroup.find(vnum);
		if (it == m_map_pkSpecialItemGroup.end()) {
			it = m_map_pkQuestItemGroup.find(vnum);
			if (it != m_map_pkQuestItemGroup.end())
				ptr = it->second;
		}
		else {
			ptr = it->second;
		}

		if (ptr) {
			_rV.assign(ptr->m_vecItems.begin(), ptr->m_vecItems.end());
		}
	}

	return _rV;

}
#endif
