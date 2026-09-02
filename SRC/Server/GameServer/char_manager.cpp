#include "stdafx.h"
#include "ecs/systems/PointSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "constants.h"
#include "utils.h"
#include "desc.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "mob_manager.h"
#include "party.h"
#include "regen.h"
#include "p2p.h"
#include "dungeon.h"
#include "db.h"
#include "config.h"
#include "questmanager.h"
#include "questlua.h"
#include "locale_service.h"
#include "shutdown_manager.h"
#include "sectree_manager.h"
#include "cmd.h"
#include <common/CommonDefines.h>
#include <common/service.h>

#include "item_manager.h"

#include <algorithm>
#include <random>
#include <string_view>

#include <unordered_set>
#include "safebox.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/VIDRegistry.hpp"
#include "ecs/PIDRegistry.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/components/identity_components.hpp"
namespace
{
	inline double uniform_random(double min, double max)
	{
		static thread_local std::mt19937 rng(std::random_device{}());
		std::uniform_real_distribution<double> dist(min, max);
		return dist(rng);
	}
	constexpr uint32_t kChanceScale = 10000;
	bool RollEventChance(uint32_t chanceBasisPoints)
	{
		const uint32_t clampedChance = std::clamp(chanceBasisPoints, 0u, kChanceScale);
		const double roll = uniform_random(0.0, 100.0);
		const double chancePercent = static_cast<double>(clampedChance) / 100.0;
		return roll < chancePercent;
	}
#ifdef ENABLE_EVENT_MANAGER
	static bool g_bDungeonTicketExtraMetinSpawn = false;
	static inline void CalcDungeonTicketExtraPos(int baseX, int baseY, int idx, int& outX, int& outY)
	{

		const int STEP = 35;


		const int ring = (idx / 8) + 1;
		const int r = STEP * ring;

		int dx = 0, dy = 0;
		switch (idx % 8)
		{
		case 0: dx = +r; dy = 0;  break;
		case 1: dx = -r; dy = 0;  break;
		case 2: dx = 0;  dy = +r; break;
		case 3: dx = 0;  dy = -r; break;
		case 4: dx = +r; dy = +r; break;
		case 5: dx = -r; dy = +r; break;
		case 6: dx = +r; dy = -r; break;
		default:dx = -r; dy = -r; break;
		}

		outX = baseX + dx;
		outY = baseY + dy;
	}

	// vdelmek + utols llapot
	static bool g_bPrevDungeonTicketActive = false;
	static int  g_iPrevDungeonTicketExtraCount = -1;
	// ------------------------------------------------------------
	// Map1 Mass Spawn events (event_table.sql driven)
	//  - TANAKA_EVENT: vnum 5000
	//  - GOLDEN_FROG_EVENT: vnum 124
	// New wave spawns ONLY when previous wave is fully cleared.
	// Positions are based on the old Lua grid (tile coords).
	// ------------------------------------------------------------
// NOTE:
// TANAKA_EVENT and GOLDEN_FROG_EVENT are part of the Event Manager enum (tables.h).
// Do NOT define them as preprocessor macros here, otherwise the numeric indices can
// drift and you'll end up triggering the wrong scheduled event from event_table.sql.

	struct TMap1WaveEventState
	{
		uint8_t eventIndex;
		uint32_t mobVnum;
		const char* name;
		int32_t mapIndex;
		int32_t defaultWaveCount;

		bool wasActive = false;
		bool hadWave = false;
		time_t nextRetryTime = 0;

		std::unordered_set<uint32_t> aliveVIDs;
	};

	static TMap1WaveEventState s_TanakaWave{ TANAKA_EVENT, 5000u, "Tanaka", 1, 300 };
	static TMap1WaveEventState s_GoldenFrogWave{ GOLDEN_FROG_EVENT, 124u, "Golden Frogs", 1, 300 };

	// Lua tile grid
	constexpr int32_t kWaveStartX = 81;
	constexpr int32_t kWaveEndX = 450;
	constexpr int32_t kWaveStartY = 66;
	constexpr int32_t kWaveEndY = 450;
	constexpr int32_t kWaveStep = 20;

	// GM safe zone (do not spawn inside)
	constexpr int32_t kGmX1 = 249;
	constexpr int32_t kGmY1 = 233;
	constexpr int32_t kGmX2 = 298;
	constexpr int32_t kGmY2 = 278;

	static void Map1Wave_Prune(CHARACTER_MANAGER& mgr, TMap1WaveEventState& st)
	{
		if (st.aliveVIDs.empty())
			return;

		std::vector<uint32_t> toErase;
		toErase.reserve(st.aliveVIDs.size());

		for (const uint32_t vid : st.aliveVIDs)
		{
			auto* ch = mgr.Find(vid);
			if (!ch)
			{
				toErase.push_back(vid);
				continue;
			}

			const entt::entity character = ch->GetEntityHandle();

			if (CombatSystem::IsDead(character) || ecs::PlayerRuntime::GetMapIndex(character) != st.mapIndex || ecs::PlayerRuntime::GetRaceNum(character) != st.mobVnum)
			{
				toErase.push_back(vid);
				continue;
			}
		}

		for (const uint32_t vid : toErase)
			st.aliveVIDs.erase(vid);
	}

	static void Map1Wave_Cleanup(CHARACTER_MANAGER& mgr, TMap1WaveEventState& st)
	{
		for (const uint32_t vid : st.aliveVIDs)
		{
			if (auto* ch = mgr.Find(vid))
				mgr.DestroyCharacter(ch);
		}

		st.aliveVIDs.clear();
		st.hadWave = false;
		st.nextRetryTime = 0;
	}

	static int32_t Map1Wave_GetWaveCount(const TMap1WaveEventState& st, const TEventManagerData* ev)
	{
		int32_t waveCount = st.defaultWaveCount;

		// Optional: event_table value[0] can override wave size (0 -> default).
		if (ev && ev->value[0] > 0)
			waveCount = static_cast<int32_t>(ev->value[0]);

		if (waveCount < 1) waveCount = 1;
		if (waveCount > 1000) waveCount = 1000;
		return waveCount;
	}

	static int32_t Map1Wave_SpawnWave(CHARACTER_MANAGER& mgr, TMap1WaveEventState& st, const TEventManagerData* ev)
	{
		PIXEL_POSITION base;
		if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(st.mapIndex, base))
		{
			LOG_ERROR("Map1Wave_SpawnWave: cannot find base position for map {}", st.mapIndex);
			return 0;
		}

		st.aliveVIDs.clear();

		const int32_t targetCount = Map1Wave_GetWaveCount(st, ev);
		int32_t spawned = 0;

		for (int32_t tx = kWaveStartX; tx <= kWaveEndX && spawned < targetCount; tx += kWaveStep)
		{
			for (int32_t ty = kWaveStartY; ty <= kWaveEndY && spawned < targetCount; ty += kWaveStep)
			{
				const bool inGmZone = (tx >= kGmX1 && tx <= kGmX2 && ty >= kGmY1 && ty <= kGmY2);
				if (inGmZone)
					continue;

				const int32_t x = base.x + (tx * 100);
				const int32_t y = base.y + (ty * 100);

				if (auto* mob = mgr.SpawnMob(st.mobVnum, st.mapIndex, x, y, 0, true, -1, true))
				{
					st.aliveVIDs.insert(mob->GetLegacyVID());
					++spawned;
				}
			}
		}

		if (spawned > 0)
		{
			char buf[256];
			snprintf(buf, sizeof(buf), "[Event] %d %s have been spawned!", spawned, st.name);
			SendNoticeMap(buf, st.mapIndex, true);
			SendNoticeMap("No new spawns will occur until you kill the last one!", st.mapIndex, true);
			BroadcastNotice("!! EVENT STARTED!! ENTER:MAP1-->URIEL!!", true);

		}

		return spawned;
	}

	static void Map1Wave_UpdateOne(CHARACTER_MANAGER& mgr, TMap1WaveEventState& st, int iPulse)
	{
		const TEventManagerData* ev = mgr.CheckEventIsActive(st.eventIndex, 0);
		const bool active = (ev != nullptr);

		if (!active)
		{
			if (st.wasActive)
			{
				// Event ended -> cleanup remaining spawned mobs
				Map1Wave_Cleanup(mgr, st);
			}

			st.wasActive = false;
			return;
		}

		st.wasActive = true;

		// run once per second
		if (0 != (iPulse % PASSES_PER_SEC(1)))
			return;

		const time_t now = get_global_time();
		if (st.nextRetryTime != 0 && now < st.nextRetryTime)
			return;

		const size_t before = st.aliveVIDs.size();
		Map1Wave_Prune(mgr, st);

		// If previous wave just got cleared -> notice
		if (st.hadWave && before > 0 && st.aliveVIDs.empty())
		{
			SendNoticeMap("[Event] Event map clear!", st.mapIndex, true);
		}

		// Spawn new wave only if there are no alive mobs from this event
		if (st.aliveVIDs.empty())
		{
			const int32_t spawned = Map1Wave_SpawnWave(mgr, st, ev);
			if (spawned > 0)
			{
				st.hadWave = true;
				st.nextRetryTime = 0;
			}
			else
			{
				// If spawn was blocked everywhere, do not spam every second
				st.nextRetryTime = now + 5;
			}
		}
	}

	static void Map1MassSpawnEvents_Update(CHARACTER_MANAGER& mgr, int iPulse)
	{
		Map1Wave_UpdateOne(mgr, s_TanakaWave, iPulse);
		Map1Wave_UpdateOne(mgr, s_GoldenFrogWave, iPulse);
	}

#endif

}

#ifdef ENABLE_EVENT_MANAGER
// Called from CHARACTER::Dead (char_battle.cpp) to decrement wave counters
void Map1MassSpawnEvent_OnMobDead(uint32_t vid)
{
	s_TanakaWave.aliveVIDs.erase(vid);
	s_GoldenFrogWave.aliveVIDs.erase(vid);
}
#endif




CHARACTER_MANAGER::CHARACTER_MANAGER() : itemshopUpdateTime(0),
m_iVIDCount(0), dummy1{},
m_pkChrSelectedStone(nullptr),
m_bUsePendingDestroy(false)
{
	m_iMobItemRate = 100;
	m_iMobDamageRate = 100;
	m_iMobGoldAmountRate = 100;
	m_iMobGoldDropRate = 100;
	m_iMobExpRate = 100;

	m_iMobItemRatePremium = 100;
	m_iMobGoldAmountRatePremium = 100;
	m_iMobGoldDropRatePremium = 100;
	m_iMobExpRatePremium = 100;

	m_iUserDamageRate = 100;
	m_iUserDamageRatePremium = 100;
}

CHARACTER_MANAGER::~CHARACTER_MANAGER()
{
	Destroy();
}

void CHARACTER_MANAGER::Destroy()
{

#ifdef ENABLE_ITEMSHOP
	m_IShopManager.clear();
#endif

	auto it = m_map_pkChrByVID.begin();
	while (it != m_map_pkChrByVID.end()) {
		LPCHARACTER ch = it->second;
		M2_DESTROY_CHARACTER(ch); // m_map_pkChrByVID is changed here
		it = m_map_pkChrByVID.begin();
	}
}

void CHARACTER_MANAGER::GracefulShutdown()
{
	auto it = m_map_pkPCChr.begin();

	while (it != m_map_pkPCChr.end())
		it++->second->Disconnect("GracefulShutdown");
}

uint32_t CHARACTER_MANAGER::AllocVID()
{
	++m_iVIDCount;
	return m_iVIDCount;
}

LPCHARACTER CHARACTER_MANAGER::CreateCharacter(const char* name, uint32_t dwPID)
{
	uint32_t dwVID = AllocVID();

#ifdef M2_USE_POOL
	LPCHARACTER ch = pool_.Construct();
#else
	auto ch = new CHARACTER;
#endif
	ch->Create(name, dwVID, dwPID ? true : false);

	if (EntityFactory::EnsureLegacyCharacterEntity(g_registry, ch, dwVID) == entt::null) {
		--m_iVIDCount;
#ifdef M2_USE_POOL
		pool_.Destroy(ch);
#else
		M2_DELETE(ch);
#endif
		return nullptr;
	}

#ifdef ENABLE_BUG_FIXES
	if (dwVID != ch->GetLegacyVID()) {
		--m_iVIDCount;
		M2_DESTROY_CHARACTER(ch);
		return nullptr;
	}
#endif

	m_map_pkChrByVID.insert(std::make_pair(dwVID, ch));

	if (dwPID)
	{
		char szName[CHARACTER_NAME_MAX_LEN + 1];
		str_lower(name, szName, sizeof(szName));

		m_map_pkPCChr.insert(NAME_MAP::value_type(szName, ch));
		m_map_pkChrByPID.insert(std::make_pair(dwPID, ch));
	}

	return ch;
}

