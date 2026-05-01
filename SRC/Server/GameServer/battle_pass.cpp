#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "battle_pass.h"

#include "p2p.h"
#include "locale_service.h"
#include "char_interface.hpp"
#include "desc_client.h"
#include "desc_manager.h"
#include "buffer_manager.h"
#include "packet.h"
#include "questmanager.h"
#include "questlua.h"
#include "start_position.h"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "item_manager.h"
#include "sectree_manager.h"
#include "regen.h"
#include "log.h"
#include "db.h"
#include "target.h"
#include "party.h"

#include <string>
#include <algorithm>
#include <array>
#include <optional>
#include <ctime>

namespace
{
	// Battle Pass vegso jutalom duplazas elleni vedelem.
	// A flag neve honaphoz van kotve (YYYYMM), igy automatikusan resetel uj honapnal.
	static uint32_t GetBattlePassCycleKeyYYYYMM()
	{
		time_t t = get_global_time();
		struct tm now = *localtime(&t);
		return (uint32_t)((now.tm_year + 1900) * 100 + (now.tm_mon + 1));
	}

	static std::string GetBattlePassFinalRewardFlag(uint8_t bBattlePassId)
	{
		char buf[96];
		snprintf(buf, sizeof(buf), "battle_pass.final_reward.%u.%u", (unsigned)bBattlePassId, (unsigned)GetBattlePassCycleKeyYYYYMM());
		return std::string(buf);
	}

	static bool IsBattlePassFinalRewardTaken(LPCHARACTER ch, uint8_t bBattlePassId)
	{
		if (!ch)
			return false;

		quest::PC* pc = quest::CQuestManager::instance().GetPCForce((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));
		if (!pc)
			return false;

		const std::string flag = GetBattlePassFinalRewardFlag(bBattlePassId);
		return pc->GetFlag(flag) != 0;
	}

	static void SetBattlePassFinalRewardTaken(LPCHARACTER ch, uint8_t bBattlePassId)
	{
		if (!ch)
			return;

		quest::PC* pc = quest::CQuestManager::instance().GetPCForce((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));
		if (!pc)
			return;

		const std::string flag = GetBattlePassFinalRewardFlag(bBattlePassId);
		pc->SetFlag(flag, 1);
	}
}


const std::string g_astMissionType[MISSION_TYPE_MAX] = {
	"",
	"MONSTER_KILL",
	"PLAYER_KILL",
	"MONSTER_DAMAGE",
	"PLAYER_DAMAGE",
	"USE_ITEM",
	"SELL_ITEM",
	"CRAFT_ITEM",
	"REFINE_ITEM",
	"DESTROY_ITEM",
	"COLLECT_ITEM",
	"FRY_FISH",
	"CATCH_FISH",
	"SPENT_YANG",
	"FARM_YANG",
	"COMPLETE_DUNGEON",
	"COMPLETE_MINIGAME",
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
	"STAY_ONLINE_MINUTES",
#endif
#ifdef ENABLE_BATTLE_PASS_CHAT_CNT
	"COUNTER_CHAT",
#endif
	"COLLECT_ITEM1",
	"COLLECT_ITEM2",
	"USE_ITEM1",
	"USE_ITEM2",
};

CBattlePass::CBattlePass()
{
	m_pLoader = nullptr;
}

CBattlePass::~CBattlePass()
{
	if (m_pLoader)
		delete m_pLoader;
}

bool CBattlePass::ReadBattlePassFile()
{
	char szBattlePassFileName[256];
	snprintf(szBattlePassFileName, sizeof(szBattlePassFileName),"%s/battle_pass.txt", LocaleService_GetBasePath().c_str());
			
	m_pLoader = new CGroupTextParseTreeLoader;
	CGroupTextParseTreeLoader& loader = *m_pLoader;

	if (false == loader.Load(szBattlePassFileName))
	{
		LOG_ERROR("battle_pass.txt load error");
		return false;
	}

	if (!ReadBattlePassGroup())
		return false;
	
	if (!ReadBattlePassMissions())
		return false;
	
	return true;
}

