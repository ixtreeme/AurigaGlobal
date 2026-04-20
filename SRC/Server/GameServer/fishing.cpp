//#define __FISHING_MAIN__
#include "stdafx.h"
#include "constants.h"
#include "fishing.h"
#include "locale_service.h"

#ifndef __FISHING_MAIN__
#include "item_manager.h"

#include "config.h"
#include "packet.h"

#include "sectree_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"

#include "log.h"

#include "questmanager.h"
#include "buffer_manager.h"
#include "desc_client.h"
#include "locale_service.h"

#include "affect.h"
#include "unique_item.h"
#endif

#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

#define ENABLE_FISHINGROD_RENEWAL
namespace fishing
{
	enum
	{
		MAX_FISH = 37,
		NUM_USE_RESULT_COUNT = 10, // 1 : DEAD 2 : BONE 3 ~ 12 : rest
		FISH_BONE_VNUM = 27799,
		SHELLFISH_VNUM = 27987,
		EARTHWORM_VNUM = 27801,
		WATER_STONE_VNUM_BEGIN = 28030,
		WATER_STONE_VNUM_END = 28043,
		FISH_NAME_MAX_LEN = 64,
		MAX_PROB = 4,
	};

	enum
	{
		USED_NONE,
		USED_SHELLFISH,
		USED_WATER_STONE,
		USED_TREASURE_MAP,
		USED_EARTHWARM,
		MAX_USED_FISH
	};

	enum
	{
		FISHING_TIME_NORMAL,
		FISHING_TIME_SLOW,
		FISHING_TIME_QUICK,
		FISHING_TIME_ALL,
		FISHING_TIME_EASY,

		FISHING_TIME_COUNT,

		MAX_FISHING_TIME_COUNT = 31,
	};

	enum
	{
		FISHING_LIMIT_NONE,
		FISHING_LIMIT_APPEAR,
	};