#ifndef DEBUG_ALLOC
void CHARACTER_MANAGER::DestroyCharacter(LPCHARACTER ch)
#else
void CHARACTER_MANAGER::DestroyCharacter(LPCHARACTER ch, const char* file, size_t line)
#endif
{
	if (!ch)
		return;

	// <Factor> Check whether it has been already deleted or not.
	const auto it = m_map_pkChrByVID.find(ch->GetLegacyVID());
	if (it == m_map_pkChrByVID.end()) {
		LOG_ERROR("[CHARACTER_MANAGER::DestroyCharacter] <Factor> {} not found", ch->GetLegacyVID());
		return; // prevent duplicated destrunction
	}

	const entt::entity character = ch->GetEntityHandle();

#ifdef __NEWPET_SYSTEM__
	if (ecs::PlayerRuntime::IsNPC(character) && !ch->IsPet() && !ch->IsNewPet() && ch->GetRider() == nullptr)
#else
	if (ecs::PlayerRuntime::IsNPC(character) && !ch->IsPet() && ch->GetRider() == NULL)
#endif
	{
		if (ch->GetDungeon())
		{
			ch->GetDungeon()->DeadCharacter(ch);
		}
	}

	if (m_bUsePendingDestroy)
	{
		m_set_pkChrPendingDestroy.insert(ch);
		return;
	}

	if (const auto it2 = m_set_pkChrForDelayedSave.find(ch); it2 != m_set_pkChrForDelayedSave.end()) {
		ch->SaveReal();
		m_set_pkChrForDelayedSave.erase(it2);
	}

	//if (ecs::PlayerRuntime::IsPC(character))											   // Ixtreeme fix -- ITEM_SAVE invalid owner pointer
	//	ITEM_MANAGER::instance().FlushDelayedSaveByOwner(ch);  // Ixtreeme fix -- ITEM_SAVE invalid owner pointer

	m_map_pkChrByVID.erase(it);

	if (true == ecs::PlayerRuntime::IsPC(character))
	{
		char szName[CHARACTER_NAME_MAX_LEN + 1];

		str_lower(ecs::PlayerRuntime::GetName(character).data(), szName, sizeof(szName));

		auto it = m_map_pkPCChr.find(szName);

		if (m_map_pkPCChr.end() != it)
			m_map_pkPCChr.erase(it);
	}

	if (0 != ecs::PlayerRuntime::GetPlayerID(character))
	{
		auto it = m_map_pkChrByPID.find(ecs::PlayerRuntime::GetPlayerID(character));

		if (m_map_pkChrByPID.end() != it)
		{
			m_map_pkChrByPID.erase(it);
		}
	}

	if (ecs::PlayerRuntime::IsPC(character)) {
		auto it = m_set_pkChrForDelayedSave.find(ch);
		if (m_set_pkChrForDelayedSave.end() != it)
		{
			ch->SaveReal();
			m_set_pkChrForDelayedSave.erase(it);
		}
	}

	UnregisterRaceNumMap(ch);

	RemoveFromStateList(ch);

	if (const entt::entity entity = ch->GetEntityHandle();
		entity != entt::null && g_registry.valid(entity))
	{
		// Phase 15E-final.LPENTITY.4-architect.D.6.fixup-6:
		// Run ViewCleanup explicitly while the ECS entity is still valid.
		//
		// CEntity::Destroy() (called from ~CHARACTER via M2_DELETE below)
		// also calls ViewCleanup, but by that point EntityFactory::Destroy
		// has already null-ed ch->GetEntityHandle() and reg.destroy()-ed
		// the entity. The D.6 ViewCleanup char-branch resolves
		// `EntityOf(this)` through `ch->GetEntityHandle()`, sees null, and
		// silently skips the ViewerMap walk - so no SendRemove burst goes
		// out to the dying character's peers.
		//
		// User-visible symptom: when a Metin stone (CHAR_TYPE_STONE mob) is
		// killed, the death animation packet (HEADER_GC_DEAD) reaches the
		// clients and starts the shatter animation, but the follow-up
		// HEADER_GC_CHARACTER_DELETE packet is never sent because no
		// SendRemove fired. The shattered fragments stay rendered on the
		// map indefinitely.
		//
		// Fix: invoke ViewCleanup here, then EntityFactory::Destroy. The
		// destructor's CEntity::Destroy will hit ViewCleanup again, but by
		// then m_map_view is empty (legacy clear) and the ECS handle is
		// null - both branches of the D.6 ViewCleanup are no-ops on the
		// second pass.
		ch->ViewCleanup();
		// The CHARACTER destructor still needs the ECS-backed inventory,
		// session and social components while it tears the legacy shell down.
		// EntityFactory::Destroy is deliberately deferred to the end of
		// CHARACTER::Destroy; destroying the entity here made ClearItem() see
		// an empty inventory and left live CItem objects with a dangling owner.
	}

#ifdef M2_USE_POOL
	pool_.Destroy(ch);
#else
#ifndef DEBUG_ALLOC
	M2_DELETE(ch);
#else
	M2_DELETE_EX(ch, file, line);
#endif
#endif
}

LPCHARACTER CHARACTER_MANAGER::Find(uint32_t dwVID)
{
	if (const entt::entity entity = CVIDRegistry::Instance().Find(dwVID);
		entity != entt::null && g_registry.valid(entity))
	{
		if (const auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(entity); legacy && legacy->ptr) {
			return legacy->ptr;
		}
	}

	const auto it = m_map_pkChrByVID.find(dwVID);

	if (m_map_pkChrByVID.end() == it)
		return nullptr;

	LOG_ERROR("VID_DRIFT fallback in CHARACTER_MANAGER::Find({})", dwVID);

	// <Factor> Added sanity check
	LPCHARACTER found = it->second;
	if (found != nullptr && dwVID != found->GetLegacyVID()) {
		LOG_ERROR("[CHARACTER_MANAGER::Find] <Factor> {} != {}", dwVID, found->GetLegacyVID());
		return nullptr;
	}
	return found;
}

LPCHARACTER CHARACTER_MANAGER::FindByPID(uint32_t dwPID)
{
	const auto it = m_map_pkChrByPID.find(dwPID);

	if (m_map_pkChrByPID.end() == it)
		return nullptr;

	// <Factor> Added sanity check
	LPCHARACTER found = it->second;
	const entt::entity character = found ? found->GetEntityHandle() : entt::null;
	if (found != nullptr && dwPID != ecs::PlayerRuntime::GetPlayerID(character)) {
		LOG_ERROR("[CHARACTER_MANAGER::FindByPID] <Factor> {} != {}", dwPID, ecs::PlayerRuntime::GetPlayerID(character));
		return nullptr;
	}
	return found;
}

entt::entity CHARACTER_MANAGER::FindEntity(uint32_t dwVID)
{
	// CVIDRegistry is the ECS-side authority for VID -> entity; Find() only
	// falls back to the legacy map (and logs VID_DRIFT) when they disagree.
	if (const entt::entity entity = CVIDRegistry::Instance().Find(dwVID);
		entity != entt::null && g_registry.valid(entity))
		return entity;

	LPCHARACTER found = Find(dwVID);
	return found ? found->GetEntityHandle() : entt::null;
}

entt::entity CHARACTER_MANAGER::FindPCEntity(const char* name)
{
	LPCHARACTER found = FindPC(name);
	return found ? found->GetEntityHandle() : entt::null;
}

entt::entity CHARACTER_MANAGER::FindEntityByPID(uint32_t dwPID)
{
	// CPIDRegistry is the ECS-side authority for PID -> entity, written where
	// the PlayerID component is. The legacy map is only a fallback, and a hit
	// there means the two indexes have drifted apart.
	if (const entt::entity entity = CPIDRegistry::Instance().Find(dwPID);
		entity != entt::null && g_registry.valid(entity))
		return entity;

	LPCHARACTER found = FindByPID(dwPID);
	if (!found)
		return entt::null;

	LOG_ERROR("PID_DRIFT: {} resolves through the legacy map but not CPIDRegistry", dwPID);
	return found->GetEntityHandle();
}

LPCHARACTER CHARACTER_MANAGER::FindPC(const char* name)
{
	char szName[CHARACTER_NAME_MAX_LEN + 1];
	str_lower(name, szName, sizeof(szName));
	const auto it = m_map_pkPCChr.find(szName);

	if (it == m_map_pkPCChr.end())
		return nullptr;

	// <Factor> Added sanity check
	LPCHARACTER found = it->second;
	const entt::entity character = found ? found->GetEntityHandle() : entt::null;
	if (found != nullptr && strncasecmp(szName, ecs::PlayerRuntime::GetName(character).data(), CHARACTER_NAME_MAX_LEN) != 0) {
		LOG_ERROR("[CHARACTER_MANAGER::FindPC] <Factor> {} != {}", name, ecs::PlayerRuntime::GetName(character).data());
		return nullptr;
	}
	return found;
}

LPCHARACTER CHARACTER_MANAGER::SpawnMobRandomPosition(uint32_t dwVnum, int32_t lMapIndex)
{
	const CMob* pkMob = CMobManager::instance().Get(dwVnum);

	if (!pkMob)
	{
		LOG_ERROR("no mob data for vnum {}", dwVnum);
		return nullptr;
	}

	if (!map_allow_find(lMapIndex))
	{
		LOG_ERROR("not allowed map {}", lMapIndex);
		return nullptr;
	}

	LPSECTREE_MAP pkSectreeMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
	if (pkSectreeMap == nullptr) {
		return nullptr;
	}

	int i;
	int32_t x, y;
	for (i = 0; i < 2000; i++)
	{
		x = number(1, pkSectreeMap->m_setting.iWidth / 100 - 1) * 100 + pkSectreeMap->m_setting.iBaseX;
		y = number(1, pkSectreeMap->m_setting.iHeight / 100 - 1) * 100 + pkSectreeMap->m_setting.iBaseY;
		//LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);
		LPSECTREE tree = pkSectreeMap->Find(x, y);

		if (!tree)
			continue;

		uint32_t dwAttr = tree->GetAttribute(x, y);

		if (IS_SET(dwAttr, ATTR_BLOCK | ATTR_OBJECT))
			continue;

		if (IS_SET(dwAttr, ATTR_BANPK))
			continue;

		break;
	}

	if (i == 2000)
	{
		LOG_ERROR("cannot find valid location");
		return nullptr;
	}

	LPSECTREE sectree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);

	if (!sectree)
	{
		LOG_INFO("SpawnMobRandomPosition: cannot create monster at non-exist sectree {} x {} (map {})", x, y, lMapIndex);
		return nullptr;
	}

#ifdef ENABLE_MULTI_NAMES
	LPCHARACTER ch = CHARACTER_MANAGER::instance().CreateCharacter(pkMob->m_table.szLocaleName[DEFAULT_LANGUAGE]);
#else
	LPCHARACTER ch = CHARACTER_MANAGER::instance().CreateCharacter(pkMob->m_table.szLocaleName);