bool CBattlePass::ReadBattlePassGroup()
{
	std::string stName;

	CGroupNode* pGroupNode = m_pLoader->GetGroup("battlepass");

	if (nullptr == pGroupNode)
	{
		LOG_ERROR("battle_pass.txt need BattlePass group.");
		return false;
	}

	int n = pGroupNode->GetRowCount();
	if (0 == n)
	{
		LOG_ERROR("Group BattlePass is Empty.");
		return false;
	}

	std::set<uint8_t> setIDs;

	for (int i = 0; i < n; i++)
	{
		const CGroupNode::CGroupNodeRow* pRow;
		pGroupNode->GetRow(i, &pRow);

		std::string stBattlePassName;
		uint8_t battlePassId;
		
		if (!pRow->GetValue("battlepassname", stBattlePassName))
		{
			LOG_ERROR("In Group BattlePass, No BattlePassName column.");
			return false;
		}
		
		if (!pRow->GetValue("battlepassid", battlePassId))
		{
			LOG_ERROR("In Group BattlePass, {}'s ID is invalid", stBattlePassName.c_str());
			return false;
		}

		if (setIDs.contains(battlePassId))
		{
			LOG_ERROR("In Group BattlePass, duplicated id exist.");
			return false;
		}
		
		setIDs.insert(battlePassId);

		m_map_battle_pass_name.insert(TMapBattlePassName::value_type(battlePassId, stBattlePassName));
	}
	
	return true;
}
//#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
//void CHARACTER::BattlePass_StayOnlineTick()
//{
//	if (!GetDesc())
//		return;
//
//	const uint8_t bBattlePassId = GetBattlePassId();
//	if (!bBattlePassId)
//		return;
//
//	// 60 mp-
//	const uint32_t now = get_dword_time();
//	if (now < m_dwBattlePassStayOnlineNextTick)
//		return;
//
//	m_dwBattlePassStayOnlineNextTick = now + 60 * 1000;
//
//	uint32_t dwNotUsed = 0, dwCount = 0;
//	if (!CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, STAY_ONLINE_MINUTES, &dwNotUsed, &dwCount))
//		return;
//
//	if (IsCompletedMission(STAY_ONLINE_MINUTES))
//		return;
//
//	if (GetMissionProgress(STAY_ONLINE_MINUTES, bBattlePassId) >= dwCount)
//		return;
//
//
//	UpdateMissionProgress(STAY_ONLINE_MINUTES, bBattlePassId, 1, dwCount);
//}
//#endif

