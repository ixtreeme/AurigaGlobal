#pragma once
#include <boost/unordered_map.hpp>

#include <common/stl.h>
#include <common/length.h>
#include <common/tables.h>
#include "group_text_parse_tree.h"

#include "packet.h"

class CBattlePass : public singleton<CBattlePass>
{
	public:
		CBattlePass();
		virtual ~CBattlePass();
		
		bool ReadBattlePassFile();
		bool ReadBattlePassGroup();
#ifdef ENABLE_BATTLE_PASS_RELOAD
		bool ReadBattlePassMissions(bool isReloading = false);
#else
		bool ReadBattlePassMissions();
#endif
	
		uint8_t GetMissionTypeByName(std::string stMissionName);
		std::string GetMissionNameByType(uint8_t bType);
		std::string GetBattlePassNameByID(uint8_t bID);
		
		void GetMissionSearchName(uint8_t bMissionType, std::string*, std::string*);
		
		void BattlePassRequestOpen(LPCHARACTER pkChar);
		bool BattlePassMissionGetInfo(uint8_t bBattlePassId, uint8_t bMissionType, uint32_t* dwFirstInfo, uint32_t* dwSecondInfo);
		void BattlePassRewardMission(LPCHARACTER pkChar, uint32_t bMissionType, uint32_t bBattlePassId);

		void BattlePassRequestReward(LPCHARACTER pkChar);
		void BattlePassReward(LPCHARACTER pkChar);
		
#ifdef ENABLE_BATTLE_PASS_SECURITY_KILL
		bool IsEligibleForPlayerKill(uint32_t dwKillerID, uint32_t dwPlayerID);
		void RegisterPlayerKill(uint32_t dwKillerID, uint32_t dwPlayerID);
#endif

	private:
		CGroupTextParseTreeLoader* m_pLoader;
		
		typedef std::map <uint8_t, std::string> TMapBattlePassName;
		typedef std::map <std::string, std::vector<TBattlePassRewardItem>> TMapBattlePassReward;
		typedef std::map <std::string, std::vector<TBattlePassMissionInfo>> TMapBattleMissionInfo;
#ifdef ENABLE_BATTLE_PASS_SECURITY_KILL	
		typedef std::map <uint32_t, std::vector<TBattlePassKillVictim *>> TKillMap;
#endif

		TMapBattlePassName m_map_battle_pass_name;
		TMapBattlePassReward m_map_battle_pass_reward;
		TMapBattleMissionInfo m_map_battle_pass_mission_info;
		
#ifdef ENABLE_BATTLE_PASS_SECURITY_KILL	
		TKillMap m_playersKills;
#endif
};