#endif

	if (!ch)
	{
		LOG_INFO("SpawnMobRandomPosition: cannot create new character");
		return nullptr;
	}

	const entt::entity character = ch->GetEntityHandle();

	ch->SetProto(pkMob);

	// if mob is npc with no empire assigned, assign to empire of map
	if (pkMob->m_table.bType == CHAR_TYPE_NPC)
		if (ecs::PlayerRuntime::GetEmpire(character) == 0)
			ch->SetEmpire(SECTREE_MANAGER::instance().GetEmpireFromMapIndex(lMapIndex));

	ch->SetRotation(number(0, 360));

	if (!ecs::MovementSystem::Show(character, lMapIndex, x, y, 0, false))
	{
		M2_DESTROY_CHARACTER(ch);
		LOG_ERROR("SpawnMobRandomPosition: cannot show monster");
		return nullptr;
	}

	// Phase 8 diagnosis: keep startup ECS registration conservative.
	// NPCs/stones are required for quest resolution; bulk monster registration
	// on startup is deferred until the login path is stable again.
	if (ch)
	{
		if (pkMob->m_table.bType == CHAR_TYPE_STONE)
		{
			EntityFactory::CreateStone(g_registry, ch->GetMobTable(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetMapIndex(character), ch->GetLegacyVID());
		}
		else if (pkMob->m_table.bType == CHAR_TYPE_MONSTER)
		{
			EntityFactory::CreateMonster(g_registry, ch->GetMobTable(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetMapIndex(character), ch->GetLegacyVID());
		}
		else if (pkMob->m_table.bType == CHAR_TYPE_NPC || pkMob->m_table.bType == CHAR_TYPE_WARP || pkMob->m_table.bType == CHAR_TYPE_GOTO)
		{
			EntityFactory::CreateNPC(g_registry, ch->GetMobTable(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetMapIndex(character), ch->GetLegacyVID());
		}
	}

	return ch;
}

LPCHARACTER CHARACTER_MANAGER::SpawnMob(uint32_t dwVnum, int32_t lMapIndex, int32_t x, int32_t y, int32_t z, bool bSpawnMotion, int iRot, bool bShow)
{
	const CMob* pkMob = CMobManager::instance().Get(dwVnum);
	if (!pkMob)
	{
		LOG_ERROR("SpawnMob: no mob data for vnum {}", dwVnum);
		return nullptr;
	}

	if (!(pkMob->m_table.bType == CHAR_TYPE_NPC || pkMob->m_table.bType == CHAR_TYPE_WARP || pkMob->m_table.bType == CHAR_TYPE_GOTO) || mining::IsVeinOfOre(dwVnum))
	{
		LPSECTREE tree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);

		if (!tree)
		{
			LOG_INFO("no sectree for spawn at {} {} mobvnum {} mapindex {}", x, y, dwVnum, lMapIndex);
			//"no sectree for spawn at %d %d mobvnum %d mapindex %d", x, y, dwVnum, lMapIndex);
			return nullptr;
		}

		uint32_t dwAttr = tree->GetAttribute(x, y);

		bool is_set = false;

		if (mining::IsVeinOfOre(dwVnum))
			is_set = IS_SET(dwAttr, ATTR_BLOCK);
#ifdef ENABLE_EVENT_MANAGER
		else if (g_bDungeonTicketExtraMetinSpawn && pkMob->m_table.bType == CHAR_TYPE_STONE)
			// EXTRA metin esetn engedjk az ATTR_OBJECT-et (klnben nem lehet ugyanarra a pozira spawnolni)
			is_set = IS_SET(dwAttr, ATTR_BLOCK);
#endif
		else
			is_set = IS_SET(dwAttr, ATTR_BLOCK | ATTR_OBJECT);

		if (is_set)
		{
			// SPAWN_BLOCK_LOG
			static bool s_isLog = quest::CQuestManager::instance().GetEventFlag("spawn_block_log");
			static uint32_t s_nextTime = get_global_time() + 10000;

			uint32_t curTime = static_cast<uint32_t>(get_global_time());

			if (curTime > s_nextTime)
			{
				s_nextTime = curTime;
				s_isLog = quest::CQuestManager::instance().GetEventFlag("spawn_block_log");

			}

			if (s_isLog)
				//"SpawnMob: BLOCKED position for spawn %s %u at %d %d (attr %u)", pkMob->m_table.szName, dwVnum, x, y, dwAttr);
				LOG_INFO("SpawnMob: BLOCKED position for spawn {} {} at {} {} (attr {})", pkMob->m_table.szName, dwVnum, x, y, dwAttr);
			// END_OF_SPAWN_BLOCK_LOG
			return nullptr;
		}

		if (IS_SET(dwAttr, ATTR_BANPK))
		{
			//"SpawnMob: BAN_PK position for mob spawn %s %u at %d %d", pkMob->m_table.szName, dwVnum, x, y);
			LOG_INFO("SpawnMob: BAN_PK position for mob spawn {} {} at {} {}", pkMob->m_table.szName, dwVnum, x, y);
			return nullptr;
		}
	}

	const LPSECTREE sectree = SECTREE_MANAGER::instance().Get(lMapIndex, x, y);

	if (!sectree)
	{
		//"SpawnMob: cannot create monster at non-exist sectree %d x %d (map %d)", x, y, lMapIndex);
		LOG_INFO("SpawnMob: cannot create monster at non-exist sectree {} x {} (map {})", x, y, lMapIndex);
		return nullptr;
	}

#ifdef ENABLE_MULTI_NAMES
	LPCHARACTER ch = CHARACTER_MANAGER::instance().CreateCharacter(pkMob->m_table.szLocaleName[DEFAULT_LANGUAGE]);
#else
	LPCHARACTER ch = CHARACTER_MANAGER::instance().CreateCharacter(pkMob->m_table.szLocaleName);
#endif

	if (!ch)
	{
		//"SpawnMob: cannot create new character");
		LOG_INFO("SpawnMob: cannot create new character");
		return nullptr;
	}

	const entt::entity character = ch->GetEntityHandle();

	if (iRot == -1)
		iRot = number(0, 360);

	ch->SetProto(pkMob);
#ifdef ENABLE_EVENT_MANAGER
	if (g_bDungeonTicketExtraMetinSpawn && pkMob->m_table.bType == CHAR_TYPE_STONE)
		ch->SetDungeonTicketExtraMetin(true);
#endif

	// if mob is npc with no empire assigned, assign to empire of map
	if (pkMob->m_table.bType == CHAR_TYPE_NPC)
		if (ecs::PlayerRuntime::GetEmpire(character) == 0)
			ch->SetEmpire(SECTREE_MANAGER::instance().GetEmpireFromMapIndex(lMapIndex));

#ifdef ENABLE_ANCIENT_PYRAMID
	ch->SetRotation(iRot, true);
#else
	ch->SetRotation(iRot);
#endif

	if (bShow && !ecs::MovementSystem::Show(character, lMapIndex, x, y, z, bSpawnMotion))
	{
		M2_DESTROY_CHARACTER(ch);
		//"SpawnMob: cannot show monster");
		LOG_INFO("SpawnMob: cannot show monster");
		return nullptr;
	}
#ifdef ENABLE_EVENT_MANAGER
	// DUNGEON_TICKET_LOOT_EVENT: minden metin kapjon +value1 extra metint (dungeon mapok kivve)
	if (!g_bDungeonTicketExtraMetinSpawn && ch && pkMob->m_table.bType == CHAR_TYPE_STONE && !ch->IsDungeonTicketExtraMetin())
	{
		const TEventManagerData* ev = CheckEventIsActive(DUNGEON_TICKET_LOOT_EVENT, 0);
		if (ev)
		{
			// value1 = ev->value[0]
			int extraCount = (int)ev->value[0];

			// vdelem
			if (extraCount < 0) extraCount = 0;
			if (extraCount > 30) extraCount = 30;

			// dungeon kizrs
			const bool isDungeonMap =
				(lMapIndex >= 10000) ||
				(CDungeonManager::instance().FindByMapIndex(lMapIndex) != nullptr);

			if (!isDungeonMap && extraCount > 0)
			{
				g_bDungeonTicketExtraMetinSpawn = true;

				for (int i = 0; i < extraCount; ++i)
				{
					int sx, sy;
					CalcDungeonTicketExtraPos(x, y, i, sx, sy);
					SpawnMob(dwVnum, lMapIndex, sx, sy, z, bSpawnMotion, iRot, bShow);
				}


				g_bDungeonTicketExtraMetinSpawn = false;
			}
		}
	}
#endif

	// Phase 8 diagnosis: keep startup ECS registration conservative.
	// NPCs/stones are required for quest resolution; bulk monster registration
	// on startup is deferred until the login path is stable again.
	if (ch)
	{
		if (pkMob->m_table.bType == CHAR_TYPE_STONE)
		{
			EntityFactory::CreateStone(g_registry, ch->GetMobTable(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetMapIndex(character), ch->GetLegacyVID());
		}
		else if (pkMob->m_table.bType == CHAR_TYPE_MONSTER)
		{
			EntityFactory::CreateMonster(g_registry, ch->GetMobTable(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetMapIndex(character), ch->GetLegacyVID());
		}
		else if (pkMob->m_table.bType == CHAR_TYPE_NPC || pkMob->m_table.bType == CHAR_TYPE_WARP || pkMob->m_table.bType == CHAR_TYPE_GOTO)
		{
			EntityFactory::CreateNPC(g_registry, ch->GetMobTable(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), ecs::PlayerRuntime::GetMapIndex(character), ch->GetLegacyVID());
		}
	}

	// Phase 8 diagnosis - REMOVE AFTER SPAWN PATH IS VERIFIED
	if (bShow && ch)
	{
		const bool isNpcLike = (pkMob->m_table.bType == CHAR_TYPE_NPC || pkMob->m_table.bType == CHAR_TYPE_WARP || pkMob->m_table.bType == CHAR_TYPE_GOTO);
		if (ecs::PlayerRuntime::IsStone(character) || isNpcLike)
		{
		}
	}

	return ch;
}

LPCHARACTER CHARACTER_MANAGER::SpawnMobRange(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, bool bIsException, bool bSpawnMotion, bool bAggressive)
{
	const CMob* pkMob = CMobManager::instance().Get(dwVnum);

	if (!pkMob)
		return nullptr;

	if (pkMob->m_table.bType == CHAR_TYPE_STONE)	//   SPAWN  ִ.
		bSpawnMotion = true;

	int i = 16;

	while (i--)
	{
		int x = number(sx, ex);
		int y = number(sy, ey);
		/*
		   if (bIsException)
		   if (is_regen_exception(x, y))
		   continue;
		 */
		auto* ch = SpawnMob(dwVnum, lMapIndex, x, y, 0, bSpawnMotion);

		if (ch)
		{
			const entt::entity character = ch->GetEntityHandle();
			LOG_TRACE("MOB_SPAWN: {}({}) {}x{}", ecs::PlayerRuntime::GetName(character).data(), ch->GetLegacyVID(), ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character));
			if (bAggressive)
				ch->SetAggressive();
			return ch;
		}
	}

	return nullptr;
}

void CHARACTER_MANAGER::SelectStone(LPCHARACTER pkChr)
{

	m_pkChrSelectedStone = pkChr;
}

bool CHARACTER_MANAGER::SpawnMoveGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, int tx, int ty, LPREGEN pkRegen, bool bAggressive_)
{

	if (!dwVnum)
		return false;

	CMobGroup* pkGroup = CMobManager::Instance().GetGroup(dwVnum);

	if (!pkGroup)
	{
		LOG_ERROR("NOT_EXIST_GROUP_VNUM({}) Map({}) ", dwVnum, lMapIndex);
		return false;
	}

	LPCHARACTER pkChrMaster = nullptr;
	LPPARTY pkParty = nullptr;

	const std::vector<uint32_t>& c_rdwMembers = pkGroup->GetMemberVector();

	bool bSpawnedByStone = false;
	bool bAggressive = bAggressive_;

	if (m_pkChrSelectedStone)
	{
		bSpawnedByStone = true;
		if (m_pkChrSelectedStone->GetDungeon())
			bAggressive = true;
	}

	for (uint32_t i = 0; i < c_rdwMembers.size(); ++i)
	{
		LPCHARACTER tch = SpawnMobRange(c_rdwMembers[i], lMapIndex, sx, sy, ex, ey, true, bSpawnedByStone);

		if (!tch)
		{
			if (i == 0)	//  Ͱ  쿡 ׳
				return false;

			continue;
		}

		const entt::entity spawned = tch->GetEntityHandle();

		sx = ecs::PlayerRuntime::GetX(spawned) - number(300, 500);
		sy = ecs::PlayerRuntime::GetY(spawned) - number(300, 500);
		ex = ecs::PlayerRuntime::GetX(spawned) + number(300, 500);
		ey = ecs::PlayerRuntime::GetY(spawned) + number(300, 500);

		if (m_pkChrSelectedStone)
			tch->SetStone((m_pkChrSelectedStone ? m_pkChrSelectedStone->GetEntityHandle() : entt::null));
		else if (pkParty)
		{
			pkParty->Join(tch->GetLegacyVID());
			pkParty->Link(((tch) ? (tch)->GetEntityHandle() : entt::null));
		}
		else if (!pkChrMaster)
		{
			pkChrMaster = tch;
			pkChrMaster->SetRegen(pkRegen);

			pkParty = CPartyManager::instance().CreateParty(((pkChrMaster) ? (pkChrMaster)->GetEntityHandle() : entt::null));
		}
		if (bAggressive)
			tch->SetAggressive();

		if (ecs::MovementSystem::Goto(spawned, tx, ty))
			tch->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	}

	return true;
}

bool CHARACTER_MANAGER::SpawnGroupGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, LPREGEN pkRegen, bool bAggressive_, LPDUNGEON pDungeon)
{
	const uint32_t dwGroupID = CMobManager::Instance().GetGroupFromGroupGroup(dwVnum);

	if (dwGroupID != 0)
	{
		return SpawnGroup(dwGroupID, lMapIndex, sx, sy, ex, ey, pkRegen, bAggressive_, pDungeon);
	}
	else
	{
		LOG_ERROR("NOT_EXIST_GROUP_GROUP_VNUM({}) MAP({})", dwVnum, lMapIndex);
		return false;
	}
}