	int aFishingTime[FISHING_TIME_COUNT][MAX_FISHING_TIME_COUNT] =
	{
		{   0,   0,   0,   0,   0,   2,   4,   8,  12,  16,  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20,  16,  12,   8,   4,   2,   0,   0,   0,   0,   0 },
		{   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   4,   8,  12,  16,  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20 },
		{  20,  22,  25,  30,  50,  80,  50,  30,  25,  22,  20,  16,  12,   8,   4,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
		{ 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 },
		{  20,  20,  20,  20,  20,  22,  24,  28,  32,  36,  40,  42,  45,  50,  70, 100,  70,  50,  45,  42,  40,  36,  32,  28,  24,  22,  20,  20,  20,  20,  20 },
	};

	struct SFishInfo
	{
		char name[FISH_NAME_MAX_LEN];

		uint32_t vnum;
		uint32_t dead_vnum;
		uint32_t grill_vnum;
		int prob[MAX_PROB];
		int difficulty;

		int time_type;
		int length_range[3]; // MIN MAX EXTRA_MAX : 99% MIN~MAX, 1% MAX~EXTRA_MAX

		int used_table[NUM_USE_RESULT_COUNT];
		// 6000 2000 1000 500 300 100 50 30 10 5 4 1
	};

	bool operator < ( const SFishInfo& lhs, const SFishInfo& rhs )
	{
		return lhs.vnum < rhs.vnum;
	}

	int g_prob_sum[MAX_PROB];
	int g_prob_accumulate[MAX_PROB][MAX_FISH];

	SFishInfo fish_info[MAX_FISH] = { { "\0", }, };

void Initialize()
{
	SFishInfo fish_info_bak[MAX_FISH];
	memcpy(fish_info_bak, fish_info, sizeof(fish_info));

	memset(fish_info, 0, sizeof(fish_info));


	// LOCALE_SERVICE
	const int FILE_NAME_LEN = 256;
	char szFishingFileName[FILE_NAME_LEN+1];
	snprintf(szFishingFileName, sizeof(szFishingFileName),
			"%s/fishing.txt", LocaleService_GetBasePath().c_str());
	FILE * fp = fopen(szFishingFileName, "r");
	// END_OF_LOCALE_SERVICE

	if (*fish_info_bak[0].name)
		SendLog("Reloading fish table.");

	if (!fp)
	{
		SendLog("error! cannot open fishing.txt");

		// 1é3÷?! AI¸§AI AÖA¸¸é ¸®1oAä3î ÇN´U.
		if (*fish_info_bak[0].name)
		{
			memcpy(fish_info, fish_info_bak, sizeof(fish_info));
			SendLog("  restoring to backup");
		}
		return;
	}

	memset(fish_info, 0, sizeof(fish_info));

	char buf[512];
	int idx = 0;

	while (fgets(buf, 512, fp))
	{
		if (*buf == '#')
			continue;

		char * p = strrchr(buf, '\n');
		*p = '\0';

		const char * start = buf;
		const char * tab = strchr(start, '\t');

		if (!tab)
		{
			printf("Tab error on line: %s\n", buf);
			SendLog("error! parsing fishing.txt");

			if (*fish_info_bak[0].name)
			{
				memcpy(fish_info, fish_info_bak, sizeof(fish_info));
				SendLog("  restoring to backup");
			}
			break;
		}

		char szCol[256], szCol2[256];
		int iColCount = 0;

		do
		{
			strlcpy(szCol2, start, MIN(sizeof(szCol2), (tab - start) + 1));
			szCol2[tab-start] = '\0';

			trim_and_lower(szCol2, szCol, sizeof(szCol));

			if (!*szCol || *szCol == '\t')
				iColCount++;
			else
			{
				switch (iColCount++)
				{
					case 0: strlcpy(fish_info[idx].name, szCol, sizeof(fish_info[idx].name)); break;
					case 1: str_to_number(fish_info[idx].vnum, szCol); break;
					case 2: str_to_number(fish_info[idx].dead_vnum, szCol); break;
					case 3: str_to_number(fish_info[idx].grill_vnum, szCol); break;
					case 4: str_to_number(fish_info[idx].prob[0], szCol); break;
					case 5: str_to_number(fish_info[idx].prob[1], szCol); break;
					case 6: str_to_number(fish_info[idx].prob[2], szCol); break;
					case 7: str_to_number(fish_info[idx].prob[3], szCol); break;
					case 8: str_to_number(fish_info[idx].difficulty, szCol); break;
					case 9: str_to_number(fish_info[idx].time_type, szCol); break;
					case 10: str_to_number(fish_info[idx].length_range[0], szCol); break;
					case 11: str_to_number(fish_info[idx].length_range[1], szCol); break;
					case 12: str_to_number(fish_info[idx].length_range[2], szCol); break;
					case 13: // 0
					case 14: // 1
					case 15: // 2
					case 16: // 3
					case 17: // 4
					case 18: // 5
					case 19: // 6
					case 20: // 7
					case 21: // 8
					case 22: // 9
							 str_to_number(fish_info[idx].used_table[iColCount-1-12], szCol);
							 break;
				}
			}

			start = tab + 1;
			tab = strchr(start, '\t');
		} while (tab);

		idx++;

		if (idx == MAX_FISH)
			break;
	}

	fclose(fp);

	for (int i = 0; i < MAX_FISH; ++i)
	{
		sys_log(0, "FISH: %-24s vnum %5lu prob %4d %4d %4d %4d len %d %d %d",
				fish_info[i].name,
				fish_info[i].vnum,
				fish_info[i].prob[0],
				fish_info[i].prob[1],
				fish_info[i].prob[2],
				fish_info[i].prob[3],
				fish_info[i].length_range[0],
				fish_info[i].length_range[1],
				fish_info[i].length_range[2]);
	}

	// E®·ü °e»e
	for (int j = 0; j < MAX_PROB; ++j)
	{
		g_prob_accumulate[j][0] = fish_info[0].prob[j];

		for (int i = 1; i < MAX_FISH; ++i)
			g_prob_accumulate[j][i] = fish_info[i].prob[j] + g_prob_accumulate[j][i - 1];

		g_prob_sum[j] = g_prob_accumulate[j][MAX_FISH - 1];
		sys_log(0, "FISH: prob table %d %d", j, g_prob_sum[j]);
	}
}

int DetermineFishByProbIndex(int prob_idx)
{
	int rv = number(1, g_prob_sum[prob_idx]);
	int * p = std::lower_bound(g_prob_accumulate[prob_idx], g_prob_accumulate[prob_idx]+ MAX_FISH, rv);
	int fish_idx = p - g_prob_accumulate[prob_idx];
	return fish_idx;
}

int GetProbIndexByMapIndex(int index)
{
	if (index > 60)
		return -1;

	switch (index)
	{
		case 358:
		case 359:
		case 360:
		case 361:
			return 0;

		case 3:
		case 23:
		case 43:
			return 1;
	}

	return -1;
}

#ifndef __FISHING_MAIN__
int DetermineFish(LPCHARACTER ch) {
	int map_idx = ch->GetMapIndex();
	int prob_idx = GetProbIndexByMapIndex(map_idx);

	if (prob_idx < 0)
		return 0;

	// ADD_PREMIUM
	if (ch->GetPremiumRemainSeconds(PREMIUM_FISH_MIND) > 0 || ch->IsEquipUniqueGroup(UNIQUE_GROUP_FISH_MIND) || ch->GetPoint(POINT_FISHING_RARE) > 0)
	{
		if (quest::CQuestManager::instance().GetEventFlag("manwoo") != 0)
			prob_idx = 3;
		else
			prob_idx = 2;
	}
	// END_OF_ADD_PREMIUM

	int adjust = 0;
	if (quest::CQuestManager::instance().GetEventFlag("fish_miss_pct") != 0)
	{
		int fish_pct_value = MINMAX(0, quest::CQuestManager::instance().GetEventFlag("fish_miss_pct"), 200);
		adjust = (100-fish_pct_value) * fish_info[0].prob[prob_idx] / 100;
	}

	int rv = number(adjust + 1, g_prob_sum[prob_idx]);

	int * p = std::lower_bound(g_prob_accumulate[prob_idx], g_prob_accumulate[prob_idx] + MAX_FISH, rv);
	int fish_idx = p - g_prob_accumulate[prob_idx];
	// Áß±1?!1­´Â ±Ýµc3î¸®, ±Ý?­1e, Ao?­1e 3a?AÁö 3E°Ô ÇÔ
	{
		uint32_t vnum = fish_info[fish_idx].vnum;

		if (vnum == 50008 || vnum == 50009 || vnum == 80008)
			return 0;
	}

	return (fish_idx);
}

void FishingReact(LPCHARACTER ch)
{
	TPacketGCFishing p;
	p.header = HEADER_GC_FISHING;
	p.subheader = FISHING_SUBHEADER_GC_REACT;
	p.info = ch->GetPacketVID();
	ch->PacketAround(&p, sizeof(p));
}

void FishingSuccess(LPCHARACTER ch)
{
	TPacketGCFishing p;
	p.header = HEADER_GC_FISHING;
	p.subheader = FISHING_SUBHEADER_GC_SUCCESS;
	p.info = ch->GetPacketVID();
	ch->PacketAround(&p, sizeof(p));
}

void FishingFail(LPCHARACTER ch)
{
	TPacketGCFishing p;
	p.header = HEADER_GC_FISHING;
	p.subheader = FISHING_SUBHEADER_GC_FAIL;
	p.info = ch->GetPacketVID();
	ch->PacketAround(&p, sizeof(p));
}

void FishingPractice(LPCHARACTER ch)
{
	if (!ch)
		return;

	LPITEM rod = ch->GetWear(WEAR_WEAPON);
	if (rod && rod->GetType() == ITEM_ROD)
	{
		// AÖ´ë 1ö·Aµµ°! 3A´N °a?i 3¬1A´ë 1ö·A
		if ( rod->GetRefinedVnum()>0 && rod->GetSocket(0) < rod->GetValue(2) && number(1,rod->GetValue(1))==1 )
		{
			rod->SetSocket(0, rod->GetSocket(0) + 1);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 283, "%d#%d", rod->GetSocket(0), rod->GetValue(2));
#endif
			if (rod->GetSocket(0) == rod->GetValue(2)) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 279, "");
				ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 280, "");
#endif
			}
		}
	}
	// 1I3c¸¦ »«´U
	rod->SetSocket(2, 0);
}

