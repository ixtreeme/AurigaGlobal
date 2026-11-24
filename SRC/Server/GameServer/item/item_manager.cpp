#include "stdafx.h"
#include <common/retired_content.h>
#include "../ecs/CharacterAccessors.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/components/item_components.hpp"
#include "../ecs/AIHelpers.hpp"
#include "utils.h"
#include "config.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "log.h"
#include "skill.h"
#include "priv_manager.h"
#include "questmanager.h"
#include "unique_item.h"
#include "safebox.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include <unordered_set>
#include "../ecs/components/inventory_components.hpp"
#include "locale_service.h"
#include "item.h"
#include "item_manager.h"
#include "../ecs/EntityFactory.hpp"
#include "../ecs/Registry.hpp"
#include "../ecs/ItemRegistry.hpp"

#include <common/VnumHelper.h>
#ifdef ENABLE_STOLE_COSTUME
#include <common/stole_length.h>
#endif

namespace
{
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
	: m_iTopOfTable(0), m_dwCurrentID(0)
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
    // Detachment may mutate manager indexes. Snapshot handles, and use the same
    // guarded native lifecycle as runtime destruction, without deleting DB rows.
    std::vector<entt::entity> items;
    items.reserve(m_VIDMap.size());
    for (const auto& [vid, item] : m_VIDMap) items.push_back(item);
    for (const auto item : items) {
        if (!ItemSystem::IsValidItem(item)) continue;
        auto* flags = g_registry.try_get<ecs::ItemFlags>(item);
        if (!flags) continue;
        const bool skipSave = flags->skipSave;
        flags->skipSave = true;
        ScopeExit restorePolicy {[item, skipSave] {
            if (g_registry.valid(item))
                if (auto* remaining = g_registry.try_get<ecs::ItemFlags>(item))
                    remaining->skipSave = skipSave;
        }};
        DestroyItem(item);
    }
    std::erase_if(m_VIDMap, [](const auto& row) { return !ItemSystem::IsValidItem(row.second); });
    std::erase_if(m_map_pkItemByID, [](const auto& row) { return !ItemSystem::IsValidItem(row.second); });
}

namespace { uint64_t g_bonusCatalogRevision = 1; }
uint64_t ITEM_MANAGER::BonusCatalogRevision() const { return g_bonusCatalogRevision; }

bool ITEM_MANAGER::Initialize(TItemTable* table, int size)
{
    ++g_bonusCatalogRevision;
	if (size < 0 || (size && !table)) return false;
	std::vector<TItemTable> incoming;
	if (size) incoming.assign(table, table + size);
	std::erase_if(incoming, [](const TItemTable& row) { return auriga::retired_content::Item(row.bType, row.bSubType); });
	size = static_cast<int>(incoming.size());
	// Retain old storage until every live entity has been rebound below.
	m_vec_prototype.swap(incoming);
	m_vec_item_vnum_range_info.clear();
	m_map_vid.clear();

	int	i;

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
	char buf[512] {};

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

    LOG_INFO("ITEM_VID_MAP {}", m_VIDMap.size());
    for (const auto& [vid, item] : m_VIDMap) {
        if (!ItemSystem::IsValidItem(item)) continue;
        const auto* current = GetTable(ItemSystem::GetItemOriginalVnum(item));
        if (!current) LOG_ERROR("cannot reset item table: item={}", ItemSystem::GetItemID(item));
        ItemSystem::RefreshItemPrototype(item, current);
    }

	return true;
}


#ifdef ENABLE_ITEM_EXTRA_PROTO
bool ITEM_MANAGER::InitializeExtraProto(TItemExtraProto* table, uint32_t count)
{
    ++g_bonusCatalogRevision;
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

    std::vector<entt::entity> items;
    for (const auto& [vid, item] : m_VIDMap) items.push_back(item);
    for (const auto item : items) {
        if (!ItemSystem::IsValidItem(item)) continue;
        const auto extra = map.find(ItemSystem::GetItemOriginalVnum(item));
        ItemSystem::SetItemExtraProto(item, extra != map.end() ? &extra->second : nullptr);
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

entt::entity ITEM_MANAGER::CreateItem(uint32_t, uint32_t, uint32_t, bool, int, bool)
{
    // Retired callers cannot create hidden legacy inventory or ground entities.
    // Native issuance must use the durable ItemStore transaction builders.
    return entt::null;
}


void ITEM_MANAGER::RemoveItem(entt::entity itemEntity, const char* reason)
{
	if (!ItemSystem::IsValidItem(itemEntity) || m_itemsBeingDestroyed.contains(itemEntity))
		return;

	const uint32_t id = ItemSystem::GetItemID(itemEntity);
	const uint32_t vid = ItemSystem::GetItemVID(itemEntity);

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
				return storage != entt::null ? SafeboxSystem::GetItem(storage, cell) : entt::null;
			}
#if defined(AURIGA_LEGACY_ITEM_TESTS)
			if (window == MOUNT_INVENTORY) return MountSystem::GetMountInventoryItem(owner, cell);
#endif
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
			if (detached()) DestroyItemNow(itemEntity);
			return;
		}

		if (window == SAFEBOX || window == MALL)
		{
			// Each safebox entry point revalidates the storage entity, so a
			// removal callback may close it without leaving a dangling handle.
			const auto storage = SafeboxSystem::Get(owner, window);
			if (storage == entt::null || SafeboxSystem::GetItem(storage, cell) != itemEntity) return;
			SafeboxSystem::Remove(storage, cell);
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
				if (detached()) DestroyItemNow(itemEntity);
				return;
			}
			if (recordedSlot() != itemEntity) return;
			InventorySystem::RemoveFromCharacter(itemEntity);
			if (!live()) return;
		}

		if (!detached()) return;