LPCHARACTER CHARACTER_MANAGER::SpawnGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, LPREGEN pkRegen, bool bAggressive_, LPDUNGEON pDungeon)
{

	if (!dwVnum)
		return nullptr;

	CMobGroup* pkGroup = CMobManager::Instance().GetGroup(dwVnum);

	if (!pkGroup)
	{
		LOG_ERROR("NOT_EXIST_GROUP_VNUM({}) Map({}) ", dwVnum, lMapIndex);
		return nullptr;
	}

	LPCHARACTER pkChrMaster = nullptr;
	LPPARTY pkParty = nullptr;

	const std::vector<uint32_t>& c_rdwMembers = pkGroup->GetMemberVector();

	bool bSpawnedByStone = false;
	bool bAggressive = bAggressive_;

	if (m_pkChrSelectedStone)
	{
		bSpawnedByStone = true;

		if (m_pkChrSelectedStone->GetDungeon())
			bAggressive = true;
	}

	LPCHARACTER chLeader = nullptr;

	for (uint32_t i = 0; i < c_rdwMembers.size(); ++i)
	{
		LPCHARACTER tch = SpawnMobRange(c_rdwMembers[i], lMapIndex, sx, sy, ex, ey, true, bSpawnedByStone);

		if (!tch)
		{
			if (i == 0)	//  Ͱ  쿡 ׳
				return nullptr;

			continue;
		}

		if (i == 0)
			chLeader = tch;

		tch->SetDungeon(pDungeon);

		const entt::entity spawned = tch->GetEntityHandle();

		sx = ecs::PlayerRuntime::GetX(spawned) - number(300, 500);
		sy = ecs::PlayerRuntime::GetY(spawned) - number(300, 500);
		ex = ecs::PlayerRuntime::GetX(spawned) + number(300, 500);
		ey = ecs::PlayerRuntime::GetY(spawned) + number(300, 500);

		if (m_pkChrSelectedStone)
			tch->SetStone((m_pkChrSelectedStone ? m_pkChrSelectedStone->GetEntityHandle() : entt::null));
		else if (pkParty)
		{
			pkParty->Join(tch->GetLegacyVID());
			pkParty->Link(((tch) ? (tch)->GetEntityHandle() : entt::null));
		}
		else if (!pkChrMaster)
		{
			pkChrMaster = tch;
			pkChrMaster->SetRegen(pkRegen);

			pkParty = CPartyManager::instance().CreateParty(((pkChrMaster) ? (pkChrMaster)->GetEntityHandle() : entt::null));
		}

		if (bAggressive)
			tch->SetAggressive();
	}

	return chLeader;
}

struct FuncUpdateAndResetChatCounter
{
	void operator () (LPCHARACTER ch)
	{
		ch->ResetChatCounter();
		ch->CFSM::Update();
	}
};

void CHARACTER_MANAGER::Update(int iPulse)
{
	using namespace std;
	//#ifdef __GNUC__
	//	using namespace __gnu_cxx;
	//#endif

	BeginPendingDestroy();
#ifdef ENABLE_EVENT_MANAGER

	const TEventManagerData* ev = CheckEventIsActive(DUNGEON_TICKET_LOOT_EVENT, 0);
	const bool nowActive = (ev != nullptr);

	int nowExtraCount = 0;
	if (ev)
		nowExtraCount = (int)ev->value[0];

	if (nowExtraCount < 0) nowExtraCount = 0;
	if (nowExtraCount > 30) nowExtraCount = 30;

	const bool needResync =
		(nowActive != g_bPrevDungeonTicketActive) ||
		(nowActive && nowExtraCount != g_iPrevDungeonTicketExtraCount);

	if (needResync)
	{

		CHARACTER_VECTOR all;
		all.reserve(m_map_pkChrByVID.size());
		for (const auto& it : m_map_pkChrByVID)
			if (it.second)
				all.push_back(it.second);

		for (LPCHARACTER ch : all)
		{
			if (!ch) continue;
			if (ecs::PlayerRuntime::IsStone(ch->GetEntityHandle()) && ch->IsDungeonTicketExtraMetin())
				DestroyCharacter(ch);
		}


		if (nowActive && nowExtraCount > 0)
		{

			all.clear();
			all.reserve(m_map_pkChrByVID.size());
			for (const auto& it : m_map_pkChrByVID)
				if (it.second)
					all.push_back(it.second);

			for (LPCHARACTER ch : all)
			{
				if (!ch) continue;
				const entt::entity character = ch->GetEntityHandle();

				if (!ecs::PlayerRuntime::IsStone(character)) continue;
				if (ch->IsDungeonTicketExtraMetin()) continue;

				const int32_t mapIndex = ecs::PlayerRuntime::GetMapIndex(character);

				const bool isDungeonMap =
					(mapIndex >= 10000) ||
					(CDungeonManager::instance().FindByMapIndex(mapIndex) != nullptr);

				if (isDungeonMap)
					continue;

				const uint32_t vnum = ecs::PlayerRuntime::GetRaceNum(character);
				const int32_t x = ecs::PlayerRuntime::GetX(character);
				const int32_t y = ecs::PlayerRuntime::GetY(character);
				const int32_t z = ch->GetZ();

				g_bDungeonTicketExtraMetinSpawn = true;

				for (int i = 0; i < nowExtraCount; ++i)
				{
					int sx, sy;
					CalcDungeonTicketExtraPos(x, y, i, sx, sy);
					SpawnMob(vnum, mapIndex, sx, sy, z, true, -1, true);
				}


				g_bDungeonTicketExtraMetinSpawn = false;
			}
		}

		g_bPrevDungeonTicketActive = nowActive;
		g_iPrevDungeonTicketExtraCount = nowExtraCount;
	}
	// Map1 mass spawn events (Tanaka / Golden Frog)
	Map1MassSpawnEvents_Update(*this, iPulse);
#endif

	// PC ĳ Ʈ
	{
		if (!m_map_pkPCChr.empty())
		{
			// ̳
			CHARACTER_VECTOR v;
			v.reserve(m_map_pkPCChr.size());
			//#ifdef __GNUC__
			//			transform(m_map_pkPCChr.begin(), m_map_pkPCChr.end(), back_inserter(v), select2nd<NAME_MAP::value_type>());
			//#else
			transform(m_map_pkPCChr.begin(), m_map_pkPCChr.end(), back_inserter(v), std::bind(&NAME_MAP::value_type::second, std::placeholders::_1));
			//#endif

			if (0 == iPulse % PASSES_PER_SEC(5))
			{
				FuncUpdateAndResetChatCounter f;
				for_each(v.begin(), v.end(), f);
			}
			else
			{
				//for_each(v.begin(), v.end(), mem_fun(&CFSM::Update));
				for_each(v.begin(), v.end(), bind(&CHARACTER::UpdateCharacter, std::placeholders::_1, iPulse));
			}
		}
		//#ifdef ENABLE_FAKE_SHOP_HEADER
		//
		//		if (0 == (iPulse % PASSES_PER_SEC(5)))
		//		{
		//			for (const auto& it : m_map_pkPCChr)
		//			{
		//				if (LPCHARACTER ch = it.second)
		//					ch->UpdateMountCountOverhead(ch);
		//			}
		//		}
		//#endif
				//		for_each_pc(bind2nd(mem_fun(&CHARACTER::UpdateCharacter), iPulse));
	}

	//  Ʈ
	{
		if (!m_set_pkChrState.empty())
		{
			CHARACTER_VECTOR v;
			v.reserve(m_set_pkChrState.size());
			//#ifdef __GNUC__
			//			transform(m_set_pkChrState.begin(), m_set_pkChrState.end(), back_inserter(v), identity<CHARACTER_SET::value_type>());
			//#else
			v.insert(v.end(), m_set_pkChrState.begin(), m_set_pkChrState.end());
			//#endif
			for_each(v.begin(), v.end(), bind(&CHARACTER::UpdateStateMachine, std::placeholders::_1, iPulse));
		}
	}

	// 1ð ѹ
	if (0 == iPulse % PASSES_PER_SEC(3600))
	{
		for (auto it = m_map_dwMobKillCount.begin(); it != m_map_dwMobKillCount.end(); ++it)
			DBManager::instance().SendMoneyLog(MONEY_LOG_MONSTER_KILL, it->first, it->second);

		m_map_dwMobKillCount.clear();
	}

	// ׽Ʈ  60ʸ ĳ
	if (test_server && 0 == iPulse % PASSES_PER_SEC(60))
		LOG_INFO("CHARACTER COUNT vid {} pid {}", m_map_pkChrByVID.size(), m_map_pkChrByPID.size());

	//  DestroyCharacter ϱ
	FlushPendingDestroy();

	// ShutdownManager Update
	CShutdownManager::Instance().Update();
}

void CHARACTER_MANAGER::ProcessDelayedSave()
{
	auto it = m_set_pkChrForDelayedSave.begin();
	while (it != m_set_pkChrForDelayedSave.end())
	{
		LPCHARACTER pkChr = *it++;
		pkChr->SaveReal();
	}

	m_set_pkChrForDelayedSave.clear();
}

bool CHARACTER_MANAGER::AddToStateList(LPCHARACTER ch)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "char_manager.cpp::bool CHARACTER_MANAGER::AddToStateList");//INGAME_DEBUG_RAZOR93
#endif
	assert(ch != NULL);

	if (const auto it = m_set_pkChrState.find(ch); it == m_set_pkChrState.end())
	{
		m_set_pkChrState.insert(ch);
		return true;
	}

	return false;
}

void CHARACTER_MANAGER::RemoveFromStateList(LPCHARACTER ch)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "char_manager.cpp::void CHARACTER_MANAGER::RemoveFromStateList");//INGAME_DEBUG_RAZOR93
#endif
	if (const auto it = m_set_pkChrState.find(ch); it != m_set_pkChrState.end())
	{
		//0, "RemoveFromStateList %p", ch);
		m_set_pkChrState.erase(it);
	}
}

void CHARACTER_MANAGER::DelayedSave(LPCHARACTER ch) {
	//#ifdef ENABLE_INGAME_DEBUG_RAZOR93d
	//	ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "char_manager.cpp::void CHARACTER_MANAGER::DelayedSave");//INGAME_DEBUG_RAZOR93
	//#endif
		//////////FIX m_set_pkChrForDelayedSave.insert(ch);
	if (const auto it = m_set_pkChrForDelayedSave.find(ch); it != m_set_pkChrForDelayedSave.end()) {
		m_set_pkChrForDelayedSave.erase(it);
	}

	m_set_pkChrForDelayedSave.insert(ch);
}

bool CHARACTER_MANAGER::FlushDelayedSave(LPCHARACTER ch)
{
	//#ifdef ENABLE_INGAME_DEBUG_RAZOR93d
	//	ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::FlushDelayedSave");//INGAME_DEBUG_RAZOR93
	//#endif
	const auto it = m_set_pkChrForDelayedSave.find(ch);

	if (it == m_set_pkChrForDelayedSave.end())
		return false;

	m_set_pkChrForDelayedSave.erase(it);
	ch->SaveReal();
	return true;
}

void CHARACTER_MANAGER::RegisterForMonsterLog(LPCHARACTER ch)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::RegisterForMonsterLog");//INGAME_DEBUG_RAZOR93
#endif
	m_set_pkChrMonsterLog.insert(ch);
}

void CHARACTER_MANAGER::UnregisterForMonsterLog(LPCHARACTER ch)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::UnregisterForMonsterLog");//INGAME_DEBUG_RAZOR93
#endif
	m_set_pkChrMonsterLog.erase(ch);
}

void CHARACTER_MANAGER::PacketMonsterLog(entt::entity character, const void* buf, int size)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::PacketMonsterLog");//INGAME_DEBUG_RAZOR93
#endif
	for (auto it = m_set_pkChrMonsterLog.begin(); it != m_set_pkChrMonsterLog.end(); ++it)
	{
		LPCHARACTER c = *it;
		const entt::entity cEntity = c ? c->GetEntityHandle() : entt::null;


		if (ch && DISTANCE_APPROX(ecs::PlayerRuntime::GetX(cEntity) - ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(cEntity) - ecs::PlayerRuntime::GetY(character)) > 6000)
			continue;

		LPDESC d = ecs::PlayerRuntime::GetDesc(cEntity);

		if (d)
			d->Packet(buf, size);
	}
}

void CHARACTER_MANAGER::KillLog(uint32_t dwVnum)
{
	constexpr uint32_t SEND_LIMIT = 10000;

	if (const auto it = m_map_dwMobKillCount.find(dwVnum); it == m_map_dwMobKillCount.end())
		m_map_dwMobKillCount.insert(std::make_pair(dwVnum, 1));
	else
	{
		++it->second;

		if (it->second > SEND_LIMIT)
		{
			DBManager::instance().SendMoneyLog(MONEY_LOG_MONSTER_KILL, it->first, it->second);
			m_map_dwMobKillCount.erase(it);
		}
	}
}

void CHARACTER_MANAGER::RegisterRaceNum(uint32_t dwVnum)
{
	m_set_dwRegisteredRaceNum.insert(dwVnum);
}

void CHARACTER_MANAGER::RegisterRaceNumMap(LPCHARACTER ch)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(ch ? ch->GetEntityHandle() : entt::null, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::RegisterRaceNumMap");//INGAME_DEBUG_RAZOR93
#endif
	const auto entity = ch ? ch->GetEntityHandle() : entt::null;
	uint32_t dwVnum = ecs::PlayerRuntime::GetRaceNum(entity);

	if (m_set_dwRegisteredRaceNum.contains(dwVnum)) // ϵ ȣ ̸
	{
		LOG_INFO("RegisterRaceNumMap {} {}", ecs::PlayerRuntime::GetName(entity), dwVnum);
		m_map_pkChrByRaceNum[dwVnum].insert(ch);
	}
}