bool PredictFish(LPCHARACTER ch)
{
	// ADD_PREMIUM
	// 3î1ÉE—
	if (ch->FindAffect(AFFECT_FISH_MIND_PILL) ||
			ch->GetPremiumRemainSeconds(PREMIUM_FISH_MIND) > 0 ||
			ch->IsEquipUniqueGroup(UNIQUE_GROUP_FISH_MIND))
		return true;
	// END_OF_ADD_PREMIUM

	return false;
}

EVENTFUNC(fishing_event)
{
	fishing_event_info * info = dynamic_cast<fishing_event_info *>( event->info );

	if ( info == nullptr)
	{
		sys_err( "fishing_event> <Factor> Null pointer" );
		return 0;
	}

	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(info->pid);

	if (!ch)
		return 0;


	LPITEM rod = ch->GetWear(WEAR_WEAPON);

	if (!(rod && rod->GetType() == ITEM_ROD))
	{
		ch->GetFishingEventRef() = nullptr;
		return 0;
	}

	switch (info->step)
	{
		case 0:	// Eçµé¸®±â ¶Ç´Â ¶±1ä¸¸ 3—3A°¨
			++info->step;

			//info->ch->Motion(MOTION_FISHING_SIGN);
			info->hang_time = get_dword_time();
			info->fish_id = DetermineFish(ch);
			FishingReact(ch);

			if (PredictFish(ch))
			{
				TPacketGCFishing p;
				p.header	= HEADER_GC_FISHING;
				p.subheader	= FISHING_SUBHEADER_GC_FISH;
				p.info	= fish_info[info->fish_id].vnum;
				ch->GetDesc()->Packet(&p, sizeof(TPacketGCFishing));
			}
			return (PASSES_PER_SEC(6));

		default:
			++info->step;

			if (info->step > 5)
				info->step = 5;

			ch->GetFishingEventRef() = nullptr;
			FishingFail(ch);
			rod->SetSocket(2, 0);
			return 0;
	}
}