#ifdef ENABLE_BATTLE_PASS_RELOAD
bool CBattlePass::ReadBattlePassMissions(bool isReloading)
#else
bool CBattlePass::ReadBattlePassMissions()
#endif
{
#ifdef ENABLE_BATTLE_PASS_RELOAD
	if (isReloading)
	{
		m_map_battle_pass_reward.clear();
		m_map_battle_pass_mission_info.clear();
	}
#endif

	auto it = m_map_battle_pass_name.begin();
	while (it != m_map_battle_pass_name.end())
	{
		std::string battlePassName = it++->second;
		
		CGroupNode* pGroupNode = m_pLoader->GetGroup(battlePassName.c_str());
	
		if (nullptr == pGroupNode)
		{
			LOG_ERROR("battle_pass.txt need group {}.", battlePassName.c_str());
			return false;
		}
		
		int n = pGroupNode->GetChildNodeCount();
		if (n < 2)
		{
			LOG_ERROR("Group {} need to have at least one grup for Reward and one Mission. Row: {}", battlePassName.c_str(), n);
			return false;
		}
		
		{
			CGroupNode* pChild;
			if (nullptr == (pChild = pGroupNode->GetChildNode("reward")))
			{
				LOG_ERROR("In Group {}, Reward group is not defined.", battlePassName.c_str());
				return false;
			}
			
			int m = pChild->GetRowCount();
			std::vector<TBattlePassRewardItem> rewardVector;

			for (int j = 1; j <= m; j++)
			{
				std::stringstream ss;
				ss << j;
				const CGroupNode::CGroupNodeRow* pRow = nullptr;
	
				pChild->GetRow(ss.str(), &pRow);
				if (nullptr == pRow)
				{
					LOG_ERROR("In Group {}, subgroup Reward, No {} row.", battlePassName.c_str(), j);
					return false;
				}
				
				TBattlePassRewardItem itemReward;

				if (!pRow->GetValue("itemvnum", itemReward.dwVnum))
				{
					LOG_ERROR("In Group {}, subgroup Reward, ItemVnum is empty.", battlePassName.c_str());
					return false;
				}
				
				if (!pRow->GetValue("itemcount", itemReward.bCount))
				{
					LOG_ERROR("In Group {}, subgroup Reward, ItemCount is empty.", battlePassName.c_str());
					return false;
				}
				
				rewardVector.push_back(itemReward);
			}
			
			m_map_battle_pass_reward.insert(TMapBattlePassReward::value_type(battlePassName.c_str(), rewardVector));
		}
		
		std::vector<TBattlePassMissionInfo> missionInfoVector;

		std::array<bool, 3> useItemSlots = { false, false, false };
		std::array<bool, 3> collectItemSlots = { false, false, false };
		
		for (int i = 1; i < n; i++)
		{
			std::stringstream ss;
			ss << "mission_" << i;

			CGroupNode* pChild;
			if (nullptr == (pChild = pGroupNode->GetChildNode(ss.str())))
			{
				LOG_ERROR("In Group {}, {} subgroup is not defined.", battlePassName.c_str(), ss.str().c_str());
				return false;
			}
			
			int m = pChild->GetRowCount();
			
			std::string stMissionSearch[] = {"", ""};
			bool bAlreadySearched = false;
			uint8_t bRewardContor = 0;
			TBattlePassMissionInfo missionInfo = {};

			for (int j = 0; j < m; j++)
			{
				const CGroupNode::CGroupNodeRow* pRow = nullptr;
	
				pChild->GetRow(j, &pRow);
				if (nullptr == pRow)
				{
					LOG_ERROR("In Group {} and subgroup {} null row.", battlePassName.c_str(), ss.str().c_str());
					return false;
				}
				
				// InfoDesc = ItemVnum from reward
				// InfoName = ItemCount from reward

				std::string stInfoDesc;
				if (!pRow->GetValue("infodesc", stInfoDesc))
				{
					LOG_ERROR("In Group {} and subgroup {} InfoDesc does not exist.", battlePassName.c_str(), ss.str().c_str());
					return false;
				}
				
				if(stInfoDesc == "type")
				{
					std::string stInfoName;
					if (!pRow->GetValue("infoname", stInfoName))
					{
						LOG_ERROR("In Group {} and subgroup {} InfoName does not exist.", battlePassName.c_str(), ss.str().c_str());
						return false;
					}
					
					missionInfo.bMissionType = GetMissionTypeByName(stInfoName);
				}
				
				if(missionInfo.bMissionType <= MISSION_TYPE_NONE || missionInfo.bMissionType >= MISSION_TYPE_MAX)
				{
					LOG_ERROR("In Group {} and subgroup {} Wrong mission type: {}.", battlePassName.c_str(), ss.str().c_str(), static_cast<int>(missionInfo.bMissionType));
					return false;
				}
				
				if(!bAlreadySearched)
				{
					GetMissionSearchName(missionInfo.bMissionType, &stMissionSearch[0], &stMissionSearch[1]);
					bAlreadySearched = true;
				}
				
				for(int k = 0; k < 2; k++)
				{
					if(stMissionSearch[k] != "")
					{
						if(stInfoDesc == stMissionSearch[k])
						{
							if (!pRow->GetValue("infoname", missionInfo.dwMissionInfo[k]))
							{
								LOG_ERROR("In Group {} and subgroup {} InfoDesc {} InfoName does not exist.", battlePassName.c_str(), ss.str().c_str(), stMissionSearch[k].c_str());
								return false;
							}
							
							LOG_TRACE("BattlePassInfo: Group {} // Subgroup {} // InfoName {} // InfoValue {}", battlePassName.c_str(), ss.str().c_str(), stMissionSearch[k].c_str(), missionInfo.dwMissionInfo[k]);
							
							stMissionSearch[k] = "";
						}
					}
				}
				
				if(bRewardContor >= MISSION_REWARD_COUNT)
				{
					LOG_ERROR("In Group {} and subgroup {} More than 3 rewards.", battlePassName.c_str(), ss.str().c_str());
					return false;
				}
				
				if(isdigit(*stInfoDesc.c_str()))
				{
					uint32_t dwVnum = atoi(stInfoDesc.c_str());
					uint8_t bCount = 1;
		
					if (!pRow->GetValue("infoname", bCount))
					{
						LOG_ERROR("In Group {} and subgroup {} Wrong ItemCount.", battlePassName.c_str(), ss.str().c_str());
						return false;
					}
							
					missionInfo.aRewardList[bRewardContor].dwVnum = dwVnum;
					missionInfo.aRewardList[bRewardContor].bCount = bCount;
					bRewardContor++;
				}
			}

			auto assignMultiSlot = [](uint8_t requestedType,
				const std::array<uint8_t, 3>& slots,
				std::array<bool, 3>& usedSlots) -> std::optional<uint8_t> {
					if (requestedType == slots[0])
					{
						for (size_t idx = 0; idx < usedSlots.size(); idx++)
						{
							if (!usedSlots[idx])
							{
								usedSlots[idx] = true;
								return slots[idx];
							}
						}
						return std::nullopt;
					}

					if (requestedType == slots[1])
					{
						if (usedSlots[1])
							return std::nullopt;
						usedSlots[1] = true;
						return slots[1];
					}

					if (requestedType == slots[2])
					{
						if (usedSlots[2])
							return std::nullopt;
						usedSlots[2] = true;
						return slots[2];
					}

					return requestedType;
				};

			if (missionInfo.bMissionType == USE_ITEM || missionInfo.bMissionType == USE_ITEM1 || missionInfo.bMissionType == USE_ITEM2)
			{
				const std::array<uint8_t, 3> slots = { USE_ITEM, USE_ITEM1, USE_ITEM2 };
				const auto assignedType = assignMultiSlot(missionInfo.bMissionType, slots, useItemSlots);
				if (!assignedType.has_value())
				{
					LOG_ERROR("In Group {} and subgroup {} Too many USE_ITEM missions (max 3).", battlePassName.c_str(), ss.str().c_str());
					return false;
				}
				missionInfo.bMissionType = assignedType.value();
			}
			else if (missionInfo.bMissionType == COLLECT_ITEM || missionInfo.bMissionType == COLLECT_ITEM1 || missionInfo.bMissionType == COLLECT_ITEM2)
			{
				const std::array<uint8_t, 3> slots = { COLLECT_ITEM, COLLECT_ITEM1, COLLECT_ITEM2 };
				const auto assignedType = assignMultiSlot(missionInfo.bMissionType, slots, collectItemSlots);
				if (!assignedType.has_value())
				{
					LOG_ERROR("In Group {} and subgroup {} Too many COLLECT_ITEM missions (max 3).", battlePassName.c_str(), ss.str().c_str());
					return false;
				}
				missionInfo.bMissionType = assignedType.value();
			}
			
			missionInfoVector.push_back(missionInfo);
		}
		
		m_map_battle_pass_mission_info.insert(TMapBattleMissionInfo::value_type(battlePassName.c_str(), missionInfoVector));
	}

	return true;
}