void CHARACTER_MANAGER::UnregisterRaceNumMap(LPCHARACTER ch)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(ch ? ch->GetEntityHandle() : entt::null, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::UnregisterRaceNumMap");//INGAME_DEBUG_RAZOR93
#endif
	const entt::entity character = ch ? ch->GetEntityHandle() : entt::null;
	uint32_t dwVnum = ecs::PlayerRuntime::GetRaceNum(character);

	if (const auto it = m_map_pkChrByRaceNum.find(dwVnum); it != m_map_pkChrByRaceNum.end())
		it->second.erase(ch);
}

bool CHARACTER_MANAGER::GetCharactersByRaceNum(uint32_t dwRaceNum, CharacterVectorInteractor& i)
{
	const auto it = m_map_pkChrByRaceNum.find(dwRaceNum);

	if (it == m_map_pkChrByRaceNum.end())
		return false;

	i = it->second;
	return true;
}

#define FIND_JOB_WARRIOR_0	(1 << 3)
#define FIND_JOB_WARRIOR_1	(1 << 4)
#define FIND_JOB_WARRIOR_2	(1 << 5)
#define FIND_JOB_WARRIOR	(FIND_JOB_WARRIOR_0 | FIND_JOB_WARRIOR_1 | FIND_JOB_WARRIOR_2)
#define FIND_JOB_ASSASSIN_0	(1 << 6)
#define FIND_JOB_ASSASSIN_1	(1 << 7)
#define FIND_JOB_ASSASSIN_2	(1 << 8)
#define FIND_JOB_ASSASSIN	(FIND_JOB_ASSASSIN_0 | FIND_JOB_ASSASSIN_1 | FIND_JOB_ASSASSIN_2)
#define FIND_JOB_SURA_0		(1 << 9)
#define FIND_JOB_SURA_1		(1 << 10)
#define FIND_JOB_SURA_2		(1 << 11)
#define FIND_JOB_SURA		(FIND_JOB_SURA_0 | FIND_JOB_SURA_1 | FIND_JOB_SURA_2)
#define FIND_JOB_SHAMAN_0	(1 << 12)
#define FIND_JOB_SHAMAN_1	(1 << 13)
#define FIND_JOB_SHAMAN_2	(1 << 14)
#define FIND_JOB_SHAMAN		(FIND_JOB_SHAMAN_0 | FIND_JOB_SHAMAN_1 | FIND_JOB_SHAMAN_2)
#ifdef ENABLE_WOLFMAN_CHARACTER
#define FIND_JOB_WOLFMAN_0	(1 << 15)
#define FIND_JOB_WOLFMAN_1	(1 << 16)
#define FIND_JOB_WOLFMAN_2	(1 << 17)
#define FIND_JOB_WOLFMAN		(FIND_JOB_WOLFMAN_0 | FIND_JOB_WOLFMAN_1 | FIND_JOB_WOLFMAN_2)
#endif

//
// (job+1)*3+(skill_group)
//
LPCHARACTER CHARACTER_MANAGER::FindSpecifyPC(unsigned int uiJobFlag, int32_t lMapIndex, LPCHARACTER except, int iMinLevel, int iMaxLevel)
{
	LPCHARACTER chFind = nullptr;
	int n = 0;

	for (auto it = m_map_pkChrByPID.begin(); it != m_map_pkChrByPID.end(); ++it)
	{
		auto* ch = it->second;

		if (ch == except)
			continue;

		const entt::entity character = ch->GetEntityHandle();
		const int32_t level = ecs::PointSystem::GetLevel(character);

		if (level < iMinLevel)
			continue;

		if (level > iMaxLevel)
			continue;

		if (ecs::PlayerRuntime::GetMapIndex(character) != lMapIndex)
			continue;

		if (uiJobFlag)
		{
			unsigned int uiChrJob = 1 << ((ch->GetJob() + 1) * 3 + ch->GetSkillGroup());

			if (!IS_SET(uiJobFlag, uiChrJob))
				continue;
		}

		if (!chFind || number(1, ++n) == 1)
			chFind = ch;
	}

	return chFind;
}

int CHARACTER_MANAGER::GetMobItemRate(entt::entity character)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::GetMobItemRate");
#endif
	if (ecs::PlayerRuntime::GetPremiumRemainSeconds(character, PREMIUM_ITEM) > 0)
		return m_iMobItemRatePremium;
	return m_iMobItemRate;
}

int CHARACTER_MANAGER::GetMobDamageRate(entt::entity character)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::GetMobDamageRate");//INGAME_DEBUG_RAZOR93
#endif
	return m_iMobDamageRate;
}

int CHARACTER_MANAGER::GetMobGoldAmountRate(entt::entity character)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::GetMobGoldAmountRate");//INGAME_DEBUG_RAZOR93
#endif
	if (ecs::PlayerRuntime::GetPremiumRemainSeconds(character, PREMIUM_GOLD) > 0)
		return m_iMobGoldAmountRatePremium;

	return m_iMobGoldAmountRate;
}

int CHARACTER_MANAGER::GetMobGoldDropRate(entt::entity character)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::GetMobGoldDropRate");//INGAME_DEBUG_RAZOR93
#endif
	if (ecs::PlayerRuntime::GetPremiumRemainSeconds(character, PREMIUM_GOLD) > 0)
		return m_iMobGoldDropRatePremium;

	return m_iMobGoldDropRate;
}

int CHARACTER_MANAGER::GetMobExpRate(entt::entity character)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::GetMobExpRate");//INGAME_DEBUG_RAZOR93
#endif
	if (ecs::PlayerRuntime::GetPremiumRemainSeconds(character, PREMIUM_EXP) > 0)
		return m_iMobExpRatePremium;

	return m_iMobExpRate;
}

int CHARACTER_MANAGER::GetUserDamageRate(entt::entity character)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::GetUserDamageRate");//INGAME_DEBUG_RAZOR93
#endif
	if (ecs::PlayerRuntime::GetPremiumRemainSeconds(character, PREMIUM_EXP) > 0)
		return m_iUserDamageRatePremium;

	return m_iUserDamageRate;
}

void CHARACTER_MANAGER::SendScriptToMap(int32_t lMapIndex, std::string_view s)
{
	LPSECTREE_MAP pSecMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);

	if (nullptr == pSecMap)
		return;

	packet_script p;

	p.header = HEADER_GC_SCRIPT;
	p.skin = 1;
	p.src_size = s.size();

	quest::FSendPacket f;
	p.size = p.src_size + sizeof(packet_script);
	f.buf.write(&p, sizeof(packet_script));
	f.buf.write(s.data(), s.size());

	pSecMap->for_each(f);
}

bool CHARACTER_MANAGER::BeginPendingDestroy()
{
	// Begin  Ŀ Begin  ϴ 쿡 Flush  ʴ
	// ̹ ۵Ǿ false  ó
	if (m_bUsePendingDestroy)
		return false;

	m_bUsePendingDestroy = true;
	return true;
}

void CHARACTER_MANAGER::FlushPendingDestroy()
{
	using namespace std;

	m_bUsePendingDestroy = false; // ÷׸  ؾ  Destroy ó

	if (!m_set_pkChrPendingDestroy.empty())
	{
		LOG_INFO("FlushPendingDestroy size {}", m_set_pkChrPendingDestroy.size());

		auto it = m_set_pkChrPendingDestroy.begin();
		for (const auto end = m_set_pkChrPendingDestroy.end(); it != end; ++it) {
			M2_DESTROY_CHARACTER(*it);
		}

		m_set_pkChrPendingDestroy.clear();
	}
}

CharacterVectorInteractor::CharacterVectorInteractor(const CHARACTER_SET& r)
{
	reserve(r.size());
	insert(end(), r.begin(), r.end());

	if (CHARACTER_MANAGER::instance().BeginPendingDestroy())
		m_bMyBegin = true;
}

CharacterVectorInteractor::~CharacterVectorInteractor()
{
	if (m_bMyBegin)
		CHARACTER_MANAGER::instance().FlushPendingDestroy();
}

