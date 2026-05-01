#ifndef __INC_METIN_II_GAME_DUNGEON_H
#define __INC_METIN_II_GAME_DUNGEON_H

#include "sectree_manager.h"

class CParty;

class CDungeon
{
	typedef std::unordered_map<LPPARTY, int> TPartyMap;
	typedef std::map<std::string, LPCHARACTER> TUniqueMobMap;

	public:
	// <Factor> Non-persistent identifier type
	typedef uint32_t IdType;

	~CDungeon();

	// <Factor>
	IdType GetId() const { return m_id; }

	void	Notice(
#ifdef TEXTS_IMPROVEMENT
	uint32_t idx,
#endif
	const char* msg
#ifdef TEXTS_IMPROVEMENT
	, bool big = false
#endif
	);

	void CmdChat(const char* msg);

	void	JoinParty(LPPARTY pParty);
	void	QuitParty(LPPARTY pParty);

	void	Join(LPCHARACTER ch);

	void	IncMember(LPCHARACTER ch);
	void	DecMember(LPCHARACTER ch);
	void	JoinParty_Coords(LPPARTY pParty, int32_t X, int32_t Y, int32_t index);
	void	Join_Coords(LPCHARACTER ch, int32_t X, int32_t Y, int32_t index);

	// DUNGEON_KILL_ALL_BUG_FIX
	void	Purge();
	void KillAll();
	void KillAllMonsters();
#ifdef __DEFENSE_WAVE__
	void KillAllMonstersHydra();
#endif
	// END_OF_DUNGEON_KILL_ALL_BUG_FIX

	void IncMonster() { m_monstercount++; }
	void DecMonster() { if (m_monstercount == 0) { return; } m_monstercount--; }
	int32_t CountMonster() { return m_monstercount; }

	void	IncPartyMember(LPPARTY pParty, LPCHARACTER ch);
	void	DecPartyMember(LPPARTY pParty, LPCHARACTER ch);

	int	GetKillMobCount();
	int	GetKillStoneCount();
	int32_t	GetMapIndex() { return m_lMapIndex; }

	LPCHARACTER SpawnMob(int32_t vnum, int32_t x, int32_t y, int32_t dir = 0);

	void	SpawnRegen(const char* filename, bool once = true);
	void	AddRegen(LPREGEN regen);
	void	ClearRegen();
	bool	IsValidRegen(LPREGEN regen, size_t regen_id);

	void	SetUnique(const char* key, uint32_t vid);
	void KillUnique(std::string_view key);
	bool	IsUniqueDead(std::string_view key);
	int32_t GetUniqueVid(std::string_view key);

	void	DeadCharacter(LPCHARACTER ch);

	void	JumpAll(int32_t idx, int32_t x, int32_t y);

	void ExitAllLobby(uint8_t lobby);

	void	ExitAllToStartPosition();
	void	JumpToEliminateLocation();
	void	SetExitAllAtEliminate(int32_t time);

	int GetFlag(std::string name);
	void SetFlag(std::string name, int32_t value);
	void	SetWarpLocation (int32_t map_index, int x, int y);

	// item group은 item_vnum과 item_count로 구성.
	typedef std::vector <std::pair <uint32_t, int> > ItemGroup;
	//void	InsertItemGroup (std::string& group_name, uint32_t item_vnum);

	template <class Func> Func ForEachMember(Func f);

	protected:
	CDungeon(IdType id, int32_t lOriginalMapIndex, int32_t lMapIndex);

	void	Initialize();
	void	CheckDestroy();

	private:
	bool m_completed;
	IdType 		m_id; // <Factor>
	int32_t		m_lOrigMapIndex;
	int32_t		m_lMapIndex;

	CHARACTER_SET	    m_set_pkCharacter;
	std::map<std::string, int>  m_map_Flag;
	TPartyMap	m_map_pkParty;
	TAreaMap&	m_map_Area;
		TUniqueMobMap m_map_UniqueMob;

		int32_t m_monstercount;

	// 적 전멸시 워프하는 위치
	int		m_iWarpDelay;
	int32_t		m_lWarpMapIndex;
	int32_t		m_lWarpX;
	int32_t		m_lWarpY;
	std::string	m_stRegenFile;

	std::vector<LPREGEN> m_regen;

	LPEVENT		deadEvent;
	// <Factor>
	LPEVENT exit_all_event_;
	LPEVENT jump_to_event_;
	size_t regen_id_;

	friend class CDungeonManager;
	friend EVENTFUNC(dungeon_dead_event);
	// <Factor>
	friend EVENTFUNC(dungeon_jump_to_event);

	// 파티 단위 던전 입장을 위한 임시 변수.
	// m_map_pkParty는 관리가 부실하여 사용할 수 없다고 판단하여,
	// 임시로 한 파티에 대한 관리를 하는 변수 생성.

	LPPARTY m_pParty;

	public :
	void SetPartyNull();
#ifdef __DEFENSE_WAVE__
	public:
		LPCHARACTER GetMast() { return m_Mast; }
		void SetMast(LPCHARACTER Mast) { m_Mast = Mast; }
		void UpdateMastHP();
		void RestoreMastPartialHP();

	protected:
		LPCHARACTER m_Mast;
#endif
};

class CDungeonManager : public singleton<CDungeonManager>
{
	typedef std::map<CDungeon::IdType, LPDUNGEON> TDungeonMap;
	typedef std::map<int32_t, LPDUNGEON> TMapDungeon;

	public:
	CDungeonManager();
	virtual ~CDungeonManager();

	LPDUNGEON	Create(int32_t lOriginalMapIndex);
	void		Destroy(CDungeon::IdType dungeon_id);
	LPDUNGEON	Find(CDungeon::IdType dungeon_id);
	LPDUNGEON	FindByMapIndex(int32_t lMapIndex);

	private:
	TDungeonMap	m_map_pkDungeon;
	TMapDungeon m_map_pkMapDungeon;

	// <Factor> Introduced unsigned 32-bit dungeon identifier
	CDungeon::IdType next_id_;
};
#ifdef ENABLE_DUNGEON_SHARED_DROP_HWID
template <class Func> Func CDungeon::ForEachMember(Func f)
{
	for (auto it = m_set_pkCharacter.begin(); it != m_set_pkCharacter.end(); ++it)
		f(*it);
	return f;
}

#endif
#endif
