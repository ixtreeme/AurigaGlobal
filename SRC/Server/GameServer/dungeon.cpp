#include "stdafx.h"
#include "dungeon.h"
#include "char.h"
#include "char_manager.h"
#include "party.h"
#include "affect.h"
#include "packet.h"
#include "desc.h"
#include "config.h"
#include "regen.h"
#include "start_position.h"
#include "item.h"
#include "item_manager.h"
#include "utils.h"
#include "questmanager.h"
#include "ecs/EventDispatcher.hpp"
#include "ecs/events.hpp"

CDungeon::CDungeon(IdType id, int32_t lOriginalMapIndex, int32_t lMapIndex)
	: m_id(id),
	m_lOrigMapIndex(lOriginalMapIndex),
	m_lMapIndex(lMapIndex),
	m_map_Area(SECTREE_MANAGER::instance().GetDungeonArea(lOriginalMapIndex))
{
	Initialize();
	//sys_log(0,"DUNGEON create orig %d real %d", lOriginalMapIndex, lMapIndex);
}

CDungeon::~CDungeon()
{
#ifdef __DEFENSE_WAVE__
	m_Mast = nullptr;
#endif

	if (m_pParty != nullptr) {
		m_pParty->SetDungeon_for_Only_party (nullptr);
	}

	//sys_log(0,"DUNGEON destroy orig %d real %d", m_lOrigMapIndex, m_lMapIndex	);

	ClearRegen();

	if (deadEvent) {
		event_cancel(&deadEvent);
	}

	if (exit_all_event_) {
		event_cancel(&exit_all_event_);
	}

	if (jump_to_event_) {
		event_cancel(&jump_to_event_);
	}
}

struct FWarpToDungeonCoords
{
	FWarpToDungeonCoords(int32_t lMapIndex, int32_t X, int32_t Y, LPDUNGEON d)
	: m_lMapIndex(lMapIndex), m_x(X), m_y(Y), m_pkDungeon(d)
	{
	}

	void operator () (LPCHARACTER ch)
	{
		ch->SaveExitLocation();
		ch->WarpSet(m_x, m_y, m_lMapIndex);
	}

	int32_t m_lMapIndex;
	int32_t m_x;
	int32_t m_y;
	LPDUNGEON m_pkDungeon;
};

void CDungeon::Join_Coords(LPCHARACTER ch, int32_t X, int32_t Y, int32_t index)
{
	if (SECTREE_MANAGER::instance().GetMap(m_lMapIndex) == nullptr) 
	{
		sys_err("CDungeon: SECTREE_MAP not found for #%ld", m_lMapIndex);
		return;
	}
	X*=100;
	Y*=100;
	FWarpToDungeonCoords(m_lMapIndex, X, Y, this) (ch);
}

void CDungeon::JoinParty_Coords(LPPARTY pParty, int32_t X, int32_t Y, int32_t index)
{
	pParty->SetDungeon(this);
	m_map_pkParty.insert(std::make_pair(pParty,0));

	if (SECTREE_MANAGER::instance().GetMap(m_lMapIndex) == nullptr) 
	{
		sys_err("CDungeon: SECTREE_MAP not found for #%ld", m_lMapIndex);
		return;
	}
	X*=100;
	Y*=100;
	FWarpToDungeonCoords f(m_lMapIndex, X, Y, this);
	pParty->ForEachOnMapMember(f,index);
}

void CDungeon::Initialize()
{
	m_completed = false;
	deadEvent = nullptr;
	// <Factor>
	exit_all_event_ = nullptr;
	jump_to_event_ = nullptr;
	regen_id_ = 0;

	m_monstercount = 0;

	m_iWarpDelay = 0;
	m_lWarpMapIndex = 0;
	m_lWarpX = 0;
	m_lWarpY = 0;

	m_stRegenFile = "";

	m_pParty = nullptr;
#ifdef __DEFENSE_WAVE__
	m_Mast = nullptr;
#endif
}

void CDungeon::SetFlag(std::string name, int32_t value)
{
	auto it = m_map_Flag.find(name);
	if (it != m_map_Flag.end())
	{
		it->second = value;
	}
	else
	{
		m_map_Flag.insert(make_pair(name, value));
	}
}