LPEVENT CreateFishingEvent(LPCHARACTER ch)
{
	fishing_event_info* info = AllocEventInfo<fishing_event_info>();
	info->pid	= ch->GetPlayerID();
	info->step	= 0;
	info->hang_time	= 0;

	int time = number(10, 40);

	TPacketGCFishing p;
	p.header	= HEADER_GC_FISHING;
	p.subheader	= FISHING_SUBHEADER_GC_START;
	p.info		= ch->GetPacketVID();
	p.dir		= (uint8_t)(ch->GetRotation()/5);
	ch->PacketAround(&p, sizeof(TPacketGCFishing));

	return event_create(fishing_event, info, PASSES_PER_SEC(time));
}

int GetFishingLevel(LPCHARACTER ch)
{
	LPITEM rod = ch->GetWear(WEAR_WEAPON);

	if (!rod || rod->GetType()!= ITEM_ROD)
		return 0;

	return rod->GetSocket(2) + rod->GetValue(0);
}

int Compute(uint32_t fish_id, uint32_t ms, uint32_t* item, int level) {
	if (fish_id == 0)
		return -2;

	if (fish_id >= MAX_FISH)
	{
		sys_err("Wrong FISH ID : %d", fish_id);
		return -2;
	}

	if (ms > 6000)
		return -1;

	int time_step = MINMAX(0,((ms + 99) / 200), MAX_FISHING_TIME_COUNT - 1);
	if (number(1, 100) <= aFishingTime[fish_info[fish_id].time_type][time_step])
	{
		if (number(1, fish_info[fish_id].difficulty) <= level)
		{
			*item = fish_info[fish_id].vnum;
			return 0;
		}

		return -3;
	}

	return -1;
}