#ifdef ENABLE_EVENT_MANAGER
#include "item_manager.h"
#include "item.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char.h"
void CHARACTER_MANAGER::ClearEventData()
{
	m_eventData.clear();
}
void CHARACTER_MANAGER::CheckBonusEvent(entt::entity character)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	//#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	//	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::CheckBonusEvent");//INGAME_DEBUG_RAZOR93
	//#endif
	const TEventManagerData* eventPtr = CheckEventIsActive(BONUS_EVENT, ecs::PlayerRuntime::GetEmpire(character));
	if (eventPtr)
		ch->ApplyPoint(eventPtr->value[0], eventPtr->value[1]);
}
const TEventManagerData* CHARACTER_MANAGER::CheckEventIsActive(uint8_t eventIndex, uint8_t empireIndex)
{
	const time_t cur_Time = time(nullptr);
	const struct tm vKey = *localtime(&cur_Time);

	for (const auto& [dayIndex, dayVector] : m_eventData)
	{
		for (const auto& eventData : dayVector)
		{
			if (eventData.eventIndex == eventIndex)
			{
				if (eventData.channelFlag != 0)
					if (eventData.channelFlag != g_bChannel)
						continue;
				if (eventData.empireFlag != 0 && empireIndex != 0)
					if (eventData.empireFlag != empireIndex)
						continue;

				if (eventData.eventStatus == true)
					return &eventData;
				//if (cur_Time >= eventData.startTime && cur_Time <= eventData.endTime)
				//	return &eventData;
			}
		}
	}
	return nullptr;
}
void CHARACTER_MANAGER::CheckEventForDrop(entt::entity character, entt::entity killer, std::vector<entt::entity>& vec_item)
{
	LPCHARACTER pkChr = ecs::LegacyCharOf(character);
	LPCHARACTER pkKiller = ecs::LegacyCharOf(killer);
	const uint8_t killerEmpire = ecs::PlayerRuntime::GetEmpire(killer);
	const TEventManagerData* eventPtr = nullptr;
	LPITEM rewardItem = nullptr;

	if (ecs::PlayerRuntime::IsStone(character))
	{
		eventPtr = CheckEventIsActive(DOUBLE_METIN_LOOT_EVENT, killerEmpire);
		if (eventPtr && RollEventChance(eventPtr->value[3]))
		{


			std::vector<entt::entity> m_cache;
			for (const auto& vItem : vec_item)
			{
				rewardItem = ITEM_MANAGER::Instance().CreateItem(ItemSystem::GetItemVnum(vItem), ItemSystem::GetItemCount(vItem), 0, true);
				if (rewardItem) m_cache.emplace_back((rewardItem ? rewardItem->GetEntityHandle() : entt::null));
			}
			for (const auto& rItem : m_cache)
				vec_item.emplace_back(rItem);

		}
	}
	else if (ecs::PlayerRuntime::GetRaceNum(character) == 693//Szellem orkvezr
		|| ecs::PlayerRuntime::GetRaceNum(character) == 2832//Shen tbornok
		|| ecs::PlayerRuntime::GetRaceNum(character) == 6191//Nemere
		|| ecs::PlayerRuntime::GetRaceNum(character) == 768//slime
		|| ecs::PlayerRuntime::GetRaceNum(character) == 1093//Kaszs
		|| ecs::PlayerRuntime::GetRaceNum(character) == 3491//Triton
		|| ecs::PlayerRuntime::GetRaceNum(character) == 2493//Beran-Setaou
		|| ecs::PlayerRuntime::GetRaceNum(character) == 4158//Szfinksz
		|| ecs::PlayerRuntime::GetRaceNum(character) == 4203//Agares
		|| ecs::PlayerRuntime::GetRaceNum(character) == 2598//Irn Azrael
		|| ecs::PlayerRuntime::GetRaceNum(character) == 719//Ru-Taig
		|| ecs::PlayerRuntime::GetRaceNum(character) == 6393//Ers Ochao fejedelem
		|| ecs::PlayerRuntime::GetRaceNum(character) == 6191//Nemere
		|| ecs::PlayerRuntime::GetRaceNum(character) == 6091//Razador
		|| ecs::PlayerRuntime::GetRaceNum(character) == 2092//Pk-brn
		|| ecs::PlayerRuntime::GetRaceNum(character) == 4815//	Fagyvész zsarnok óriás
		|| ecs::PlayerRuntime::GetRaceNum(character) == 4584//Fandalia nover
		|| ecs::PlayerRuntime::GetRaceNum(character) == 4011//Eien (BOSS)
		|| ecs::PlayerRuntime::GetRaceNum(character) == 3910//Skeletos



		)
	{
		eventPtr = CheckEventIsActive(BUPLA_RUN_BOSS_LOOT_EVENT, killerEmpire);
		if (eventPtr && RollEventChance(eventPtr->value[3]))
		{
			std::vector<entt::entity> m_cache;
			for (const auto& vItem : vec_item)
			{
				rewardItem = ITEM_MANAGER::Instance().CreateItem(ItemSystem::GetItemVnum(vItem), ItemSystem::GetItemCount(vItem), 0, true);
				if (rewardItem)
					m_cache.emplace_back((rewardItem ? rewardItem->GetEntityHandle() : entt::null));
			}

			for (const auto& rItem : m_cache)
				vec_item.emplace_back(rItem);

		}
		if (ecs::PlayerRuntime::GetRaceNum(character) == 4011)
		{
			LPITEM extraDrop = ITEM_MANAGER::Instance().CreateItem(50101, 1, 0, true);
			if (extraDrop)
				vec_item.emplace_back((extraDrop ? extraDrop->GetEntityHandle() : entt::null));
		}
	}
	else if (ecs::PlayerRuntime::GetRaceNum(character) == 491//map1
		|| ecs::PlayerRuntime::GetRaceNum(character) == 492//map1
		|| ecs::PlayerRuntime::GetRaceNum(character) == 493//map1
		|| ecs::PlayerRuntime::GetRaceNum(character) == 494//map1
		|| ecs::PlayerRuntime::GetRaceNum(character) == 691//orkvezr
		|| ecs::PlayerRuntime::GetRaceNum(character) == 2491//Yonghan Parancsnok
		|| ecs::PlayerRuntime::GetRaceNum(character) == 3590//Arccsont
		|| ecs::PlayerRuntime::GetRaceNum(character) == 3490//Kappa tbornok
		|| ecs::PlayerRuntime::GetRaceNum(character) == 3690//Homr tbornok
		|| ecs::PlayerRuntime::GetRaceNum(character) == 3691//Tarisznyark kirly
		|| ecs::PlayerRuntime::GetRaceNum(character) == 3791//Wubba kirly
		|| ecs::PlayerRuntime::GetRaceNum(character) == 3591//Vrs fnk
		|| ecs::PlayerRuntime::GetRaceNum(character) == 3595//Brutlis Arccsont
		|| ecs::PlayerRuntime::GetRaceNum(character) == 4157//Anubis
		|| ecs::PlayerRuntime::GetRaceNum(character) == 4155//Bastet
		|| ecs::PlayerRuntime::GetRaceNum(character) == 4156//Ra
		|| ecs::PlayerRuntime::GetRaceNum(character) == 2597//Charon
		|| ecs::PlayerRuntime::GetRaceNum(character) == 5001//Szellem I
		|| ecs::PlayerRuntime::GetRaceNum(character) == 6407//En-Tai uralkod
		|| ecs::PlayerRuntime::GetRaceNum(character) == 6408//Bagjanamu
		|| ecs::PlayerRuntime::GetRaceNum(character) == 2206//Lngkirly
		|| ecs::PlayerRuntime::GetRaceNum(character) == 2094//Pk-br
		)
	{
		eventPtr = CheckEventIsActive(DOUBLE_BOSS_LOOT_EVENT, killerEmpire);
		if (eventPtr && RollEventChance(eventPtr->value[3]))
		{

			std::vector<entt::entity> m_cache;
			for (const auto& vItem : vec_item)
			{
				rewardItem = ITEM_MANAGER::Instance().CreateItem(ItemSystem::GetItemVnum(vItem), ItemSystem::GetItemCount(vItem), 0, true);
				if (rewardItem)
					m_cache.emplace_back((rewardItem ? rewardItem->GetEntityHandle() : entt::null));
			}

			for (const auto& rItem : m_cache)
				vec_item.emplace_back(rItem);

		}
	}

	eventPtr = CheckEventIsActive(DOUBLE_MISSION_BOOK_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{

		// If you have different book index put here!
		constexpr uint32_t m_lbookItems[] = { 50300, 50301, 50302 };
		std::vector<entt::entity> m_cache;
		for (const auto& vItem : vec_item)
		{
			const uint32_t itemVnum = ItemSystem::GetItemVnum(vItem);
			for (const auto& missionBook : m_lbookItems)
			{
				if (missionBook == itemVnum)
				{
					rewardItem = ITEM_MANAGER::Instance().CreateItem(itemVnum, ItemSystem::GetItemCount(vItem), 0, true);
					if (rewardItem) m_cache.emplace_back((rewardItem ? rewardItem->GetEntityHandle() : entt::null));

					break;
				}
			}
			for (const auto& rItem : m_cache)
				vec_item.emplace_back(rItem);
		}
	}

	eventPtr = CheckEventIsActive(DUNGEON_TICKET_LOOT_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{
		// If you have different book index put here!
		constexpr uint32_t m_lticketItems[] = { 71201 };
		std::vector<entt::entity> m_cache;
		for (const auto& vItem : vec_item)
		{
			const uint32_t itemVnum = ItemSystem::GetItemVnum(vItem);
			for (const auto& ticketItem : m_lticketItems)
			{
				if (ticketItem == itemVnum)
				{
					rewardItem = ITEM_MANAGER::Instance().CreateItem(itemVnum, ItemSystem::GetItemCount(vItem), 0, true);
					if (rewardItem) m_cache.emplace_back((rewardItem ? rewardItem->GetEntityHandle() : entt::null));

					break;
				}
			}
			for (const auto& rItem : m_cache)
				vec_item.emplace_back(rItem);
		}
	}
	eventPtr = CheckEventIsActive(MOONLIGHT_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{

		// If your moonlight item vnum is different change 50011!
		LPITEM item = ITEM_MANAGER::Instance().CreateItem(50011, 1, 0, true);
		if (item) vec_item.emplace_back((item ? item->GetEntityHandle() : entt::null));

	}
	eventPtr = CheckEventIsActive(LELEKGOMB_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{

		// If your moonlight item vnum is different change 50011!
		LPITEM item = ITEM_MANAGER::Instance().CreateItem(30135, 1, 0, true);
		if (item) vec_item.emplace_back((item ? item->GetEntityHandle() : entt::null));

	}

	eventPtr = CheckEventIsActive(HATSZOG_LADA_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{

		// If your moonlight item vnum is different change 50011!
		LPITEM item = ITEM_MANAGER::Instance().CreateItem(50037, 1, 0, true);
		if (item) vec_item.emplace_back((item ? item->GetEntityHandle() : entt::null));

	}
	eventPtr = CheckEventIsActive(MIKI_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{

		// If your moonlight item vnum is different change 50011!
		LPITEM item = ITEM_MANAGER::Instance().CreateItem(50010, 1, 0, true);
		if (item) vec_item.emplace_back((item ? item->GetEntityHandle() : entt::null));

	}

	eventPtr = CheckEventIsActive(VALENTIN_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{

		const uint32_t dwBossVnum = eventPtr->value[0];

		if (dwBossVnum && pkChr && ecs::PlayerRuntime::IsStone(character) && pkKiller && ecs::PlayerRuntime::IsPC(killer))
		{
			const int32_t mapIndex = ecs::PlayerRuntime::GetMapIndex(character);
			const int32_t baseX = ecs::PlayerRuntime::GetX(character);
			const int32_t baseY = ecs::PlayerRuntime::GetY(character);
			const int32_t baseZ = pkChr->GetZ();

			LPCHARACTER boss = nullptr;


			for (int i = 0; i < 16 && !boss; ++i)
			{
				PIXEL_POSITION p;
				p.x = baseX + number(-400, 400);
				p.y = baseY + number(-400, 400);

				if (!SECTREE_MANAGER::instance().GetMovablePosition(mapIndex, p.x, p.y, p))
					continue;

				boss = CHARACTER_MANAGER::instance().SpawnMob(dwBossVnum, mapIndex, p.x, p.y, baseZ, true, -1, true);
			}

			if (boss)
			{
				boss->SetAggressive();
				boss->SetVictim(killer);
			}
		}





	}

	eventPtr = CheckEventIsActive(EASTER_EVENT, killerEmpire);
	if (eventPtr)
	{
		if (RollEventChance(eventPtr->value[3]))
		{
			const uint32_t dwBossVnum = eventPtr->value[0];

			if (dwBossVnum && pkChr && ecs::PlayerRuntime::IsStone(character) && pkKiller && ecs::PlayerRuntime::IsPC(killer))
			{
				const int32_t mapIndex = ecs::PlayerRuntime::GetMapIndex(character);
				const int32_t baseX = ecs::PlayerRuntime::GetX(character);
				const int32_t baseY = ecs::PlayerRuntime::GetY(character);
				const int32_t baseZ = pkChr->GetZ();

				LPCHARACTER boss = nullptr;

				for (int i = 0; i < 16 && !boss; ++i)
				{
					PIXEL_POSITION p;
					p.x = baseX + number(-400, 400);
					p.y = baseY + number(-400, 400);

					if (!SECTREE_MANAGER::instance().GetMovablePosition(mapIndex, p.x, p.y, p))
						continue;

					boss = CHARACTER_MANAGER::instance().SpawnMob(dwBossVnum, mapIndex, p.x, p.y, baseZ, true, -1, true);
				}

				if (boss)
				{
					boss->SetAggressive();
					boss->SetVictim(killer);
				}
			}
		}

		if (RollEventChance(eventPtr->value[2]))
		{
			LPITEM item = ITEM_MANAGER::instance().CreateItem(50181, 1, 0, true);//egy néger kosár fasz
			if (item)
				vec_item.emplace_back((item ? item->GetEntityHandle() : entt::null));
		}
	}
	eventPtr = CheckEventIsActive(KARI_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{
		//CSAK FLAG A NAPTARBA
	}
	eventPtr = CheckEventIsActive(COIN_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{

		// If your moonlight item vnum is different change 50011!
		LPITEM item = ITEM_MANAGER::Instance().CreateItem(39068, 1, 0, true);
		if (item) vec_item.emplace_back((item ? item->GetEntityHandle() : entt::null));

	}
	eventPtr = CheckEventIsActive(DUPLA_BOSS_PONT_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{
		if (
			ecs::PlayerRuntime::GetRaceNum(character) == 491 || // map1
			ecs::PlayerRuntime::GetRaceNum(character) == 492 || // map1
			ecs::PlayerRuntime::GetRaceNum(character) == 493 || // map1
			ecs::PlayerRuntime::GetRaceNum(character) == 494 || // map1
			ecs::PlayerRuntime::GetRaceNum(character) == 691 || // Orkvezr
			ecs::PlayerRuntime::GetRaceNum(character) == 2491 || // Yonghan Parancsnok
			ecs::PlayerRuntime::GetRaceNum(character) == 3590 || // Arccsont
			ecs::PlayerRuntime::GetRaceNum(character) == 3490 || // Kappa tbornok
			ecs::PlayerRuntime::GetRaceNum(character) == 3690 || // Homr tbornok
			ecs::PlayerRuntime::GetRaceNum(character) == 3691 || // Tarisznyark kirly
			ecs::PlayerRuntime::GetRaceNum(character) == 3791 || // Wubba kirly
			ecs::PlayerRuntime::GetRaceNum(character) == 3591 || // Vrs fnk
			ecs::PlayerRuntime::GetRaceNum(character) == 3595 || // Brutlis Arccsont
			ecs::PlayerRuntime::GetRaceNum(character) == 4157 || // Anubis
			ecs::PlayerRuntime::GetRaceNum(character) == 4155 || // Bastet
			ecs::PlayerRuntime::GetRaceNum(character) == 4156 || // Ra
			ecs::PlayerRuntime::GetRaceNum(character) == 2597 || // Charon
			ecs::PlayerRuntime::GetRaceNum(character) == 5001 || // Szellem I
			ecs::PlayerRuntime::GetRaceNum(character) == 6407 || // En-Tai uralkod
			ecs::PlayerRuntime::GetRaceNum(character) == 6408 || // Bagjanamu
			ecs::PlayerRuntime::GetRaceNum(character) == 2206 || // Lngkirly
			ecs::PlayerRuntime::GetRaceNum(character) == 2094    // Pk-br
			)
		{
			// === DROP ===
			LPITEM item = ITEM_MANAGER::Instance().CreateItem(99998, 1, 0, true);
			if (item)
				vec_item.emplace_back((item ? item->GetEntityHandle() : entt::null));
		}
	}
	eventPtr = CheckEventIsActive(DUPLA_RUN_PONT_EVENT, killerEmpire);
	if (eventPtr && RollEventChance(eventPtr->value[3]))
	{
		if (ecs::PlayerRuntime::GetRaceNum(character) == 693//Szellem orkvezr
			|| ecs::PlayerRuntime::GetRaceNum(character) == 2832//Shen tbornok
			|| ecs::PlayerRuntime::GetRaceNum(character) == 4584//Blood run
			|| ecs::PlayerRuntime::GetRaceNum(character) == 6191//Nemere
			|| ecs::PlayerRuntime::GetRaceNum(character) == 768//slime
			|| ecs::PlayerRuntime::GetRaceNum(character) == 1093//Kaszs
			|| ecs::PlayerRuntime::GetRaceNum(character) == 3491//Triton
			|| ecs::PlayerRuntime::GetRaceNum(character) == 2493//Beran-Setaou
			|| ecs::PlayerRuntime::GetRaceNum(character) == 4158//Szfinksz
			|| ecs::PlayerRuntime::GetRaceNum(character) == 4203//Agares
			|| ecs::PlayerRuntime::GetRaceNum(character) == 2598//Irn Azrael
			|| ecs::PlayerRuntime::GetRaceNum(character) == 719//Ru-Taig
			|| ecs::PlayerRuntime::GetRaceNum(character) == 6393//Er s Ochao fejedelem
			|| ecs::PlayerRuntime::GetRaceNum(character) == 6191//Nemere
			|| ecs::PlayerRuntime::GetRaceNum(character) == 6091//Razador
			|| ecs::PlayerRuntime::GetRaceNum(character) == 2092//Pk-b
			|| ecs::PlayerRuntime::GetRaceNum(character) == 4011//Eien (BOSS
			|| ecs::PlayerRuntime::GetRaceNum(character) == 6192//	Jotun Thrym/
			|| ecs::PlayerRuntime::GetRaceNum(character) == 3910//Skeletos
			)
		{
			// === DROP ===
			LPITEM item = ITEM_MANAGER::Instance().CreateItem(99999, 1, 0, true);
			if (item)
				vec_item.emplace_back((item ? item->GetEntityHandle() : entt::null));
		}
	}
	if (ecs::PlayerRuntime::IsStone(character))
	{
		eventPtr = CheckEventIsActive(DUPLA_SZILI_EVENT, killerEmpire);
		if (eventPtr && RollEventChance(eventPtr->value[3]))
		{
			std::vector<entt::entity> m_cache;

			for (const auto& vItem : vec_item)
			{

				if (ItemSystem::GetItemVnum(vItem) == 30271)
				{
					rewardItem = ITEM_MANAGER::Instance().CreateItem(30271, ItemSystem::GetItemCount(vItem), 0, true);
					if (rewardItem)
						m_cache.emplace_back((rewardItem ? rewardItem->GetEntityHandle() : entt::null));
				}
			}


			for (const auto& rItem : m_cache)
				vec_item.emplace_back(rItem);
		}
	}

}
void CHARACTER_MANAGER::CompareEventSendData(TEMP_BUFFER* buf)
{
	const uint8_t dayCount = m_eventData.size();
	const uint8_t subIndex = EVENT_MANAGER_LOAD;
	const int cur_Time = time(nullptr);
	TPacketGCEventManager p;
	p.header = HEADER_GC_EVENT_MANAGER;
	p.size = sizeof(TPacketGCEventManager) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(int);
	for (const auto& [dayIndex, dayData] : m_eventData)
	{
		const uint8_t dayEventCount = dayData.size();
		p.size += sizeof(uint8_t) + sizeof(uint8_t) + dayEventCount * sizeof(TEventManagerData);
	}
	buf->write(&p, sizeof(TPacketGCEventManager));
	buf->write(&subIndex, sizeof(uint8_t));
	buf->write(&dayCount, sizeof(uint8_t));
	buf->write(&cur_Time, sizeof(int));
	for (const auto& [dayIndex, dayData] : m_eventData)
	{
		const uint8_t dayEventCount = dayData.size();
		buf->write(&dayIndex, sizeof(uint8_t));
		buf->write(&dayEventCount, sizeof(uint8_t));
		if (dayEventCount > 0)
			buf->write(dayData.data(), dayEventCount * sizeof(TEventManagerData));
	}
}
void CHARACTER_MANAGER::UpdateAllPlayerEventData()
{
	TEMP_BUFFER buf;
	CompareEventSendData(&buf);
	const DESC_MANAGER::DESC_SET& c_ref_set = DESC_MANAGER::instance().GetClientSet();
	for (const auto& desc : c_ref_set)
	{
		if (!desc->GetCharacter())
			continue;
		desc->Packet(buf.read_peek(), buf.size());
	}
}
void CHARACTER_MANAGER::SendDataPlayer(entt::entity character)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::SendDataPlayer");//INGAME_DEBUG_RAZOR93
#endif
	auto desc = ecs::PlayerRuntime::GetDesc(character);
	if (!desc)
		return;
	TEMP_BUFFER buf;
	CompareEventSendData(&buf);
	desc->Packet(buf.read_peek(), buf.size());
}
bool CHARACTER_MANAGER::CloseEventManuel(uint8_t eventIndex)
{
	auto eventPtr = CheckEventIsActive(eventIndex);
	if (eventPtr != nullptr)
	{
		const uint8_t subHeader = EVENT_MANAGER_REMOVE_EVENT;
		db_clientdesc->DBPacketHeader(HEADER_GD_EVENT_MANAGER, 0, sizeof(uint8_t) + sizeof(uint16_t));
		db_clientdesc->Packet(&subHeader, sizeof(uint8_t));
		db_clientdesc->Packet(&eventPtr->eventID, sizeof(uint16_t));
		return true;
	}
	return false;
}
void CHARACTER_MANAGER::SetEventStatus(const uint16_t eventID, const bool eventStatus, const int endTime, const char* endTimeText)
{
	//eventStatus - 0-deactive  // 1-active

	TEventManagerData* eventData = nullptr;
	for (auto it = m_eventData.begin(); it != m_eventData.end(); ++it)
	{
		if (!it->second.empty())
		{
			for (auto& pData : it->second)
			{
				if (pData.eventID == eventID)
				{
					eventData = &pData;
					break;
				}
			}
		}
	}
	if (eventData == nullptr)
		return;
	eventData->eventStatus = eventStatus;
	eventData->endTime = endTime;
	strlcpy(eventData->endTimeText, endTimeText, sizeof(eventData->endTimeText));

	// Auto open&close notice
	const std::map<uint8_t, std::pair<int, int>> m_eventText = {
		{BONUS_EVENT,{2078,2079}},
		{DOUBLE_BOSS_LOOT_EVENT,{2080,2081}},
		{DOUBLE_METIN_LOOT_EVENT,{2082,2083}},
		{DOUBLE_MISSION_BOOK_EVENT,{2084,2085}},
		{DUNGEON_COOLDOWN_EVENT,{2086,2087}},
		{DUNGEON_TICKET_LOOT_EVENT,{2088,2089}},
		{MOONLIGHT_EVENT,{2090,2091}},
		{HATSZOG_LADA_EVENT,{2188,2189}},
		{BUPLA_RUN_BOSS_LOOT_EVENT,{2192,2193}},

		{MIKI_EVENT,{2220,2221}},
		{KARI_EVENT,{2222,2223}},
		{DUPLA_SZILI_EVENT,{2224,2225}},
		{COIN_EVENT,{2226,2227}},
		{DUPLA_BOSS_PONT_EVENT,{2228,2229}},
		{DUPLA_RUN_PONT_EVENT,{2230,2231}},
		{VALENTIN_EVENT,{2243,2244}},
		{EASTER_EVENT,{2245,2246}},
		{LELEKGOMB_EVENT,{2247,2248}},



	};
	const DESC_MANAGER::DESC_SET& c_ref_set = DESC_MANAGER::instance().GetClientSet();
	const auto it = m_eventText.find(eventData->eventIndex);
	if (it != m_eventText.end())
	{

		for (const auto& desc : c_ref_set)
		{
			auto* ch = desc->GetCharacter();
			if (!ch) continue;

#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ch->GetEntityHandle(), CHAT_TYPE_BIG_NOTICE, eventStatus ? it->second.first : it->second.second, "");
#endif
		}
	}



	// Bonus event update status
	if (eventData->eventIndex == BONUS_EVENT)
	{
		for (const auto& desc : c_ref_set)
		{
			auto* ch = desc->GetCharacter();
			if (!ch)
				continue;
			if (eventData->empireFlag != 0)
				if (eventData->empireFlag != ecs::PlayerRuntime::GetEmpire(ch->GetEntityHandle()))
					continue;
			if (eventData->channelFlag != 0)
				if (eventData->channelFlag != g_bChannel)
					return;
			if (!eventStatus)
			{
				const int32_t value = eventData->value[1];
				ch->ApplyPoint(eventData->value[0], -value);
			}
			ch->ComputePoints();
		}
	}

	const int now = time(nullptr);
	const uint8_t subIndex = EVENT_MANAGER_EVENT_STATUS;

	TPacketGCEventManager p;
	p.header = HEADER_GC_EVENT_MANAGER;
	p.size = sizeof(TPacketGCEventManager) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(bool) + sizeof(int) + sizeof(int) + sizeof(eventData->endTimeText);

	TEMP_BUFFER buf;
	buf.write(&p, sizeof(TPacketGCEventManager));
	buf.write(&subIndex, sizeof(uint8_t));
	buf.write(&eventData->eventID, sizeof(uint16_t));
	buf.write(&eventData->eventStatus, sizeof(bool));
	buf.write(&eventData->endTime, sizeof(int));
	buf.write(&eventData->endTimeText, sizeof(eventData->endTimeText));
	buf.write(&now, sizeof(int));

	for (const auto& desc : c_ref_set)
	{
		if (!desc->GetCharacter())
			continue;
		desc->Packet(buf.read_peek(), buf.size());
	}

}
void CHARACTER_MANAGER::SetEventData(uint8_t dayIndex, const std::vector<TEventManagerData>& m_data)
{
	if (const auto it = m_eventData.find(dayIndex); it == m_eventData.end())
		m_eventData.emplace(dayIndex, m_data);
	else
	{
		it->second.clear();
		for (const auto& newEvents : m_data)
			it->second.emplace_back(newEvents);
	}
}
#endif

#ifdef ENABLE_ITEMSHOP
void CHARACTER_MANAGER::LoadItemShopLogReal(entt::entity character, const char* c_pData)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::LoadItemShopLogReal");//INGAME_DEBUG_RAZOR93
#endif
	if (!ch)
		return;

	uint8_t subIndex = ITEMSHOP_LOG;

	const int logCount = *(int*)c_pData;
	c_pData += sizeof(int);
	std::vector<TIShopLogData> m_vec;
	if (logCount)
	{
		for (uint32_t j = 0; j < logCount; ++j)
		{
			const TIShopLogData logData = *(TIShopLogData*)c_pData;
			m_vec.emplace_back(logData);
			c_pData += sizeof(TIShopLogData);
		}
	}

	TPacketGCItemShop p;
	p.header = HEADER_GC_ITEMSHOP;
	p.size = sizeof(TPacketGCItemShop) + sizeof(uint8_t) + sizeof(int) + sizeof(TIShopLogData) * logCount;

	TEMP_BUFFER buf;
	buf.write(&p, sizeof(TPacketGCItemShop));
	buf.write(&subIndex, sizeof(uint8_t));
	buf.write(&logCount, sizeof(int));
	if (logCount)
		buf.write(m_vec.data(), sizeof(TIShopLogData) * logCount);

	ecs::PlayerRuntime::GetDesc(character)->Packet(buf.read_peek(), buf.size());
}
void CHARACTER_MANAGER::LoadItemShopLog(entt::entity character)
{
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::LoadItemShopLog");//INGAME_DEBUG_RAZOR93
#endif
	uint8_t subIndex = ITEMSHOP_LOG;
	uint32_t accountID = ecs::PlayerRuntime::GetDesc(character)->GetAccountTable().id;

	db_clientdesc->DBPacketHeader(HEADER_GD_ITEMSHOP, ecs::PlayerRuntime::GetDesc(character)->GetHandle(), sizeof(uint8_t) + sizeof(uint32_t));
	db_clientdesc->Packet(&subIndex, sizeof(uint8_t));
	db_clientdesc->Packet(&accountID, sizeof(uint32_t));
}
void CHARACTER_MANAGER::LoadItemShopData(entt::entity character, bool isAll)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::LoadItemShopData");//INGAME_DEBUG_RAZOR93
#endif
	TEMP_BUFFER buf;
	TPacketGCItemShop p;
	p.header = HEADER_GC_ITEMSHOP;

	uint32_t dragonCoin = ch->GetDragonCoin();

	if (isAll)
	{

		int calculateSize = 0;
		uint8_t subIndex = ITEMSHOP_LOAD;
		calculateSize += sizeof(uint8_t);

		calculateSize += sizeof(uint32_t);//dragon coin
		calculateSize += sizeof(int);//updatetime

		int categoryTotalSize = m_IShopManager.size();
		calculateSize += sizeof(int);

		if (!m_IShopManager.empty())
		{
			for (auto it = m_IShopManager.begin(); it != m_IShopManager.end(); ++it)
			{
				uint8_t categoryIndex = it->first;
				calculateSize += sizeof(uint8_t);
				uint8_t categorySize = it->second.size();
				calculateSize += sizeof(uint8_t);

				if (!it->second.empty())
				{
					for (auto itEx = it->second.begin(); itEx != it->second.end(); ++itEx)
					{
						uint8_t categorySubIndex = itEx->first;
						calculateSize += sizeof(uint8_t);
						uint8_t categorySubSize = itEx->second.size();
						calculateSize += sizeof(uint8_t);
						if (categorySubSize)
							calculateSize += sizeof(TIShopData) * categorySubSize;
					}
				}
			}
		}


		p.size = sizeof(TPacketGCItemShop) + calculateSize;


		buf.write(&p, sizeof(TPacketGCItemShop));
		buf.write(&subIndex, sizeof(uint8_t));
		buf.write(&dragonCoin, sizeof(uint32_t));
		buf.write(&itemshopUpdateTime, sizeof(int));
		buf.write(&categoryTotalSize, sizeof(int));

		if (!m_IShopManager.empty())
		{
			for (auto it = m_IShopManager.begin(); it != m_IShopManager.end(); ++it)
			{
				uint8_t categoryIndex = it->first;
				buf.write(&categoryIndex, sizeof(uint8_t));
				uint8_t categorySize = it->second.size();
				buf.write(&categorySize, sizeof(uint8_t));
				if (!it->second.empty())
				{
					for (auto itEx = it->second.begin(); itEx != it->second.end(); ++itEx)
					{
						uint8_t categorySubIndex = itEx->first;
						buf.write(&categorySubIndex, sizeof(uint8_t));
						uint8_t categorySubSize = itEx->second.size();
						buf.write(&categorySubSize, sizeof(uint8_t));
						if (categorySubSize)
							buf.write(itEx->second.data(), sizeof(TIShopData) * categorySubSize);
					}
				}
			}
		}
	}
	else
	{
		p.size = sizeof(TPacketGCItemShop) + sizeof(uint8_t) + sizeof(int) + sizeof(int);
		buf.write(&p, sizeof(TPacketGCItemShop));
		uint8_t subIndex = ITEMSHOP_LOAD;
		buf.write(&subIndex, sizeof(uint8_t));
		buf.write(&dragonCoin, sizeof(uint32_t));
		buf.write(&itemshopUpdateTime, sizeof(int));
		int categoryTotalSize = 9999;
		buf.write(&categoryTotalSize, sizeof(int));
	}
	ecs::PlayerRuntime::GetDesc(character)->Packet(buf.read_peek(), buf.size());
}



bool CHARACTER_MANAGER::GetItemShopDataByVnum(uint32_t vnum, TIShopData& outData) const
{
	for (const auto& categoryEntry : m_IShopManager)
	{
		for (const auto& subCategoryEntry : categoryEntry.second)
		{
			for (const auto& shopData : subCategoryEntry.second)
			{
				if (shopData.itemVnum == vnum)
				{
					outData = shopData;
					return true;
				}
			}
		}
	}

	return false;
}


void CHARACTER_MANAGER::LoadItemShopBuyReal(entt::entity character, const char* c_pData)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::LoadItemShopBuyReal");//INGAME_DEBUG_RAZOR93
#endif
	if (!ch)
		return;
	const uint8_t returnType = *(uint8_t*)c_pData;
	c_pData += sizeof(uint8_t);

	if (returnType == 0)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You don't have enought dragon coin!");
		return;
	}
	else if (returnType == 1)
	{
		const int weekMaxCount = *(int*)c_pData;
		c_pData += sizeof(int);
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You cannot exceed the weekly purchase count.");
		return;
	}
	else if (returnType == 2)
	{
		const int monthMaxCount = *(int*)c_pData;
		c_pData += sizeof(int);
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You cannot exceed the monthly purchase count.");
		return;
	}

	const bool isOpenLog = *(bool*)c_pData;
	c_pData += sizeof(bool);

	const uint32_t itemVnum = *(uint32_t*)c_pData;
	c_pData += sizeof(uint32_t);

	const int itemCount = *(int*)c_pData;
	c_pData += sizeof(int);

	const uint32_t itemPrice = *(uint32_t*)c_pData;
	c_pData += sizeof(uint32_t);

	/*const bool hasMallItem = *(bool*)c_pData;
	c_pData += sizeof(bool);

	TPlayerItem mallItem{};
	if (hasMallItem)
	{
		mallItem = *(TPlayerItem*)c_pData;
		c_pData += sizeof(TPlayerItem);
	}*/

	TEMP_BUFFER buf;
	TPacketGCItemShop p;
	p.header = HEADER_GC_ITEMSHOP;
	p.size = sizeof(TPacketGCItemShop) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(bool);

	if (isOpenLog)
		p.size += sizeof(TIShopLogData);

	uint8_t subIndex = ITEMSHOP_DRAGONCOIN;
	uint32_t dragonCoin = ch->GetDragonCoin();

	buf.write(&p, sizeof(TPacketGCItemShop));
	buf.write(&subIndex, sizeof(uint8_t));
	buf.write(&dragonCoin, sizeof(uint32_t));
	buf.write(&isOpenLog, sizeof(bool));
	if (isOpenLog)
	{
		const TIShopLogData logData = *(TIShopLogData*)c_pData;
		c_pData += sizeof(TIShopLogData);

		buf.write(&logData, sizeof(TIShopLogData));
	}
	ecs::PlayerRuntime::GetDesc(character)->Packet(buf.read_peek(), buf.size());

	if (returnType == 3)
	{
		TIShopData itemData{};
		if (GetItemShopDataByVnum(itemVnum, itemData))
		{
			LPITEM item = ITEM_MANAGER::instance().CreateItem(itemData.itemVnum, itemCount, 0, true);

			if (item)
			{
				int32_t alSockets[ITEM_SOCKET_MAX_NUM] = {};
				for (size_t i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
					alSockets[i] = itemData.alSocket[i];

				const TItemTable* itemProto = ItemSystem::GetItemProto((item ? item->GetEntityHandle() : entt::null));
				if (itemProto != nullptr && alSockets[ITEM_SOCKET_REMAIN_SEC] == 0)
				{
					for (const auto& limit : itemProto->aLimits)
					{
						if (limit.bType == LIMIT_REAL_TIME || limit.bType == LIMIT_REAL_TIME_START_FIRST_USE || limit.bType == LIMIT_TIMER_BASED_ON_WEAR)
						{
							alSockets[ITEM_SOCKET_REMAIN_SEC] = limit.lValue == 0 ? 60 * 60 * 24 * 7 : limit.lValue;
							break;
						}
					}
				}

				if (itemProto != nullptr && itemProto->bType == ITEM_UNIQUE && alSockets[ITEM_SOCKET_UNIQUE_REMAIN_TIME] == 0)
				{
					const int32_t remainSec = itemProto->alValues[ITEM_SOCKET_REMAIN_SEC];
#ifdef ENABLE_EXTEND_ITEM_AWARD
					const int32_t remainTime = itemProto->alValues[ITEM_SOCKET_UNIQUE_REMAIN_TIME];
					alSockets[ITEM_SOCKET_UNIQUE_REMAIN_TIME] = remainTime == 0 ? remainSec : static_cast<int32_t>(time(nullptr) + remainSec);
#else
					alSockets[ITEM_SOCKET_UNIQUE_REMAIN_TIME] = remainSec;
#endif
				}

				for (size_t i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
					ItemSystem::SetItemSocket((item ? item->GetEntityHandle() : entt::null), i, alSockets[i]);

				for (size_t i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
				{
					const TPlayerItemAttribute& attribute = itemData.aAttr[i];
					if (attribute.bType != 0)
						ItemSystem::SetItemForceAttributeEcs((item ? item->GetEntityHandle() : entt::null), i, attribute.bType, attribute.sValue);
				}

				ch->AutoGiveItem(item);
			}
			else
			{
				LOG_ERROR("ItemShop purchase failed to create item vnum {} for {}", itemVnum, ecs::PlayerRuntime::GetName(character).data());
			}
		}
		else
		{
			LOG_ERROR("ItemShop purchase could not find item data for vnum {}", itemVnum);
		}
	}


	/*if (hasMallItem && ch->GetMall())
	{
		LPITEM item = ITEM_MANAGER::instance().CreateItem(mallItem.vnum, mallItem.count, mallItem.id);
		if (item)
		{
			ItemSystem::SetItemSkipSave((item ? item->GetEntityHandle() : entt::null), true);
			item->SetSockets(mallItem.alSockets);
			item->SetAttributes(mallItem.aAttr);
#ifdef ATTR_LOCK
			item->SetLockedAttr(mallItem.lockedattr);
#endif
			if (ch->GetMall()->Add(mallItem.pos, item))
				ItemSystem::SetItemSkipSave((item ? item->GetEntityHandle() : entt::null), false);
			else
				M2_DESTROY_ITEM(item);
		}
	}*/

	if (itemCount > 1)
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You bought from game itemshop : count: %d Coins: %u", itemCount, itemPrice);
	else
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You bought from game itemshop :  Coins: %u", itemPrice);
}
void CHARACTER_MANAGER::LoadItemShopBuy(entt::entity character, int itemID, int itemCount)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
#ifdef ENABLE_INGAME_DEBUG_RAZOR93
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "char_manager.cpp::CHARACTER_MANAGER::LoadItemShopBuy");//INGAME_DEBUG_RAZOR93
#endif
	if (itemCount > 20)
		return;

	if (!m_IShopManager.empty())
	{
		for (auto it = m_IShopManager.begin(); it != m_IShopManager.end(); ++it)
		{
			if (!it->second.empty())
			{
				for (auto itEx = it->second.begin(); itEx != it->second.end(); ++itEx)
				{
					if (!itEx->second.empty())
					{
						for (auto itReal = itEx->second.begin(); itReal != itEx->second.end(); ++itReal)
						{
							const TIShopData& itemData = *itReal;
							if (std::cmp_equal(itemData.id, itemID))
							{
								uint32_t dragonCoin = ch->GetDragonCoin();
								uint32_t itemPrice = itemData.itemPrice * itemCount;
								if (itemData.discount > 0)
									//itemPrice = long long(float(itemData.itemPrice) / 100.0 * float(100 - itemData.discount));//razor93
									itemPrice = static_cast<uint32_t>(float(itemData.itemPrice) / 100.0f * float(100 - itemData.discount));


								if (itemPrice > dragonCoin)
								{
									ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You don't have enough DC");
									return;
								}

								uint32_t accountID = ecs::PlayerRuntime::GetDesc(character)->GetAccountTable().id;
								uint8_t subIndex = ITEMSHOP_BUY;
								char playerName[CHARACTER_NAME_MAX_LEN + 1];
								strlcpy(playerName, ecs::PlayerRuntime::GetName(character).data(), sizeof(playerName));

								char ipAdress[16];
								strlcpy(ipAdress, ecs::PlayerRuntime::GetDesc(character)->GetHostName(), sizeof(ipAdress));

								TEMP_BUFFER buf;
								buf.write(&subIndex, sizeof(uint8_t));
								buf.write(&accountID, sizeof(uint32_t));
								buf.write(&playerName, sizeof(playerName));
								buf.write(&ipAdress, sizeof(ipAdress));
								buf.write(&itemID, sizeof(int));
								buf.write(&itemCount, sizeof(int));
								bool isLogOpen = ch->GetProtectTime("itemshop.log") == 1 ? true : false;
								buf.write(&isLogOpen, sizeof(bool));

								db_clientdesc->DBPacketHeader(HEADER_GD_ITEMSHOP, ecs::PlayerRuntime::GetDesc(character)->GetHandle(), buf.size());
								db_clientdesc->Packet(buf.read_peek(), buf.size());

								return;
							}
						}
					}
				}
			}
		}
	}


}
void RefreshItemShop(LPDESC d)
{
	LPCHARACTER ch = d->GetCharacter();
	if (!ch)
		return;
	const entt::entity character = ch->GetEntityHandle();

	if (ch->GetProtectTime("itemshop.load") == 1)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "ItemShop was update!");
		CHARACTER_MANAGER::Instance().LoadItemShopData(character, true);
	}
}
void CHARACTER_MANAGER::LoadItemShopData(const char* c_pData)
{
	m_IShopManager.clear();

	const int updateTime = *(int*)c_pData;
	c_pData += sizeof(int);

	const bool isManuelUpdate = *(bool*)c_pData;
	c_pData += sizeof(bool);

	const int categoryTotalSize = *(int*)c_pData;
	c_pData += sizeof(int);

	itemshopUpdateTime = updateTime;

	for (uint32_t j = 0; j < categoryTotalSize; ++j)
	{
		const uint8_t categoryIndex = *(uint8_t*)c_pData;
		c_pData += sizeof(uint8_t);
		const uint8_t categorySize = *(uint8_t*)c_pData;
		c_pData += sizeof(uint8_t);

		std::map<uint8_t, std::vector<TIShopData>> m_map;
		m_map.clear();

		for (uint32_t x = 0; x < categorySize; ++x)
		{
			const uint8_t categorySubIndex = *(uint8_t*)c_pData;
			c_pData += sizeof(uint8_t);

			const uint8_t categorySubSize = *(uint8_t*)c_pData;
			c_pData += sizeof(uint8_t);

			std::vector<TIShopData> m_vec;
			m_vec.clear();

			for (uint32_t b = 0; b < categorySubSize; ++b)
			{
				const TIShopData itemData = *(TIShopData*)c_pData;

				m_vec.emplace_back(itemData);
				c_pData += sizeof(TIShopData);
			}

			if (!m_vec.empty())
				m_map.emplace(categorySubIndex, m_vec);
		}
		if (!m_map.empty())
			m_IShopManager.emplace(categoryIndex, m_map);
	}

	if (isManuelUpdate)
	{
		const DESC_MANAGER::DESC_SET& c_ref_set = DESC_MANAGER::instance().GetClientSet();
		std::for_each(c_ref_set.begin(), c_ref_set.end(), RefreshItemShop);
	}
}
#endif