int CDungeon::GetFlag(std::string name)
{
	auto it =  m_map_Flag.find(name);
	if (it != m_map_Flag.end())
		return it->second;
	else
		return 0;
}

struct FWarpToDungeon
{
	FWarpToDungeon(int32_t lMapIndex, LPDUNGEON d)
		: m_lMapIndex(lMapIndex), m_pkDungeon(d)
		{
			LPSECTREE_MAP pkSectreeMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
			m_x = pkSectreeMap->m_setting.posSpawn.x;
			m_y = pkSectreeMap->m_setting.posSpawn.y;
		}

	void operator () (LPCHARACTER ch)
	{
		ch->SaveExitLocation();
		ch->WarpSet(m_x, m_y, m_lMapIndex);
		//m_pkDungeon->IncPartyMember(ch->GetParty());
	}

	int32_t m_lMapIndex;
	int32_t m_x;
	int32_t m_y;
	LPDUNGEON m_pkDungeon;
};

void CDungeon::Join(LPCHARACTER ch)
{
	if (SECTREE_MANAGER::instance().GetMap(m_lMapIndex) == nullptr) {
		sys_err("CDungeon: SECTREE_MAP not found for #%ld", m_lMapIndex);
		return;
	}
	FWarpToDungeon(m_lMapIndex, this) (ch);
}

void CDungeon::JoinParty(LPPARTY pParty)
{
	pParty->SetDungeon(this); // @warme011 the begin of the nightmare
	m_map_pkParty.insert(std::make_pair(pParty,0));

	if (SECTREE_MANAGER::instance().GetMap(m_lMapIndex) == nullptr) {
		sys_err("CDungeon: SECTREE_MAP not found for #%ld", m_lMapIndex);
		return;
	}
	FWarpToDungeon f(m_lMapIndex, this);
	pParty->ForEachOnlineMember(f);
	//sys_log(0, "DUNGEON-PARTY join %p %p", this, pParty);
}

void CDungeon::QuitParty(LPPARTY pParty)
{
	pParty->SetDungeon(nullptr);
	//sys_log(0, "DUNGEON-PARTY quit %p %p", this, pParty);
	TPartyMap::iterator it = m_map_pkParty.find(pParty); // @warme011 boom! crash!
	if (it != m_map_pkParty.end())
		m_map_pkParty.erase(it);
}

EVENTINFO(dungeon_id_info)
{
	CDungeon::IdType dungeon_id;

	dungeon_id_info()
	: dungeon_id(0)
	{
	}
};

EVENTFUNC(dungeon_dead_event)
{
	dungeon_id_info* info = dynamic_cast<dungeon_id_info*>( event->info );

	if ( info == nullptr)
	{
		sys_err( "dungeon_dead_event> <Factor> Null pointer" );
		return 0;
	}

	LPDUNGEON pDungeon = CDungeonManager::instance().Find(info->dungeon_id);
	if (pDungeon == nullptr) {
		return 0;
	}

	pDungeon->deadEvent = nullptr;

	CDungeonManager::instance().Destroy(info->dungeon_id);
	g_dispatcher.trigger(ecs::EvDungeonDead { info->dungeon_id });
	return 0;
}

void CDungeon::IncMember(LPCHARACTER ch)
{
	if (m_set_pkCharacter.find(ch) == m_set_pkCharacter.end())
		m_set_pkCharacter.insert(ch);

	event_cancel(&deadEvent);
}

void CDungeon::DecMember(LPCHARACTER ch)
{
	auto it = m_set_pkCharacter.find(ch);

	if (it == m_set_pkCharacter.end()) {
		return;
	}

	m_set_pkCharacter.erase(it);

	if (m_set_pkCharacter.empty())
	{
		dungeon_id_info* info = AllocEventInfo<dungeon_id_info>();
		info->dungeon_id = m_id;

		event_cancel(&deadEvent);
		int iSec = m_completed ? 3 : 300;
		deadEvent = event_create(dungeon_dead_event, info, PASSES_PER_SEC(iSec));
	}
}

void CDungeon::IncPartyMember(LPPARTY pParty, LPCHARACTER ch)
{
	//sys_log(0, "DUNGEON-PARTY inc %p %p", this, pParty);
	TPartyMap::iterator it = m_map_pkParty.find(pParty);

	if (it != m_map_pkParty.end())
		it->second++;
	else
		m_map_pkParty.insert(std::make_pair(pParty,1));

	IncMember(ch);
}

