#ifndef __INC_METIN_II_GAME_CHARACTER_MANAGER_H__
#define __INC_METIN_II_GAME_CHARACTER_MANAGER_H__

#include <entt/entity/entity.hpp>

#ifdef M2_USE_POOL
#include "pool.h"
#endif

#ifdef ENABLE_EVENT_MANAGER
#include "buffer_manager.h"
#endif

#include <common/stl.h>
#include <common/length.h>
#include <string_view>


class CDungeon;
class CHARACTER;
class CharacterVectorInteractor;

class CHARACTER_MANAGER : public singleton<CHARACTER_MANAGER>
{
#ifdef ENABLE_ITEMSHOP
public:
	void	LoadItemShopData(const char* c_pData);
	void	LoadItemShopData(entt::entity character, bool isAll = true);
	void	LoadItemShopLog(entt::entity character);
	void	LoadItemShopLogReal(entt::entity character, const char* c_pData);
	void	LoadItemShopBuy(entt::entity character, int itemID, int itemCount);
	bool GetItemShopDataByVnum(uint32_t vnum, TIShopData& outData) const;
	void	LoadItemShopBuyReal(entt::entity character, const char* c_pData);
	int		GetItemShopUpdateTime() { return itemshopUpdateTime; }

protected:
	int		itemshopUpdateTime;
	std::map<uint8_t, std::map<uint8_t, std::vector<TIShopData>>> m_IShopManager;
#endif

	public:
		typedef std::unordered_map<std::string, LPCHARACTER> NAME_MAP;

		CHARACTER_MANAGER();
		virtual ~CHARACTER_MANAGER();

		void                    Destroy();

		void			GracefulShutdown();	// 정상적 셧다운할 때 사용. PC를 모두 저장시키고 Destroy 한다.

		uint32_t			AllocVID();

		LPCHARACTER             CreateCharacter(const char * name, uint32_t dwPID = 0);
#ifndef DEBUG_ALLOC
		void DestroyCharacter(LPCHARACTER ch);
#else
		void DestroyCharacter(LPCHARACTER ch, const char* file, size_t line);
#endif

		void			Update(int iPulse);

		LPCHARACTER		SpawnMob(uint32_t dwVnum, int32_t lMapIndex, int32_t x, int32_t y, int32_t z, bool bSpawnMotion = false, int iRot = -1, bool bShow = true);
		LPCHARACTER		SpawnMobRange(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, bool bIsException=false, bool bSpawnMotion = false , bool bAggressive = false);
		LPCHARACTER		SpawnGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, LPREGEN pkRegen = nullptr, bool bAggressive_ = false, LPDUNGEON pDungeon = nullptr);
		bool			SpawnGroupGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, LPREGEN pkRegen = nullptr, bool bAggressive_ = false, LPDUNGEON pDungeon = nullptr);
		bool			SpawnMoveGroup(uint32_t dwVnum, int32_t lMapIndex, int sx, int sy, int ex, int ey, int tx, int ty, LPREGEN pkRegen = nullptr, bool bAggressive_ = false);
		LPCHARACTER		SpawnMobRandomPosition(uint32_t dwVnum, int32_t lMapIndex);

		void			SelectStone(LPCHARACTER pkChrStone);

		NAME_MAP &		GetPCMap() { return m_map_pkPCChr; }

		LPCHARACTER		Find(uint32_t dwVID);
		LPCHARACTER		FindPC(const char * name);
		LPCHARACTER		FindByPID(uint32_t dwPID);

		// Entity-returning counterparts. Prefer these where the caller only
		// needs the ECS handle; they keep the OOP->ECS conversion inside the
		// manager instead of at every call site.
		entt::entity		FindEntity(uint32_t dwVID);
		entt::entity		FindPCEntity(const char * name);
		entt::entity		FindEntityByPID(uint32_t dwPID);

		bool			AddToStateList(entt::entity character);
		void			RemoveFromStateList(entt::entity character);

		// DelayedSave: 어떠한 루틴 내에서 저장을 해야 할 짓을 많이 하면 저장
		// 쿼리가 너무 많아지므로 "저장을 한다" 라고 표시만 해두고 잠깐
		// (예: 1 frame) 후에 저장시킨다.
		void                    DelayedSave(LPCHARACTER ch);
		bool                    FlushDelayedSave(LPCHARACTER ch); // Delayed 리스트에 있다면 지우고 저장한다. 끊김 처리시 사용 됨.
		void			ProcessDelayedSave();

		template<class Func>	Func for_each_pc(Func f);

		void			RegisterForMonsterLog(entt::entity character);
		void			UnregisterForMonsterLog(entt::entity character);
		void			PacketMonsterLog(entt::entity character, const void* buf, int size);

		void			KillLog(uint32_t dwVnum);

		void			RegisterRaceNum(uint32_t dwVnum);
		void			RegisterRaceNumMap(LPCHARACTER ch);
		void			UnregisterRaceNumMap(LPCHARACTER ch);
		bool			GetCharactersByRaceNum(uint32_t dwRaceNum, CharacterVectorInteractor & i);

		LPCHARACTER		FindSpecifyPC(unsigned int uiJobFlag, int32_t lMapIndex, LPCHARACTER except= nullptr, int iMinLevel = 1, int iMaxLevel = PLAYER_MAX_LEVEL_CONST);

		void			SetMobItemRate(int value)	{ m_iMobItemRate = value; }
		void			SetMobDamageRate(int value)	{ m_iMobDamageRate = value; }
		void			SetMobGoldAmountRate(int value)	{ m_iMobGoldAmountRate = value; }
		void			SetMobGoldDropRate(int value)	{ m_iMobGoldDropRate = value; }
		void			SetMobExpRate(int value)	{ m_iMobExpRate = value; }

		void			SetMobItemRatePremium(int value)	{ m_iMobItemRatePremium = value; }
		void			SetMobGoldAmountRatePremium(int value)	{ m_iMobGoldAmountRatePremium = value; }
		void			SetMobGoldDropRatePremium(int value)	{ m_iMobGoldDropRatePremium = value; }
		void			SetMobExpRatePremium(int value)		{ m_iMobExpRatePremium = value; }

		void			SetUserDamageRatePremium(int value)	{ m_iUserDamageRatePremium = value; }
		void			SetUserDamageRate(int value ) { m_iUserDamageRate = value; }
				int			GetMobItemRate(entt::entity character);
		int			GetMobDamageRate(entt::entity character);
		int			GetMobGoldAmountRate(entt::entity character);
		int			GetMobGoldDropRate(entt::entity character);
		int			GetMobExpRate(entt::entity character);

		int			GetUserDamageRate(entt::entity character);
		void		SendScriptToMap(int32_t lMapIndex, std::string_view s);

		bool			BeginPendingDestroy();
		void			FlushPendingDestroy();