void Take(fishing_event_info* info, LPCHARACTER ch)
{
	if (info->step == 1)	// °í±â°! °É¸° »óAÂ¸é..
	{
		int32_t ms = (int32_t) ((get_dword_time() - info->hang_time));
		uint32_t item_vnum = 0;
		int ret = Compute(info->fish_id, ms, &item_vnum, GetFishingLevel(ch));
		switch (ret)
		{
			case -2: // AâE÷Áö 3EAo °a?i
			case -3: // 3­AIµµ ¶§1®?! 1ÇA?
			case -1: // 1A°L E®·ü ¶§1®?! 1ÇA?
				{
					int map_idx = ch->GetMapIndex();
					int prob_idx = GetProbIndexByMapIndex(map_idx);

					LogManager::instance().FishLog(
							ch->GetPlayerID(),
							prob_idx,
							info->fish_id,
							GetFishingLevel(ch),
							ms);
				}
				FishingFail(ch);
				break;

			case 0:
				if (item_vnum)
				{
					FishingSuccess(ch);

					TPacketGCFishing p;
					p.header = HEADER_GC_FISHING;
					p.subheader = FISHING_SUBHEADER_GC_FISH;
					p.info = item_vnum;
					ch->GetDesc()->Packet(&p, sizeof(TPacketGCFishing));

#ifdef ENABLE_BATTLE_PASS
						uint8_t bBattlePassId = ch->GetBattlePassId();
						if(bBattlePassId)
						{
							uint32_t dwCount, dwNotUsed;
							if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, CATCH_FISH, &dwNotUsed, &dwCount))
							{
								if(ch->GetMissionProgress(CATCH_FISH, bBattlePassId) < dwCount)
									ch->UpdateMissionProgress(CATCH_FISH, bBattlePassId, 1, dwCount);
							}
						}
#endif
					LPITEM item = ch->AutoGiveItem(item_vnum, 1, -1, false);
					if (item)
					{
#ifdef ENABLE_RANKING
						if ((item->GetType() == ITEM_FISH) || (item->GetVnum() == 27802)) {
							ch->SetRankPoints(14, ch->GetRankPoints(14) + 1);
						}
#endif


#ifndef ENABLE_NEW_FISHING_SYSTEM
						item->SetSocket(0, GetFishLength(info->fish_id));
#endif
						if (quest::CQuestManager::instance().GetEventFlag("fishevent") > 0 && (info->fish_id == 5 || info->fish_id == 6))
						{
							// AIoYA® ÁßAI1Ç·Î ±â·IÇN´U.

							TPacketGDHighscore p;
							p.dwPID = ch->GetPlayerID();
							p.lValue = item->GetSocket(0);

							if (info->fish_id == 5)
							{
								strlcpy(p.szBoard, "Fishing Event", sizeof(p.szBoard));
							}
							else if (info->fish_id == 6)
							{
								strlcpy(p.szBoard, "Fishing Event Carp", sizeof(p.szBoard));
							}

							db_clientdesc->DBPacket(HEADER_GD_HIGHSCORE_REGISTER, 0, &p, sizeof(TPacketGDHighscore));
						}
					}

					int map_idx = ch->GetMapIndex();
					int prob_idx = GetProbIndexByMapIndex(map_idx);

					LogManager::instance().FishLog(
							ch->GetPlayerID(),
							prob_idx,
							info->fish_id,
							GetFishingLevel(ch),
							ms,
							true,
							item ? item->GetSocket(0) : 0);

				}
				else
				{
					int map_idx = ch->GetMapIndex();
					int prob_idx = GetProbIndexByMapIndex(map_idx);

					LogManager::instance().FishLog(
							ch->GetPlayerID(),
							prob_idx,
							info->fish_id,
							GetFishingLevel(ch),
							ms);
					FishingFail(ch);
				}
				break;
		}
	}
	else if (info->step > 1)
	{
		int map_idx = ch->GetMapIndex();
		int prob_idx = GetProbIndexByMapIndex(map_idx);

		LogManager::instance().FishLog(
				ch->GetPlayerID(),
				prob_idx,
				info->fish_id,
				GetFishingLevel(ch),
				7000);
		FishingFail(ch);
	}
	else
	{
		TPacketGCFishing p;
		p.header = HEADER_GC_FISHING;
		p.subheader = FISHING_SUBHEADER_GC_STOP;
		p.info = ch->GetPacketVID();
		ch->PacketAround(&p, sizeof(p));
	}

	if (info->step)
	{
		FishingPractice(ch);
	}
	//Motion(MOTION_FISHING_PULL);
}

void Simulation(int level, int count, int prob_idx, LPCHARACTER ch)
{
	std::map<std::string, int> fished;
	int total_count = 0;

	for (int i = 0; i < count; ++i)
	{
		int fish_id = DetermineFishByProbIndex(prob_idx);
		uint32_t item = 0;
		Compute(fish_id, (number(2000, 4000) + number(2000,4000)) / 2, &item, level);

		if (item)
		{
			fished[fish_info[fish_id].name]++;
			total_count ++;
		}
	}

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 98, "%d#%d", fished.size(), total_count);
#endif
}

void UseFish(LPCHARACTER ch, LPITEM item)
{
	int idx = item->GetVnum() - fish_info[2].vnum+2;

	// ÇÇ¶ó1I »ç?ëoO°!, »i3AAÖ´Â°Ô 3A´N°Ç »ç?ëoO°!

	if (idx<=1 || idx >= MAX_FISH)
		return;

	int r = number(1, 100);

	item->SetCount(item->GetCount()-1);

	if (r >= 70) {
		ch->AutoGiveItem(fish_info[idx].dead_vnum);
	}
#ifdef ENABLE_NEW_FISHING_SYSTEM
	else {
		ch->AutoGiveItem(FISH_BONE_VNUM);
	}
#else
	else {
		// 1000 500 300 100 50 30 10 5 4 1
		static int s_acc_prob[NUM_USE_RESULT_COUNT] = { 1000, 1500, 1800, 1900, 1950, 1980, 1990, 1995, 1999, 2000 };
		int u_index = std::lower_bound(s_acc_prob, s_acc_prob + NUM_USE_RESULT_COUNT, r) - s_acc_prob;

		switch (fish_info[idx].used_table[u_index])
		{
			case USED_TREASURE_MAP:
			case USED_NONE:
			case USED_WATER_STONE:
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 248, "");
#endif
				break;
			case USED_SHELLFISH:
				ch->AutoGiveItem(SHELLFISH_VNUM);
				break;
			case USED_EARTHWARM:
				ch->AutoGiveItem(EARTHWORM_VNUM);
				break;
			default:
				ch->AutoGiveItem(fish_info[idx].used_table[u_index]);
				break;
		}
	}