void CDungeon::DecPartyMember(LPPARTY pParty, LPCHARACTER ch)
{
	//sys_log(0, "DUNGEON-PARTY dec %p %p", this, pParty);
	TPartyMap::iterator it = m_map_pkParty.find(pParty);

	if (it == m_map_pkParty.end())
		sys_err("cannot find party");
	else
	{
		it->second--;

		if (it->second == 0)
			QuitParty(pParty);
	}

	DecMember(ch);
}

struct FWarpToPosition
{
	int32_t lMapIndex;
	int32_t x;
	int32_t y;
	FWarpToPosition(int32_t lMapIndex, int32_t x, int32_t y)
		: lMapIndex(lMapIndex), x(x), y(y)
		{}

	void operator()(LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_CHARACTER)) {
			return;
		}
		LPCHARACTER ch = (LPCHARACTER)ent;
		if (!ch->IsPC()) {
			return;
		}
		if (ch->GetMapIndex() == lMapIndex)
		{
			ch->Show(lMapIndex, x, y, 0);
			ch->Stop();
		}
		else
		{
			ch->WarpSet(x,y,lMapIndex);
		}
	}
};

void CDungeon::JumpAll(int32_t idx, int32_t x, int32_t y)
{
	x *= 100;
	y *= 100;

	LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(idx);
	if (!pMap)
	{
		sys_err("cannot find map by index %d", idx);
		return;
	}

	FWarpToPosition f(m_lMapIndex, x, y);

	pMap->for_each(f);
}

void CDungeon::SetPartyNull()
{
	m_pParty = nullptr;
}


void CDungeonManager::Destroy(CDungeon::IdType dungeon_id)
{
	sys_log(0, "DUNGEON destroy : map index %u", dungeon_id);
	LPDUNGEON pDungeon = Find(dungeon_id);
	if (pDungeon == nullptr) {
		return;
	}
	m_map_pkDungeon.erase(dungeon_id);

	int32_t lMapIndex = pDungeon->m_lMapIndex;
	m_map_pkMapDungeon.erase(lMapIndex);

	uint32_t server_timer_arg = lMapIndex;
	quest::CQuestManager::instance().CancelServerTimers(server_timer_arg);

	SECTREE_MANAGER::instance().DestroyPrivateMap(lMapIndex);
	M2_DELETE(pDungeon);
}

LPDUNGEON CDungeonManager::Find(CDungeon::IdType dungeon_id)
{
	auto it = m_map_pkDungeon.find(dungeon_id);
	if (it != m_map_pkDungeon.end())
		return it->second;
	return nullptr;
}

LPDUNGEON CDungeonManager::FindByMapIndex(int32_t lMapIndex)
{
	auto it = m_map_pkMapDungeon.find(lMapIndex);
	if (it != m_map_pkMapDungeon.end()) {
		return it->second;
	}
	return nullptr;
}

LPDUNGEON CDungeonManager::Create(int32_t lOriginalMapIndex)
{
	uint32_t lMapIndex = SECTREE_MANAGER::instance().CreatePrivateMap(lOriginalMapIndex);

	if (!lMapIndex)
	{
		sys_log( 0, "Fail to Create Dungeon : OrginalMapindex %d NewMapindex %d", lOriginalMapIndex, lMapIndex );
		return nullptr;
	}

	// <Factor> TODO: Change id assignment, or drop it
	CDungeon::IdType id = next_id_++;
	while (Find(id) != nullptr) {
		id = next_id_++;
	}

	LPDUNGEON pDungeon = M2_NEW CDungeon(id, lOriginalMapIndex, lMapIndex);
	if (!pDungeon)
	{
		sys_err("M2_NEW CDungeon failed");
		return nullptr;
	}

	m_map_pkDungeon.insert(std::make_pair(id, pDungeon));
	m_map_pkMapDungeon.insert(std::make_pair(lMapIndex, pDungeon));

	return pDungeon;
}

CDungeonManager::CDungeonManager()
	: next_id_(0)
{
}

CDungeonManager::~CDungeonManager()
{
}