uint8_t CBattlePass::GetMissionTypeByName(std::string stMissionName)
{
	for(int i = 0; i < MISSION_TYPE_MAX; i++)
	{
		if(g_astMissionType[i] == stMissionName)
			return i;
	}
	return 0;
}

std::string CBattlePass::GetMissionNameByType(uint8_t bType)
{
	for(int i = 0; i < MISSION_TYPE_MAX; i++)
	{
		if(i == bType)
			return g_astMissionType[i];
	}
	
	return "";
}

std::string CBattlePass::GetBattlePassNameByID(uint8_t bID)
{
	const auto it = m_map_battle_pass_name.find(bID);
	
	if(it == m_map_battle_pass_name.end())
	{
		return "";
	}
	
	return it->second;
}

void CBattlePass::GetMissionSearchName(uint8_t bMissionType, std::string * st_name_1, std::string * st_name_2)
{
	switch(bMissionType)
	{
		case MONSTER_KILL:
		case USE_ITEM:
		case USE_ITEM1:
		case USE_ITEM2:
		case SELL_ITEM:
		case CRAFT_ITEM:
		case REFINE_ITEM:
		case DESTROY_ITEM:
		case COLLECT_ITEM:
		case COLLECT_ITEM1:
		case COLLECT_ITEM2:
			*st_name_1 = "vnum";
			*st_name_2 = "count";
			break;

		case PLAYER_KILL:
			*st_name_1 = "min_level";
			*st_name_2 = "count";
			break;
			
		case MONSTER_DAMAGE:
			*st_name_1 = "vnum";
			*st_name_2 = "value";
			break;
			
		case PLAYER_DAMAGE:
			*st_name_1 = "min_level";
			*st_name_2 = "value";
			break;
			
		case FRY_FISH:
		case CATCH_FISH:
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
		case STAY_ONLINE_MINUTES:
#endif
#ifdef ENABLE_BATTLE_PASS_CHAT_CNT
		case COUNTER_CHAT:
#endif
			*st_name_1 = "";
			*st_name_2 = "count";
			break;
			
		case SPENT_YANG:
		case FARM_YANG:
			*st_name_1 = "";
			*st_name_2 = "value";
			break;	
			
		case COMPLETE_DUNGEON:
		case COMPLETE_MINIGAME:
			*st_name_1 = "id";
			*st_name_2 = "count";
			break;	
	
		
		
		default:
			*st_name_1 = "";
			*st_name_2 = "";
			break;
	}
}