#if defined(AURIGA_LEGACY_ITEM_TESTS)
		if (window == MOUNT_INVENTORY)
		{
			if (g_registry.valid(owner)) ecs::PointSystem::Compute(owner);
			if (g_registry.valid(owner)) NetworkSyncSystem::PointsPacket(owner);
#ifdef ENABLE_FAKE_SHOP_HEADER
			if (g_registry.valid(owner)) MountSystem::UpdateMountCountOverheadToViewers(owner);
#endif
			if (!detached()) return;
		}
#endif
	}

	DestroyItemNow(itemEntity);
}

void ITEM_MANAGER::ForgetItem(entt::entity item, uint32_t id, uint32_t vid)
{
	if (auto it = m_map_pkItemByID.find(id); it != m_map_pkItemByID.end() && it->second == item)
		m_map_pkItemByID.erase(it);
	if (auto it = m_VIDMap.find(vid); it != m_VIDMap.end() && it->second == item)
		m_VIDMap.erase(it);
}
void ITEM_MANAGER::DestroyItem(entt::entity itemEntity)
{
	if (!ItemSystem::IsValidItem(itemEntity) || m_itemsBeingDestroyed.contains(itemEntity))
		return;

	m_itemsBeingDestroyed.insert(itemEntity);
	ItemDestructionGuard guard {m_itemsBeingDestroyed, itemEntity};
	DestroyItemNow(itemEntity);
}

void ITEM_MANAGER::DestroyItemNow(entt::entity itemEntity)
{
	const uint32_t id = ItemSystem::GetItemID(itemEntity);
	const uint32_t vid = ItemSystem::GetItemVID(itemEntity);

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
		if (window == SAFEBOX || window == MALL ||
#if defined(AURIGA_LEGACY_ITEM_TESTS)
			window == MOUNT_INVENTORY ||
#endif
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

	LOG_INFO("ITEM_DESTROY {}:{}", ItemSystem::GetItemName(itemEntity), id);

	// Preserve live indexes if final ECS retirement fails or is rejected.
	const bool wasByID = id && m_map_pkItemByID.contains(id) && m_map_pkItemByID.at(id) == itemEntity;
	const bool wasByVID = m_VIDMap.contains(vid) && m_VIDMap.at(vid) == itemEntity;
	const auto restore = [&] {
		if (!ItemSystem::IsValidItem(itemEntity))
			return;
		// Never overwrite an identity published by a nested callback.
		if (wasByID) m_map_pkItemByID.try_emplace(id, itemEntity);
		if (wasByVID) m_VIDMap.try_emplace(vid, itemEntity);
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

}

bool ItemSystem::DestroyLoadedDuplicateItem(entt::entity item)
{
    if (!IsValidItem(item) ||
        !g_registry.all_of<ecs::ItemFlags, ecs::ItemOwner, ecs::ItemLocation>(item))
        return false;

    const uint32_t id = GetItemID(item);
    const bool originalSkipSave = g_registry.get<ecs::ItemFlags>(item).skipSave;
    ScopeExit restoreSavePolicy {[item, originalSkipSave] {
        // A rejected/throwing cleanup must not leave a surviving item unsavable.
        // Reacquire by versioned handle; never touch a recycled replacement.
        if (g_registry.valid(item))
            if (auto* flags = g_registry.try_get<ecs::ItemFlags>(item))
                flags->skipSave = originalSkipSave;
    }};
    // Suppress legacy exchange while the stale entity is being detached.
    g_registry.get<ecs::ItemFlags>(item).skipSave = true;
    LOG_ERROR("DUP_ITEM_DESTROY_BEGIN entity={} id={} vid={} vnum={} last_owner_pid={} window={} cell={}",
        entt::to_integral(item), id, GetItemVID(item), GetItemVnum(item),
        GetItemLastOwnerPID(item), static_cast<int>(GetItemWindow(item)), GetItemCell(item));

    // The manager owns detachment, its reentry guard and native indexes.
    // Never detach first outside that protected lifecycle.
    ITEM_MANAGER::instance().DestroyItem(item);
    const bool destroyed = !g_registry.valid(item);
    LOG_ERROR("DUP_ITEM_DESTROY_END entity={} id={} destroyed={}", entt::to_integral(item), id, destroyed);
    return destroyed;
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

bool ITEM_MANAGER::GetDropPct(entt::entity victim, entt::entity killer,
	OUT double& iDeltaPercent, OUT int& iRandRange)
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

	LOG_INFO("Drop rate for level: {} rank: {} pct: {}", killerLevel, rank, iDeltaPercent);
	iDeltaPercent = iDeltaPercent * CHARACTER_MANAGER::instance().GetMobItemRate(killer) / 100;

	double itemDropBonus = ecs::PointSystem::GetPrecise(killer, POINT_ITEM_DROP_BONUS);
	if (itemDropBonus > 0)
	{
		const double added = std::min((itemDropBonus / 10) * 5, 50.0);
		iDeltaPercent += added;
	}

#ifdef ENABLE_NEW_COMMON_BONUSES
	if (ecs::PointSystem::GetPrecise(killer, POINT_DOUBLE_DROP_ITEM) > 0)
		iDeltaPercent *= 2;
#endif

	if (ecs::PlayerRuntime::GetPremiumRemainSeconds(killer, PREMIUM_ITEM) > 0 ||
		ItemSystem::IsEquipUniqueGroup(killer, UNIQUE_GROUP_DOUBLE_ITEM))
	{
		iDeltaPercent *= 2;
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
#ifdef ENABLE_STOLE_COSTUME
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