void CDungeon::SetUnique(const char* key, uint32_t vid)
{
	LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid);
	if (!ch) {
		sys_err("Unknown monster: %d for dungeon %d.", vid, m_lMapIndex);
		return;
	}

	m_map_UniqueMob.insert(std::make_pair(std::string(key), ch));
	ch->AddAffect(AFFECT_DUNGEON_UNIQUE, POINT_NONE, 0, AFF_DUNGEON_UNIQUE, 65535, 0, true);
}

void CDungeon::KillUnique(const std::string& key)
{
	auto it = m_map_UniqueMob.find(key);
	if (it == m_map_UniqueMob.end())
	{
		sys_err("Unknown get unique: %s for dungeon %d.", key.c_str(), m_lMapIndex);
		return;
	}

	LPCHARACTER ch = it->second;
	m_map_UniqueMob.erase(it);
	ch->Dead();
}

int32_t CDungeon::GetUniqueVid(const std::string& key)
{
	auto it = m_map_UniqueMob.find(key);
	if (it == m_map_UniqueMob.end())
	{
		sys_err("Unknown get unique: %s for dungeon %d.", key.c_str(), m_lMapIndex);
		return false;
	}

	return it->second->GetVID();
}

void CDungeon::DeadCharacter(LPCHARACTER ch)
{
	if (!ch->IsPC())
	{
		if (ch->FindAffect(AFFECT_DUNGEON_UNIQUE)) {
			auto it = m_map_UniqueMob.begin();
			for ( ; it != m_map_UniqueMob.end(); ) {
				if (it->second == ch)
				{
					it = m_map_UniqueMob.erase(it);
					break;
				}
				else
				{
					++it;
				}
			}
		}
	}
}

bool CDungeon::IsUniqueDead(const std::string& key)
{
	auto it = m_map_UniqueMob.find(key);
	if (it == m_map_UniqueMob.end())
	{
		sys_err("Unknown unique: %s for dungeon %d.", key.c_str(), m_lMapIndex);
		return false;
	}

	return it->second->IsDead();
}

LPCHARACTER CDungeon::SpawnMob(int32_t vnum, int32_t x, int32_t y, int32_t dir)
{
	LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (!map) {
		sys_err("cannot find map by index %d", m_lMapIndex);
		return nullptr;
	}

	LPCHARACTER ch = CHARACTER_MANAGER::instance().SpawnMob(vnum, m_lMapIndex, map->m_setting.iBaseX+x*100, map->m_setting.iBaseY+y*100, 0, false, dir == 0 ? -1 : dir);

	if (ch)
	{
		ch->SetDungeon(this);
	}
	else
	{
		sys_err("cannot spawn: vnum(%d), x(%d), y(%d), dir(%d) inside the map %d", vnum, x, y, dir, m_lMapIndex);
	}

	return ch;
}

void CDungeon::SpawnRegen(const char* filename, bool once)
{
	if (!filename)
	{
		sys_err("CDungeon::SpawnRegen(filename=NULL, once=%d) - m_lMapIndex[%d]", once, m_lMapIndex);
		return;
	}

	LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (!map)
	{
		sys_err("CDungeon::SpawnRegen(filename=%s, once=%d) - m_lMapIndex[%d]", filename, once, m_lMapIndex);
		return;
	}

	regen_do(filename, m_lMapIndex, map->m_setting.iBaseX, map->m_setting.iBaseY, this, once);
}

void CDungeon::AddRegen(LPREGEN regen)
{
	regen->id = regen_id_++;
	m_regen.push_back(regen);
}

void CDungeon::ClearRegen()
{
	for (auto it = m_regen.begin(); it != m_regen.end(); ++it)
	{
		LPREGEN regen = *it;

		event_cancel(&regen->event);
		M2_DELETE(regen);
	}
	m_regen.clear();
}

bool CDungeon::IsValidRegen(LPREGEN regen, size_t regen_id) {
	auto it = std::find(m_regen.begin(), m_regen.end(), regen);
	if (it == m_regen.end()) {
		return false;
	}
	LPREGEN found = *it;
	return (found->id == regen_id);
}

namespace
{
	struct FKillSectree
	{
		void operator () (LPENTITY ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER ch = (LPCHARACTER) ent;
				if (!ch->IsPC() && !ch->IsPet() && !ch->IsMount()
#ifdef __NEWPET_SYSTEM__
				 && !ch->IsNewPet()
#endif
				)
				{
					ch->Dead();
				}
			}
		}
	};

	struct FKillMonstersSectree
	{
		void operator () (LPENTITY ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER ch = (LPCHARACTER) ent;
				if (!ch->IsPC() && (ch->IsMonster() || ch->IsStone()))
				{
					ch->Dead();
				}
			}
		}
	};