void CBattlePass::BattlePassRequestOpen(LPCHARACTER pkChar)
{
	if(!pkChar)
		return;
	
	if(!ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChar)))
		return;
	
	if(!pkChar->IsLoadedBattlePass())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, 776, "");
#endif
		return;
	}

	uint8_t bBattlePassId = pkChar->GetBattlePassId();
	uint8_t fakeBattlePassID = 1; // (can be return actual month)
	
	// So if there is no active battlepass, we can't send data info,
	// but we do a fake id to send the infos else we continue as how until now.
	if (!bBattlePassId)
		bBattlePassId = fakeBattlePassID;

	TMapBattlePassName::iterator it = m_map_battle_pass_name.find(bBattlePassId);
	
	if(it == m_map_battle_pass_name.end())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, 777, "%d", bBattlePassId);
#endif
		return;
	}
	
	std::string battlePassName = it->second;
	TMapBattleMissionInfo::iterator itInfo = m_map_battle_pass_mission_info.find(battlePassName);
	
	if(itInfo == m_map_battle_pass_mission_info.end())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, 778, "%s", battlePassName.c_str());
#endif
		return;
	}
	
	TMapBattlePassReward::iterator itReward = m_map_battle_pass_reward.find(battlePassName);
	if(itReward == m_map_battle_pass_reward.end())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, 779, "%s", battlePassName.c_str());
#endif
		return;
	}
	
	std::vector<TBattlePassRewardItem> rewardInfo = itReward->second;
	std::vector<TBattlePassMissionInfo> missionInfo = itInfo->second;
	
#ifdef ENABLE_FREE_PASS_RAZOR93
	for (unsigned int i = 0; i < missionInfo.size(); i++)
	{
		missionInfo[i].dwMissionInfo[2] = pkChar->GetMissionProgress(missionInfo[i].bMissionType, bBattlePassId);

		// BOOST: a kliens fel� is felezett total menjen
		missionInfo[i].dwMissionInfo[1] = pkChar->GetBattlePassAdjustedTotal(
			missionInfo[i].bMissionType, bBattlePassId, missionInfo[i].dwMissionInfo[1]
		);

		// ha progress nagyobb lett, clamp
		if (missionInfo[i].dwMissionInfo[2] > missionInfo[i].dwMissionInfo[1])
			missionInfo[i].dwMissionInfo[2] = missionInfo[i].dwMissionInfo[1];
	}
#else

	for (unsigned int i = 0; i < missionInfo.size(); i++)
	{
		missionInfo[i].dwMissionInfo[2] = pkChar->GetMissionProgress(missionInfo[i].bMissionType, bBattlePassId);
	}