#endif
}

void Grill(LPCHARACTER ch, LPITEM item)
{
	int idx = -1;
	uint32_t vnum = item->GetVnum();
	if (vnum >= 27803 && vnum <= 27830)
		return;
	if (vnum >= 27833 && vnum <= 27860)
		idx = vnum - 27830;
	if (idx == -1)
		return;

	int count = item->GetCount();
	
#ifdef ENABLE_BATTLE_PASS
	uint8_t bBattlePassId = ch->GetBattlePassId();
	if(bBattlePassId)
	{
		uint32_t dwCount, dwNotUsed;
		if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, FRY_FISH, &dwNotUsed, &dwCount))
		{
			if(ch->GetMissionProgress(FRY_FISH, bBattlePassId) < dwCount)
				ch->UpdateMissionProgress(FRY_FISH, bBattlePassId, count, dwCount);
		}
	}
#endif

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 116, "%s", item->GetName());
#endif
	item->SetCount(0);
	ch->AutoGiveItem(fish_info[idx].grill_vnum, count);
}

bool RefinableRod(LPITEM rod)
{
	if (rod->GetType() != ITEM_ROD)
		return false;

	if (rod->IsEquipped())
		return false;

	return (rod->GetSocket(0) == rod->GetValue(2));
}

int RealRefineRod(LPCHARACTER ch, LPITEM item)
{
	if (!ch || !item)
		return 2;

	if (!RefinableRod(item))
	{
		sys_err("REFINE_ROD_HACK pid(%u) item(%s:%d)", ch->GetPlayerID(), item->GetName(), item->GetID());
		LogManager::instance().RefineLog(ch->GetPlayerID(), item->GetName(), item->GetID(), -1, 1, "ROD_HACK");
		return 6;
	}

	LPITEM rod = item;

	int iAdv = rod->GetValue(0) / 10;

	if (number(1, 100) <= rod->GetValue(3))
	{
		LogManager::instance().RefineLog(ch->GetPlayerID(), rod->GetName(), rod->GetID(), iAdv, 1, "ROD");
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(rod->GetRefinedVnum(), 1);
		if (!pkNewItem)
			return 4;

		uint8_t bCell = rod->GetCell();
		ITEM_MANAGER::instance().RemoveItem(rod, "REMOVE (REFINE FISH_ROD)");
		pkNewItem->AddToCharacter(ch, TItemPos (INVENTORY, bCell));
		LogManager::instance().ItemLog(ch, pkNewItem, "REFINE FISH_ROD SUCCESS", pkNewItem->GetName());
		return 1;
	} else {
		LogManager::instance().RefineLog(ch->GetPlayerID(), rod->GetName(), rod->GetID(), iAdv, 0, "ROD");
#ifdef ENABLE_FISHINGROD_RENEWAL
		int cur = rod->GetSocket(0);
		rod->SetSocket(0, (cur > 0) ? (cur - (cur * 20 / 100)) : 0);
		LogManager::instance().ItemLog(ch, rod, "REFINE FISH_ROD FAIL", rod->GetName());
#else
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(rod->GetValue(4), 1);
		if (!pkNewItem)
			return 3;
		
		uint8_t bCell = rod->GetCell();
		ITEM_MANAGER::instance().RemoveItem(rod, "REMOVE (REFINE FISH_ROD)");
		pkNewItem->AddToCharacter(ch, TItemPos(INVENTORY, bCell));
		LogManager::instance().ItemLog(ch, pkNewItem, "REFINE FISH_ROD FAIL", pkNewItem->GetName());
#endif
		return 2;
	}
}
#endif
}