#ifdef __DEFENSE_WAVE__
	struct FKillMonstersHydraSectree
	{
		void operator () (LPENTITY ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER ch = (LPCHARACTER) ent;
				if (!ch->IsPC() && (ch->IsMonster() || ch->IsStone()))
				{
					int32_t racevnum = ch->GetRaceNum();
					if (racevnum != 3963 && racevnum != 3964)
					{
						ch->Dead();
					}
				}
			}
		}
	};
#endif

	struct FPurgeSectree
	{
		void operator () (LPENTITY ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER ch = (LPCHARACTER) ent;

#ifdef __NEWPET_SYSTEM__
				if (!ch->IsPC() && !ch->IsPet() && !ch->IsNewPet()
#else
				if (!ch->IsPC() && !ch->IsPet()
#endif
				)
				{
					M2_DESTROY_CHARACTER(ch);
				}
			}
			else if (ent->IsType(ENTITY_ITEM))
			{
				LPITEM item = (LPITEM) ent;
				M2_DESTROY_ITEM(item);
			}
			else
				sys_err("unknown entity type %d is in dungeon", ent->GetType());
		}
	};
}

void CDungeon::KillAll()
{
	LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (map == nullptr)
	{
		sys_err("CDungeon: SECTREE_MAP not found for #%ld", m_lMapIndex);
		return;
	}

	FKillSectree f;
	map->for_each(f);
}

void CDungeon::KillAllMonsters()
{
	LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (map == nullptr)
	{
		sys_err("CDungeon: SECTREE_MAP not found for #%ld", m_lMapIndex);
		return;
	}

	FKillMonstersSectree f;
	map->for_each(f);
}

#ifdef __DEFENSE_WAVE__
void CDungeon::KillAllMonstersHydra()
{
	LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (map == nullptr)
	{
		sys_err("CDungeon: SECTREE_MAP not found for #%ld", m_lMapIndex);
		return;
	}

	FKillMonstersHydraSectree f;
	map->for_each(f);
}
#endif

void CDungeon::Purge()
{
	LPSECTREE_MAP pkMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (pkMap == nullptr) {
		sys_err("CDungeon: SECTREE_MAP not found for #%ld", m_lMapIndex);
		return;
	}
	FPurgeSectree f;
	pkMap->for_each(f);
}

struct FExitDungeonLobby
{
	uint8_t lobby;
	FExitDungeonLobby() : lobby(0) {};

	void operator()(LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER) ent;
			if (ch->IsPC())
			{
				if (lobby == 1)
				{
					ch->WarpSet(535400, 1428400);
				}
				else if (lobby == 2)
				{
					ch->WarpSet(536900, 1331400);
				}
				else if (lobby == 3)
				{
					ch->WarpSet(645800, 351400);
				}
			}
		}
	}
};

void CDungeon::ExitAllLobby(uint8_t lobby)
{
	LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (!map)
	{
		sys_err("cannot find map by index %d", m_lMapIndex);
		return;
	}

	FExitDungeonLobby f;
	f.lobby = lobby;

	map->for_each(f);
	m_completed = true;
}

namespace
{
	struct FCmdChat
	{
		FCmdChat(const char * psz) : m_psz(psz)
		{
		}

		void operator() (LPENTITY ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER ch = (LPCHARACTER) ent;
				if (ch->IsPC())
				{
					ch->ChatPacket(CHAT_TYPE_COMMAND, "%s", m_psz);
				}
			}
		}

		const char * m_psz;
	};
}

void CDungeon::CmdChat(const char* msg)
{
	LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (!map)
	{
		sys_err("cannot find map by index %d", m_lMapIndex);
		return;
	}

	FCmdChat f(msg);
	map->for_each(f);
}

namespace
{
	struct FNotice
	{
		FNotice(
#ifdef TEXTS_IMPROVEMENT
		uint32_t idx, bool big,
#endif
		const char * psz): 
#ifdef TEXTS_IMPROVEMENT
		m_idx(idx), m_big(big),
#endif
		m_psz(psz)
		{
		}