#endif
	if(!missionInfo.empty())
	{
		TPacketGCBattlePass packet;
		packet.bHeader = HEADER_GC_BATTLE_PASS_OPEN;
		packet.wSize = sizeof(packet) + sizeof(TBattlePassMissionInfo) * missionInfo.size();
		packet.wRewardSize = sizeof(TBattlePassRewardItem) * rewardInfo.size();

		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChar))->BufferedPacket(&packet, sizeof(packet));
		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChar))->BufferedPacket(missionInfo.data(), sizeof(TBattlePassMissionInfo) * missionInfo.size());
		ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChar))->Packet(rewardInfo.data(), sizeof(TBattlePassRewardItem) * rewardInfo.size());
	}
}

void CBattlePass::BattlePassRewardMission(LPCHARACTER pkChar, uint32_t bMissionType, uint32_t bBattlePassId)
{
	if(!pkChar)
		return;
	
	if(!ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChar)))
		return;

	if (!bBattlePassId)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, 780, "");
#endif
		return;
	}

	auto it = m_map_battle_pass_name.find(bBattlePassId);
	
	if(it == m_map_battle_pass_name.end())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, 777, "%d", bBattlePassId);
#endif
		return;
	}
	
	std::string battlePassName = it->second;
	TMapBattleMissionInfo::iterator itInfo = m_map_battle_pass_mission_info.find(battlePassName);
	
	if(itInfo == m_map_battle_pass_mission_info.end())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, 778, "%d", battlePassName.c_str());
#endif
		return;
	}
	
	std::vector<TBattlePassMissionInfo> missionInfo = itInfo->second;
	
	for (unsigned int i = 0; i < missionInfo.size(); i++)
	{
		if(missionInfo[i].bMissionType == bMissionType)
		{
			for(int j = 0; j < MISSION_REWARD_COUNT; j++)
			{
				if(missionInfo[i].aRewardList[j].dwVnum && missionInfo[i].aRewardList[j].bCount > 0)
					pkChar->AutoGiveItem(missionInfo[i].aRewardList[j].dwVnum, missionInfo[i].aRewardList[j].bCount);
			}
			
			break;
		}
	}
}

void CBattlePass::BattlePassRequestReward(LPCHARACTER pkChar)
{
	if(!pkChar)
		return;
	
	if(!ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChar)))
		return;
	
	uint8_t bBattlePassId = pkChar->GetBattlePassId();
	if (!bBattlePassId)
		return;

	// mar atvette a vegso jutalmat ebben a ciklusban (honap)
	if (IsBattlePassFinalRewardTaken(pkChar, bBattlePassId))
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, "A Battle Pass vegso jutalmat ebben a honapban mar atvetted.");
		return;
	}
	
	TMapBattlePassName::iterator it = m_map_battle_pass_name.find(bBattlePassId);
	if(it == m_map_battle_pass_name.end())
		return;
	
	std::string battlePassName = it->second;
	TMapBattleMissionInfo::iterator itInfo = m_map_battle_pass_mission_info.find(battlePassName);
	
	if(itInfo == m_map_battle_pass_mission_info.end())
		return;
	
	std::vector<TBattlePassMissionInfo> missionInfo = itInfo->second;
	
	bool bIsCompleted = true;
	for (unsigned int i = 0; i < missionInfo.size(); i++)
	{
		if(!pkChar->IsCompletedMission(missionInfo[i].bMissionType))
		{
			bIsCompleted = false;
			break;
		}
	}
	
	if(bIsCompleted)
	{
#ifdef TEXTS_IMPROVEMENT
		BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 548, "%s", ((pkChar)->GetName()), battlePassName.c_str());
#endif
		BattlePassReward(pkChar);
	}
}

void CBattlePass::BattlePassReward(LPCHARACTER pkChar)
{
	if(!pkChar)
		return;
	
	if(!ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkChar)))
		return;

	uint8_t bBattlePassId = pkChar->GetBattlePassId();
	if (!bBattlePassId)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, 780, "");
#endif
		return;
	}
	
	// plusz vedelem: ujra login / karaktervaltas utan se tudja ujra felvenni
	if (IsBattlePassFinalRewardTaken(pkChar, bBattlePassId))
	{
		ecs::ChatSystem::Send(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, "A Battle Pass vegso jutalmat ebben a honapban mar atvetted.");
		return;
	}

	TMapBattlePassName::iterator it = m_map_battle_pass_name.find(bBattlePassId);
	
	if(it == m_map_battle_pass_name.end())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, 777, "%d", bBattlePassId);