#ifdef __FISHING_MAIN__
int main(int argc, char **argv)
{
	//srandom(time(0) + getpid());
	srandomdev();
	/*
	   struct SFishInfo
	   {
	   const char* name;

	   uint32_t vnum;
	   uint32_t dead_vnum;
	   uint32_t grill_vnum;

	   int prob[3];
	   int difficulty;

	   int limit_type;
	   int limits[3];

	   int time_type;
	   int length_range[3]; // MIN MAX EXTRA_MAX : 99% MIN~MAX, 1% MAX~EXTRA_MAX

	   int used_table[NUM_USE_RESULT_COUNT];
	// 6000 2000 1000 500 300 100 50 30 10 5 4 1
	};
	 */
	using namespace fishing;

	Initialize();

	for (int i = 0; i < MAX_FISH; ++i)
	{
		printf("%s\t%u\t%u\t%u\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d",
				fish_info[i].name,
				fish_info[i].vnum,
				fish_info[i].dead_vnum,
				fish_info[i].grill_vnum,
				fish_info[i].prob[0],
				fish_info[i].prob[1],
				fish_info[i].prob[2],
				fish_info[i].difficulty,
				fish_info[i].time_type,
				fish_info[i].length_range[0],
				fish_info[i].length_range[1],
				fish_info[i].length_range[2]);

		for (int j = 0; j < NUM_USE_RESULT_COUNT; ++j)
			printf("\t%d", fish_info[i].used_table[j]);

		puts("");
	}

	return 1;
}

#endif

#ifdef ENABLE_NEW_FISHING_SYSTEM
namespace fishingnew
{
	int aFishFirstTableNormal[26] = {
									27803, //Süllo
									27803, //Süllo
									27803, //Süllo
									27803, //Süllo
									27803, //Süllo
									27803, //Süllo
									27806, //Ponty
									27816, //Harcsa
									27807, //Lazac
									27818, //Lótuszhal
									71136,	//Hatalmas nyalóka (1h)
									39065,//Utalvány (1 SÉ)
									2870,	//Birodalom Rúna  
									2871,	//Ork Rúna  
									2873,	//Jég Birodalom Rúna  
									99998,	// Boss Pont
									39066,	// Gaya
									39068,	// Auriga Coin (10m Yang)
									80003,// Ezüst rúd (50.000 Yang)
									80004,// Ezüst rúd (100.000 Yang)
									80005,// Aranyrúd (500.000 Yang)
									80006,// Arany rúd (1 millió Yang)
									80007,// Arany rúd (2 millió Yang)
									89106,// Ork Run Belépö
									71175,// Nemere jegy
									30625,// Egy Darab menhir iszap




	};								




	int aFishFirstTableRare[41] = {
									27804,	//Mandarinhal
									27811,	//Szivárványos pisztráng
									27810,	//Angolna
									27809,	//Pisztráng
									27814,	//Sügér
									27812,	//Sebes pisztráng
									27808,	//Amur
									2872,//Nefrit Rúna 5x
									2874,	//Sivatagi Rúna 15x
									30179,	// Csavart kulcs
									76019,	// Tritontemplom kulcs
									30713,	// Owl jegy
									30798,	// Ankh kereszt
									71095,	// Belépési engedély
									30320,	// Sárga Zsugorított f
									76025,	// Ochao jegy
									30613,	// Tündérek köve
									71174,//Razador jegy
									30325,//pok kulcs
									89101,//Rúna jegy
									27804,	//Mandarinhal
									27811,	//Szivárványos pisztráng
									27810,	//Angolna
									27809,	//Pisztráng
									27814,	//Sügér
									27812,	//Sebes pisztráng
									27808,	//Amur
									27804,	//Mandarinhal
									27811,	//Szivárványos pisztráng
									27810,	//Angolna
									27809,	//Pisztráng
									27814,	//Sügér
									27812,	//Sebes pisztráng
									27808,	//Amur
									27804,	//Mandarinhal
									27811,	//Szivárványos pisztráng
									27810,	//Angolna
									27809,	//Pisztráng
									27814,	//Sügér
									27812,	//Sebes pisztráng
									27808,	//Amur



	};

	int aFishSecondTableNormal[37] = {
									27805,//Fogas
									27822,//Tükörponty
									27823,//Aranyhal
									27824,//Kígyófeju hal
									27825,//Ragadozó ponty
									2875,//Rémálom Rúna 5x
									2876,//Sötét erdõ Rúna 5x//
									99998,//bosspont
									70606,//Felolvasztási bónusz
									70605,//fagyasztási bónusz csere
									30617,//Legendás Bónuszoló
									30618,//Legendás Megváltoztató
									86052,//Talizmánerösíto
									86051,//Talizmán büvölo
									71123,// Sárkány pikkely
									71129,// Sárkány karom
									71136,// Hatalmas Nyalóka (2h)
																		27805,//Fogas
									27822,//Tükörponty
									27823,//Aranyhal
									27824,//Kígyófeju hal
									27825,//Ragadozó ponty
																		27805,//Fogas
									27822,//Tükörponty
									27823,//Aranyhal
									27824,//Kígyófeju hal
									27825,//Ragadozó ponty
																		27805,//Fogas
									27822,//Tükörponty
									27823,//Aranyhal
									27824,//Kígyófeju hal
									27825,//Ragadozó ponty
																		27805,//Fogas
									27822,//Tükörponty
									27823,//Aranyhal
									27824,//Kígyófeju hal
									27825,//Ragadozó ponty

	};