		void operator() (LPENTITY ent) {
			if (ent->IsType(ENTITY_CHARACTER)) {
				LPCHARACTER ch = (LPCHARACTER) ent;
				if (ch->IsPC()) {
#ifdef TEXTS_IMPROVEMENT
					if (m_big == true)
					{
						ch->ChatPacketNew(CHAT_TYPE_BIG_NOTICE, m_idx, m_psz);
					}
					else
					{
						ch->ChatPacketNew(CHAT_TYPE_NOTICE, m_idx, m_psz);
					}
#else
					ch->ChatPacket(CHAT_TYPE_NOTICE, "%s", m_psz);
#endif
				}
			}
		}

#ifdef TEXTS_IMPROVEMENT
		uint32_t m_idx;
		bool m_big;
#endif
		const char * m_psz;
	};
}

void CDungeon::Notice(
#ifdef TEXTS_IMPROVEMENT
uint32_t idx,
#endif
const char* msg
#ifdef TEXTS_IMPROVEMENT
, bool big
#endif
)
{
	sys_log(0, "XXX Dungeon Notice %p %s", this, msg);
	LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (!pMap)
	{
		sys_err("cannot find map by index %d", m_lMapIndex);
		return;
	}

	FNotice f(
#ifdef TEXTS_IMPROVEMENT
	idx, big,
#endif
	msg);
	pMap->for_each(f);
}

EVENTFUNC(dungeon_jump_to_event)
{
	dungeon_id_info * info = dynamic_cast<dungeon_id_info *>(event->info);

	if ( info == nullptr)
	{
		sys_err( "dungeon_jump_to_event> <Factor> Null pointer" );
		return 0;
	}

	LPDUNGEON pDungeon = CDungeonManager::instance().Find(info->dungeon_id);
	pDungeon->jump_to_event_ = nullptr;

	if (pDungeon)
	{
		pDungeon->JumpToEliminateLocation();
		g_dispatcher.trigger(ecs::EvDungeonPrepare { info->dungeon_id });
	}
	else
		sys_err("cannot find dungeon with map index %u", info->dungeon_id);

	return 0;
}

void CDungeon::JumpToEliminateLocation()
{
	LPDUNGEON pDungeon = CDungeonManager::instance().FindByMapIndex(m_lWarpMapIndex);

	if (pDungeon)
	{
		pDungeon->JumpAll(m_lMapIndex, m_lWarpX, m_lWarpY);

		if (!m_stRegenFile.empty())
		{
			pDungeon->SpawnRegen(m_stRegenFile.c_str());
			m_stRegenFile.clear();
		}
	}
	else
	{
		// 일반 맵으로 워프
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);

		if (!pMap)
		{
			sys_err("no map by index %d", m_lMapIndex);
			return;
		}

		FWarpToPosition f(m_lWarpMapIndex, m_lWarpX * 100, m_lWarpY * 100);

		// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
		pMap->for_each(f);
	}
}

#ifdef __DEFENSE_WAVE__
struct SUpdateMastHp
{
	SUpdateMastHp(int64_t value) : m_value(value) {}

	void operator () (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			LPCHARACTER ch = (LPCHARACTER) ent;
			if (ch->IsPC())
			{
				ch->ChatPacket(CHAT_TYPE_COMMAND, "BINARY_Update_Mast_HP %d", m_value);
			}
		}
	}

	int64_t m_value;
};

void CDungeon::UpdateMastHP()
{
	LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (!map)
	{
		sys_err("cannot find map by index %d", m_lMapIndex);
		return;
	}

	LPCHARACTER mast = GetMast();
	if (mast)
	{
		SUpdateMastHp f(mast->GetHP());
		map->for_each(f);
	}
}

void CDungeon::RestoreMastPartialHP()
{
	LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(m_lMapIndex);
	if (!map)
	{
		sys_err("cannot find map by index %d", m_lMapIndex);
		return;
	}

	LPCHARACTER mast = GetMast();
	if (mast)
	{
		int64_t hp = GetMast()->GetHP();
		int32_t add = 600000;
		if (hp + add >= 12000000)
		{
			mast->SetHP(12000000);
		}
		else
		{
			mast->SetHP(hp + add);
		}

		SUpdateMastHp f(mast->GetHP());
		map->for_each(f);
	}
}
#endif