#endif
		return;
	}
	
	std::string battlePassName = it->second;
	TMapBattlePassReward::iterator itReward = m_map_battle_pass_reward.find(battlePassName);
	if(itReward == m_map_battle_pass_reward.end())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChar), CHAT_TYPE_INFO, 778, "%d", battlePassName.c_str());
#endif
		return;
	}
	
	AffectSystem::RemoveAffect(AIHelpers::EcsOf(pkChar), AFFECT_BATTLE_PASS);
	
	std::vector<TBattlePassRewardItem> rewardInfo = itReward->second;	
	
	for (unsigned int i = 0; i < rewardInfo.size(); i++)
	{
		pkChar->AutoGiveItem(rewardInfo[i].dwVnum, rewardInfo[i].bCount);
	}
	
	TBattlePassRegisterRanking ranking;
	ranking.bBattlePassId = bBattlePassId;
	strlcpy(ranking.playerName, ((pkChar)->GetName()), sizeof(ranking.playerName));
	db_clientdesc->DBPacket(HEADER_GD_REGISTER_BP_RANKING, 0, &ranking, sizeof(TBattlePassRegisterRanking));
	SetBattlePassFinalRewardTaken(pkChar, bBattlePassId);
}

bool CBattlePass::BattlePassMissionGetInfo(uint8_t bBattlePassId, uint8_t bMissionType, uint32_t* dwFirstInfo, uint32_t* dwSecondInfo)
{
	const auto it = m_map_battle_pass_name.find(bBattlePassId);
	if(it == m_map_battle_pass_name.end())
		return false;

	const std::string battlePassName = it->second;
	const auto itInfo = m_map_battle_pass_mission_info.find(battlePassName);
	
	if(itInfo == m_map_battle_pass_mission_info.end())
		return false;

	std::vector<TBattlePassMissionInfo> missionInfo = itInfo->second;
	
	for (const auto& i : missionInfo)
	{
		if(i.bMissionType == bMissionType)
		{
			*dwFirstInfo = i.dwMissionInfo[0];
			*dwSecondInfo = i.dwMissionInfo[1];
			return true;
		}
	}

	return false;
}

#ifdef ENABLE_BATTLE_PASS_SECURITY_KILL
bool CBattlePass::IsEligibleForPlayerKill(uint32_t dwKillerID, uint32_t dwPlayerID)
{
	TKillMap::iterator it = m_playersKills.find(dwKillerID);
	if(it == m_playersKills.end()) // no instance for pid so can register
		return true;
	
	std::vector<TBattlePassKillVictim *> &victimVector = it->second;
	
	auto itV = victimVector.begin();
	while (itV != victimVector.end())
	{
		TBattlePassKillVictim * tempVictim = *itV;
	
		if(tempVictim->dwVictimPid == dwPlayerID)
		{
			if (get_dword_time() < tempVictim->dwLastKillTime + 900 * 1000) // 900 seconds = 15 minutes
			{
				++itV;
				return false;
			}
			else
			{
				itV = victimVector.erase(itV);
				return true;
			}
		}
		
		++itV;
	}
	
	return true;
}

void CBattlePass::RegisterPlayerKill(uint32_t dwKillerID, uint32_t dwPlayerID)
{
	TKillMap::iterator it = m_playersKills.find(dwKillerID);
	if(it == m_playersKills.end())
	{
		std::vector<TBattlePassKillVictim *> victimVector;
		
		TBattlePassKillVictim * tempVictim = new TBattlePassKillVictim;
		tempVictim->dwVictimPid = dwPlayerID;
		tempVictim->dwLastKillTime = get_dword_time();
		
		victimVector.push_back(tempVictim);
		
		m_playersKills.insert(std::make_pair(dwKillerID, victimVector));
	}
	else
	{
		std::vector<TBattlePassKillVictim *> &victimVector = it->second;
		
		TBattlePassKillVictim * tempVictim = new TBattlePassKillVictim;
		tempVictim->dwVictimPid = dwPlayerID;
		tempVictim->dwLastKillTime = get_dword_time();
		
		victimVector.push_back(tempVictim);
	}
}
#endif