#ifdef ENABLE_EVENT_MANAGER
	public:
		void			ClearEventData();
		bool			CloseEventManuel(uint8_t eventIndex);
		void			SetEventData(uint8_t dayIndex, const std::vector<TEventManagerData>& m_data);
		void			SetEventStatus(const uint16_t eventID, const bool eventStatus, const int endTime, const char* endTimeText);
		void			SendDataPlayer(entt::entity character);
		void			CheckBonusEvent(entt::entity character);
		void			UpdateAllPlayerEventData();
		void			CompareEventSendData(TEMP_BUFFER* buf);
		const TEventManagerData* CheckEventIsActive(uint8_t eventIndex, uint8_t empireIndex = 0);
		void			CheckEventForDrop(entt::entity character, entt::entity killer, std::vector<entt::entity>& vec_item);
	protected:
		std::map<uint8_t, std::vector<TEventManagerData>>	m_eventData;
#endif
		

	private:
		int					m_iMobItemRate;
		int					m_iMobDamageRate;
		int					m_iMobGoldAmountRate;
		int					m_iMobGoldDropRate;
		int					m_iMobExpRate;

		int					m_iMobItemRatePremium;
		int					m_iMobGoldAmountRatePremium;
		int					m_iMobGoldDropRatePremium;
		int					m_iMobExpRatePremium;

		int					m_iUserDamageRate;
		int					m_iUserDamageRatePremium;
		uint32_t				m_iVIDCount;

		std::unordered_map<uint32_t, LPCHARACTER> m_map_pkChrByVID;
		std::unordered_map<uint32_t, LPCHARACTER> m_map_pkChrByPID;
		NAME_MAP			m_map_pkPCChr;

		char				dummy1[1024];	// memory barrier
		// Membership only: the update pump calls AISystem::UpdateStateMachine
		// with the entity, nothing here dereferences a character.
		std::unordered_set<entt::entity>	m_set_pkChrState;	// FSM이 돌아가고 있는 놈들
		CHARACTER_SET		m_set_pkChrForDelayedSave;
		// Membership only: PacketMonsterLog reads position and descriptor off
		// the entity.
		std::unordered_set<entt::entity>	m_set_pkChrMonsterLog;

		LPCHARACTER			m_pkChrSelectedStone;

		std::map<uint32_t, uint32_t> m_map_dwMobKillCount;

		std::set<uint32_t>		m_set_dwRegisteredRaceNum;
		std::map<uint32_t, CHARACTER_SET> m_map_pkChrByRaceNum;

		bool				m_bUsePendingDestroy;
		CHARACTER_SET		m_set_pkChrPendingDestroy;

#ifdef M2_USE_POOL
		ObjectPool<CHARACTER> pool_;
#endif
};

	template<class Func>
Func CHARACTER_MANAGER::for_each_pc(Func f)
{
	for (auto it = m_map_pkChrByPID.begin(); it != m_map_pkChrByPID.end(); ++it)
		f(it->second);

	return f;
}

class CharacterVectorInteractor : public CHARACTER_VECTOR
{
	public:
		CharacterVectorInteractor() : m_bMyBegin(false) { }

		CharacterVectorInteractor(const CHARACTER_SET & r);
		virtual ~CharacterVectorInteractor();

	private:
		bool m_bMyBegin;
};

#ifndef DEBUG_ALLOC
#define M2_DESTROY_CHARACTER(ptr) CHARACTER_MANAGER::instance().DestroyCharacter(ptr)
#else
#define M2_DESTROY_CHARACTER(ptr) CHARACTER_MANAGER::instance().DestroyCharacter(ptr, __FILE__, __LINE__)
#endif

#endif