	int aFishSecondTableRare[63] = {
									27826, //Vörös királyrák
									27827, //Ausztrál kék rák
									27813, //Vörösszárnyú keszeg
									27815, //Tenchi
									27819, //Ayu
									27820, //Viaszlazac
									27821, //Shiri
									2877,	//Tûz Rúna 5x
									2878,	//SD5 Rúna 5x
									60011,	//Paranoia Köve +6
									60031,	//Metin elleni ko+6
									60041,	//Stone of Boss+6
									2858	,	//Profiq Pepsi-je
									53251	,	//Frank (15d)
									18090	,	//Turmalin öv+0
									55706	,	//Mini Meley
									71123	,	//Sárkány pikkely
									71129	,	//Sárkány karom
									60010	,	//Paranoia Köve +5
									60020	,	//Háború Köve +5
									60030	,	//Metin elleni ko+5
									60040	,	//Boss Elleni  Kö+5
									60050	,	//Inteligencia Köve +
									60060	,	//Állatok Köve +5
									60070	,	//Ügyesség Köve +5
									60080	,	//Erö Köve+5
									72726	,	//Nap elixír (E)
									72730	,	//Hold elixír (E)
									50525	,	//Tökéletes lélekko
									611516	,//szoposszaju hal mount
									611516	,//szoposszaju hal mount
									611516	,//szoposszaju hal mount
									611516	,//szoposszaju hal mount
									611516	,//szoposszaju hal mount
									611516	,//szoposszaju hal mount
															27826, //Vörös királyrák
									27827, //Ausztrál kék rák
									27813, //Vörösszárnyú keszeg
									27815, //Tenchi
									27819, //Ayu
									27820, //Viaszlazac
									27821, //Shiri
															27826, //Vörös királyrák
									27827, //Ausztrál kék rák
									27813, //Vörösszárnyú keszeg
									27815, //Tenchi
									27819, //Ayu
									27820, //Viaszlazac
									27821, //Shiri
															27826, //Vörös királyrák
									27827, //Ausztrál kék rák
									27813, //Vörösszárnyú keszeg
									27815, //Tenchi
									27819, //Ayu
									27820, //Viaszlazac
									27821, //Shiri
															27826, //Vörös királyrák
									27827, //Ausztrál kék rák
									27813, //Vörösszárnyú keszeg
									27815, //Tenchi
									27819, //Ayu
									27820, //Viaszlazac
									27821, //Shiri


	};
	uint32_t GetFishCatchedVnum(uint8_t normal_chance, uint8_t rare_chance, bool second)
	{
		auto pick = [](const int* arr, int n) -> uint32_t {
			return arr[number(0, n - 1)];
			};

		const int firstNormalN = (int)(sizeof(aFishFirstTableNormal) / sizeof(aFishFirstTableNormal[21]));
		const int firstRareN = (int)(sizeof(aFishFirstTableRare) / sizeof(aFishFirstTableRare[20]));
		const int secondNormalN = (int)(sizeof(aFishSecondTableNormal) / sizeof(aFishSecondTableNormal[17]));
		const int secondRareN = (int)(sizeof(aFishSecondTableRare) / sizeof(aFishSecondTableRare[30]));

		// Rare dobás logika: rare_chance% eséllyel rare
		const bool isRare = (number(1, 100) <= rare_chance);

		if (isRare)
			return second ? pick(aFishSecondTableRare, secondRareN)
			: pick(aFishFirstTableRare, firstRareN);
		else
			return second ? pick(aFishSecondTableNormal, secondNormalN)
			: pick(aFishFirstTableNormal, firstNormalN);
	}

	//uint32_t GetFishCatchedVnum(uint8_t normal_chance, uint8_t rare_chance, bool second) {
	//	if (number(1, 100) >= uint8_t(normal_chance-rare_chance)) {
	//		return second == true ? aFishSecondTableRare[number(0, 6)] : aFishFirstTableRare[number(0, 4)];
	//	} else {
	//		return second == true ? aFishSecondTableNormal[number(0, 6)] : aFishFirstTableNormal[number(0, 4)];
	//	}
	//}
}
#endif

