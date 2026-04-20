#include "stdafx.h"
#include <common/service.h>
#include <common/CommonDefines.h>
#include <common/length.h>
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "skill.h"
#include "config.h"
#include "sectree_manager.h"
#include "db.h"
#ifdef ENABLE_CHOOSE_DOCTRINE_GUI
#include "horsename_manager.h"
#endif

#define MAX_STATUS_ALTERNATIVE g_iStatusPointSetMaxValue

#ifdef ENABLE_BLOCK_MULTIFARM
ACMD(do_drop_block) {
	ch->BlockDrop();
}

ACMD(do_drop_unblock) {
	ch->UnblockDrop();
}
#endif

ACMD(do_remove_affect)
{
	if (!ch)
		return;
	
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	
	if (!*arg1)
		return;
	
	uint32_t dwAffect = 0;
	str_to_number(dwAffect, arg1);
	
	switch (dwAffect)
	{
		case AFF_JEONGWIHON:
			{
				if (ch->IsAffectFlag(AFF_JEONGWIHON))
					ch->RemoveAffect(SKILL_JEONGWI);
			}
			break;
		case AFF_GEOMGYEONG:
			{
				if (ch->IsAffectFlag(AFF_GEOMGYEONG))
					ch->RemoveAffect(SKILL_GEOMKYUNG);
			}
			break;
		case AFF_CHEONGEUN:
			{
				if (ch->IsAffectFlag(AFF_CHEONGEUN))
					ch->RemoveAffect(SKILL_CHUNKEON);
			}
			break;
		case AFF_GYEONGGONG:
			{
				if (ch->IsAffectFlag(AFF_GYEONGGONG))
					ch->RemoveAffect(SKILL_GYEONGGONG);
			}
			break;
		case AFF_GWIGUM:
			{
				if (ch->IsAffectFlag(AFF_GWIGUM))
					ch->RemoveAffect(SKILL_GWIGEOM);
			}
			break;
		case AFF_TERROR:
			{
				if (ch->IsAffectFlag(AFF_TERROR))
					ch->RemoveAffect(SKILL_TERROR);
			}
			break;
		case AFF_JUMAGAP:
			{
				if (ch->IsAffectFlag(AFF_JUMAGAP))
					ch->RemoveAffect(SKILL_JUMAGAP);
			}
			break;
		case AFF_MUYEONG:
			{
				if (ch->IsAffectFlag(AFF_MUYEONG))
					ch->RemoveAffect(SKILL_MUYEONG);
			}
			break;
		case AFF_MANASHIELD:
			{
				if (ch->IsAffectFlag(AFF_MANASHIELD))
					ch->RemoveAffect(SKILL_MANASHILED);
			}
			break;
		case AFF_HOSIN:
			{
				if (ch->IsAffectFlag(AFF_HOSIN))
					ch->RemoveAffect(SKILL_HOSIN);
			}
			break;
		case AFF_BOHO:
			{
				if (ch->IsAffectFlag(AFF_BOHO))
					ch->RemoveAffect(SKILL_REFLECT);
			}
			break;
		case AFF_GICHEON:
			{
				if (ch->IsAffectFlag(AFF_GICHEON))
					ch->RemoveAffect(SKILL_GICHEON);
			}
			break;
		case AFF_KWAESOK:
			{
				if (ch->IsAffectFlag(AFF_KWAESOK))
					ch->RemoveAffect(SKILL_KWAESOK);
			}
			break;
		case AFF_JEUNGRYEOK:
			{
				if (ch->IsAffectFlag(AFF_JEUNGRYEOK))
					ch->RemoveAffect(SKILL_JEUNGRYEOK);
			}
			break;
		default:
			return;
	}
}

ACMD(do_stat2)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;
	
	if (ch->IsPolymorphed()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 314, "");
#endif
		return;
	}
	

	int64_t limit = 10;

	
	if (ch->GetPoint(POINT_STAT) < limit) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 851,

		"%lld"

		, limit);
#endif
		return;
	}
	
	uint8_t idx = 0;
	if (!strcmp(arg1, "st"))
		idx = POINT_ST;
	else if (!strcmp(arg1, "dx"))
		idx = POINT_DX;
	else if (!strcmp(arg1, "ht"))
		idx = POINT_HT;
	else if (!strcmp(arg1, "iq"))
		idx = POINT_IQ;
	else
		return;
	
	if (ch->GetRealPoint(idx) >= MAX_STATUS_ALTERNATIVE)
		return;
	
	limit = ch->GetRealPoint(idx) + limit >= MAX_STATUS_ALTERNATIVE ? MAX_STATUS_ALTERNATIVE - ch->GetRealPoint(idx) : limit;
	ch->SetRealPoint(idx, ch->GetRealPoint(idx) + limit);
	ch->SetPoint(idx, ch->GetPoint(idx) + limit);
	ch->ComputePoints();
	ch->PointChange(idx, 0);
	
	if (idx == POINT_IQ) {
		ch->PointChange(POINT_MAX_HP, 0);
	}
	else if (idx == POINT_HT) {
		ch->PointChange(POINT_MAX_SP, 0);
	}
	
	ch->PointChange(POINT_STAT, -limit);
	ch->ComputePoints();
}

#ifdef ENABLE_BIOLOGIST_UI
ACMD(do_open_biologist) {
	if (!ch)
		return;

	int stat = ch->GetQuestFlag("biologist.stat");
	if (stat > 15) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 867, "");
#endif
		return;
	}
	int min = biologistMissionInfo[stat][12];
	if (ecs::GetLevel(ch) < min) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 861, "%d", min);
#endif
		return;
	}
	ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist_reward %d#%d#%d#%d#%d#%d#%d#%d#%d", biologistMissionInfo[stat][11], biologistMissionInfo[stat][3], biologistMissionInfo[stat][4], biologistMissionInfo[stat][5], biologistMissionInfo[stat][6], biologistMissionInfo[stat][7], biologistMissionInfo[stat][8], biologistMissionInfo[stat][9], biologistMissionInfo[stat][10]);
	int time = ch->GetQuestFlag("biologist.time");
	time = time > 0 ? time : 0;
	int count = ch->GetQuestFlag("biologist.delivered");
	ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist %d#%d#%d#%d#%d#%d#%d", stat, biologistMissionInfo[stat][0], biologistMissionInfo[stat][1], count, biologistMissionInfo[stat][2], time, biologistMissionInfo[stat][1]-count > 0 ? 0 : 1);
}

ACMD(do_delivery_biologist) {
	if (!ch)
		return;

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;

	int stat = ch->GetQuestFlag("biologist.stat");
	if (stat > 15) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 867, "");
#endif
		return;
	}
	int min = biologistMissionInfo[stat][12];
	if (ecs::GetLevel(ch) < min) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 861, "%d", min);
#endif
		return;
	}

	int count = ch->GetQuestFlag("biologist.delivered");
	if (count >= biologistMissionInfo[stat][1]) {
		return;
	}

	int vnum = biologistMissionInfo[stat][0];
	if (ch->CountSpecifyItem(vnum) <= 0) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 866, "");
#endif
		return;
	}

	int iarg1 = 0;
	str_to_number(iarg1, arg1);
	int iarg2 = 0;
	str_to_number(iarg2, arg2);
	if (iarg1 == 1 && iarg2 == 1) {
		return;
	}

	bool elisir = iarg1 == 1 ? true : false;
	bool potion = iarg2 == 1 ? true : false;

	int time = ch->GetQuestFlag("biologist.time") ;
	if (time > 0 && time - get_global_time() > 0 && !elisir && !potion) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 871, "");
#endif
		return;
	}

	uint32_t item = elisir == true ? 40144 : 0;
	if (item == 0) {
		item = potion == true ? 40143 : 0;
	}

	if (item != 0) {
		if (ch->CountSpecifyItem(item) <= 0) {
#ifdef TEXTS_IMPROVEMENT
			if (item == 40143) {
				ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 863, "");
			} else {
				ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 862, "");
			}
#endif
			return;
		}

		ch->RemoveSpecifyItem(item, 1);
	}

	ch->RemoveSpecifyItem(vnum, 1);

	int success = potion == true ? 100 : biologistMissionInfo[stat][13];
	int waittime = biologistMissionInfo[stat][2] + get_global_time();
	if (number(1, 100) <= success) {
		if (waittime != 0 && count + 1 >= biologistMissionInfo[stat][1]) {
			waittime = get_global_time();
		}

		ch->SetQuestFlag("biologist.delivered", count + 1);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 865, "");
#endif
		if (waittime != 0 && count + 1 >= biologistMissionInfo[stat][1]) {
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist_delivered 1#%d#%d", count + 1, waittime - get_global_time());
		} else {
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist_delivered 0#%d#%d", count + 1, waittime - get_global_time());
		}
	}
	else {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 864, "");
#endif
		ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist_delivered 0#%d#%d", count, waittime - get_global_time());
	}
	ch->SetQuestFlag("biologist.time", waittime);
	ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist_time %d", waittime);
}

ACMD(do_reward_biologist) {
	if (!ch)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	int stat = ch->GetQuestFlag("biologist.stat");
	if (stat > 15) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 867, "");
#endif
		return;
	}
	int count = ch->GetQuestFlag("biologist.delivered");
	if (biologistMissionInfo[stat][1]-count > 0) {
		return;
	}

	int iarg1 = 0;
	str_to_number(iarg1, arg1);
	if (biologistMissionInfo[stat][11] == 0 && iarg1 != 999) {
		return;
	} else if (biologistMissionInfo[stat][11] == 1) {
		if (iarg1 < 0 || iarg1 > 3) {
			return;
		}
	}

	int newstat = stat + 1;
	ch->SetQuestFlag("biologist.stat", newstat);
	ch->SetQuestFlag("biologist.time", 0);
	ch->SetQuestFlag("biologist.delivered", 0);
	if (newstat == 16) {
		if (biologistMissionInfo[stat][11] == 0) {
			int j = 0;
			for (int i = 0; i < 4; i++) {
				j += 2;
				uint8_t bApplyOn = biologistMissionInfo[stat][j + 1];
				int32_t lApplyValue = biologistMissionInfo[stat][j + 2];
				if (bApplyOn == APPLY_NONE || lApplyValue == 0) {
					continue;
				}
				else {
					bApplyOn = aApplyInfo[bApplyOn].bPointType;
					ch->AddAffect(biologistMissionInfo[stat][14], bApplyOn, lApplyValue, 0, 315360000, 0, false);
				}
			}
		} else {
			iarg1 += 1;
			int j = 2 * iarg1;
			uint8_t bApplyOn = biologistMissionInfo[stat][j + 1];
			int32_t lApplyValue = biologistMissionInfo[stat][j + 2];
			if (bApplyOn != APPLY_NONE || lApplyValue != 0) {
				bApplyOn = aApplyInfo[bApplyOn].bPointType;
				ch->AddAffect(biologistMissionInfo[stat][14], bApplyOn, lApplyValue, 0, 315360000, 0, false);
			}
		}
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 870, "");
#endif
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 868, "");
#endif
		ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist_close");
		return;
	}
	else if (newstat > 15) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 868, "");
#endif
		ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist_close");
		return;
	} else {
		int min = biologistMissionInfo[newstat][12];
		if (ecs::GetLevel(ch) < min) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 869, "%d", min);
#endif
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist_close");
			return;
		} else {
			if (biologistMissionInfo[stat][11] == 0) {
				int j = 0;
				for (int i = 0; i < 4; i++) {
					j += 2;
					uint8_t bApplyOn = biologistMissionInfo[stat][j + 1];
					int32_t lApplyValue = biologistMissionInfo[stat][j + 2];
					if (bApplyOn == APPLY_NONE || lApplyValue == 0) {
						continue;
					}
					else {
						bApplyOn = aApplyInfo[bApplyOn].bPointType;
						ch->AddAffect(biologistMissionInfo[stat][14], bApplyOn, lApplyValue, 0, 315360000, 0, false);
					}
				}
			} else {
				iarg1 += 1;
				int j = 2 * iarg1;
				uint8_t bApplyOn = biologistMissionInfo[stat][j + 1];
				int32_t lApplyValue = biologistMissionInfo[stat][j + 2];
				if (bApplyOn != APPLY_NONE || lApplyValue != 0) {
					bApplyOn = aApplyInfo[bApplyOn].bPointType;
					ch->AddAffect(biologistMissionInfo[stat][14], bApplyOn, lApplyValue, 0, 315360000, 0, false);
				}
			}

#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 870, "");
#endif
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist_reward %d#%d#%d#%d#%d#%d#%d#%d#%d", biologistMissionInfo[newstat][11], biologistMissionInfo[newstat][3], biologistMissionInfo[newstat][4], biologistMissionInfo[newstat][5], biologistMissionInfo[newstat][6], biologistMissionInfo[newstat][7], biologistMissionInfo[newstat][8], biologistMissionInfo[newstat][9], biologistMissionInfo[newstat][10]);
			int time = ch->GetQuestFlag("biologist.time");
			time = time > 0 ? time - get_global_time() : 0;
			int count = ch->GetQuestFlag("biologist.delivered");
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologist_next %d#%d#%d#%d#%d#%d#%d", newstat, biologistMissionInfo[newstat][0], biologistMissionInfo[newstat][1], count, biologistMissionInfo[newstat][2], time, biologistMissionInfo[newstat][1]-count > 0 ? 0 : 1);
		}
	}
}

ACMD(do_change_biologist) {
	if (!ch)
		return;

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;

	
	int iarg1 = 0;
	str_to_number(iarg1, arg1);
	if (iarg1 < 0 || iarg1 > 15) {
		return;
	}
	
	if (biologistMissionInfo[iarg1][11] != 1) {
		return;
	}

	int iarg2 = 0;
	str_to_number(iarg2, arg2);
	if (iarg2 < 0 || iarg2 > 3) {
		return;
	}

	int stat = ch->GetQuestFlag("biologist.stat");
	if (stat <= iarg1)
		return;

	iarg2 = 3 + (iarg2 * 2);
	int type =  aApplyInfo[biologistMissionInfo[iarg1][iarg2]].bPointType;
	int idx = biologistMissionInfo[iarg1][14];
	CAffect * pkAff = ch->FindAffect(idx, type);
	if (pkAff) {
		return;
	}
	else {
		if (ch->CountSpecifyItem(164401) <= 0) {
			return;
		}

		ch->RemoveSpecifyItem(164401, 1);
		ch->RemoveAffect(idx);
		ch->AddAffect(idx, type, biologistMissionInfo[iarg1][iarg2 + 1], 0, 315360000, 0, false);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 873, "");
#endif
		ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "biologistch_close");
	}
}
#endif

ACMD(do_gotoxy)
{
	char arg1[256], arg2[256];
	int x = 0, y = 0, z = 0;
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 && !*arg2) {
		return;
	}
	else if (!isnhdigit(*arg1) || !isnhdigit(*arg2)) {
		return;
	}

	int iPulse = thecore_pulse();
	if (iPulse - ch->GetGoToXYTime() < PASSES_PER_SEC(10)) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 1285, "");
#endif
		return;
	}

	if (!ch->CanWarp() || ch->IsObserverMode() || ch->IsDead() || ch->IsStun() || ecs::GetMapIndex(ch) >= 10000) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 528, "");
#endif
		return;
	}

	str_to_number(x, arg1);
	str_to_number(y, arg2);
	PIXEL_POSITION p;
	if (!SECTREE_MANAGER::instance().GetMapBasePosition(ecs::GetX(ch), ecs::GetY(ch), p))
		return;

	if (ch->GetGold() < 1000000) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 232, "");
#endif
		return;
	} else {
		ch->PointChange(POINT_GOLD, -1000000);
		x += p.x / 100;
		y += p.y / 100;
		x *= 100;
		y *= 100;
		ch->Show(ecs::GetMapIndex(ch), x, y, z);
		ch->Stop();
		ch->SetGoToXYTime();
	}
}

#ifdef ENABLE_SAVEPOINT_SYSTEM
ACMD(do_open_savepoint) {
	if (ch->IsObserverMode()) {
		return;
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "SELECT slot, name, map, x, y FROM player.savepoint WHERE id = %u", ecs::GetPlayerID(ch));
	std::unique_ptr<SQLMsg> res(DBManager::instance().DirectQuery(query));
	if (res->Get()->uiNumRows > 0) {
		std::vector<int> stat;
		for (int i = 0; i < 6; i++) {
			stat.push_back(0);
		}

		MYSQL_ROW data;
		while ((data = mysql_fetch_row(res->Get()->pSQLResult))) {
			int c = 0;
			int id = atoi(data[c++]);
			std::string name = data[c++];
			int mapIdx = atoi(data[c++]), x = atoi(data[c++]), y = atoi(data[c++]);
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "append_savepoint %d %s %d %d %d", id, name.c_str(), mapIdx, x, y);
			stat[id] = 1;
		}

		for (int i = 0; i < 6; i++) {
			if (stat[i] == 0) {
				ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "append_savepoint %d %s %d %d %d", i, "-", 0, 0, 0);
			}
		}
	} else {
		for (int i = 0; i < 6; i++) {
			ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "append_savepoint %d %s %d %d %d", i, "-", 0, 0, 0);
		}
	}

	ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "open_savepoint");
}

ACMD(do_empty_savepoint) {
	if (ch->IsObserverMode()) {
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1) {
		return;
	}

	if (!isdigit(*arg1)) {
		return;
	}

	int slot;
	str_to_number(slot, arg1);
	if (slot < 0 || slot > 5) {
		return;
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "DELETE FROM player.savepoint WHERE id = %u AND slot = %d", ecs::GetPlayerID(ch), slot);
	std::unique_ptr<SQLMsg> res(DBManager::instance().DirectQuery(query));
	ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "update_savepoint %d %s %d %d %d", slot, "-", 0, 0, 0);
}

ACMD(do_go_savepoint) {
	if (ch->IsObserverMode()) {
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1) {
		return;
	}

	if (!isdigit(*arg1)) {
		return;
	}

	int slot;
	str_to_number(slot, arg1);
	if (slot < 0 || slot > 5) {
		return;
	}

	if (!ch->CanWarp()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 528, "");
#endif
		return; 
	}

	if (ecs::GetMapIndex(ch) > 10000) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 1288, "");
#endif
		return; 
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "SELECT g_x, g_y, map FROM player.savepoint WHERE id = %u AND slot = %d", ecs::GetPlayerID(ch), slot);
	std::unique_ptr<SQLMsg> res(DBManager::instance().DirectQuery(query));
	if (res->Get()->uiNumRows > 0) {
		MYSQL_ROW data;
		while ((data = mysql_fetch_row(res->Get()->pSQLResult))) {
			int c = 0;
			int x = atoi(data[c++]), y = atoi(data[c++]), mapIdx = atoi(data[c++]);

			if (mapIdx == ecs::GetMapIndex(ch)) {
				int x2 = x - ecs::GetX(ch);
				int y2 = y - ecs::GetY(ch);
				double nDist = 0;
				const double nDistant = 5000.0;
				nDist = sqrt(pow((float)x2, 2) + pow((float)y2, 2));
				if (nDistant > nDist) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 1287, "");
#endif
					return;
				}
			}
			
			ch->WarpSet(x, y);
		}
	}
}

ACMD(do_save_savepoint) {
	if (ch->IsObserverMode()) {
		return;
	}

	char arg1[256];
	char arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	if (!*arg1) {
		return;
	}

	if (!isdigit(*arg1)) {
		return;
	}

	int slot;
	str_to_number(slot, arg1);
	if (slot < 0 || slot > 5) {
		return;
	}

	int len = strlen(arg2);
	if (len < 0 || len > 8) {
		return;
	}

	int iPulse = thecore_pulse();
	if (iPulse - ch->GetSavePointTime() < PASSES_PER_SEC(10)) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 1286, "");
#endif
		return;
	}

	std::string name = arg2;
	if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_") != std::string::npos || name.find_first_not_of(' ') == std::string::npos) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 1289, "");
#endif
		return;
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "SELECT * FROM player.savepoint WHERE id = %u AND slot = %d", ecs::GetPlayerID(ch), slot);
	std::unique_ptr<SQLMsg> res(DBManager::instance().DirectQuery(query));
	if (res->Get()->uiNumRows > 0) {
		sys_err("%s savepoint slot (%d) is not empty. Maybe a hacker?", ecs::GetName(ch), slot);
	} else {
		int mapIdx = ecs::GetMapIndex(ch);
		if (mapIdx > 10000) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 531, "");
#endif
			return;
		}

		int arrayMapIndexBlocked[] = {81, 103, 105, 110, 113, 111};
		for (int i = 0; i < (int)_countof(arrayMapIndexBlocked); i++) {
			if (mapIdx == arrayMapIndexBlocked[i]) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(ch, CHAT_TYPE_INFO, 531, "");
#endif
				return;
			}
		}

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIdx);
		if (!map) {
			return;
		}

		int x = (ecs::GetX(ch) - map->m_setting.iBaseX) / 100;
		int y = (ecs::GetY(ch) - map->m_setting.iBaseY) / 100;
		PIXEL_POSITION pos = ch->GetXYZ();

		char query2[512] = {0};
		snprintf(query2, sizeof(query2), "INSERT INTO player.savepoint (id, slot, name, map, x, y, g_x, g_y) VALUES(%u, %d, '%s', %d, %d, %d, %d, %d)", ecs::GetPlayerID(ch), slot, name.c_str(), mapIdx, x, y, pos.x, pos.y);
		std::unique_ptr<SQLMsg> res2(DBManager::instance().DirectQuery(query2));
		ecs::ChatSystem::Send(ch, CHAT_TYPE_COMMAND, "update_savepoint %d %s %d %d %d", slot, name.c_str(), mapIdx, x, y);
		ch->SetSavePointTime();
	}
}
#endif

#ifdef ENABLE_CHOOSE_DOCTRINE_GUI
ACMD(do_doctrine_choose) {
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1 || ecs::GetLevel(ch) < 5 || ch->GetSkillGroup() != 0) {
		return;
	}

	int group;
	str_to_number(group, arg1);
	if (group >= 1 && group <= 2) {
		ch->SetSkillGroup(group);
		ch->ClearSkill();
		//ch->SetSkillLevel(122, 2);
		ch->SetSkillLevel(131, 1);
		//ch->SetSkillLevel(137, 20);
		//ch->SetSkillLevel(138, 20);
		//ch->SetSkillLevel(139, 20);
		//ch->SetSkillLevel(140, 20);

		ch->SetHorseLevel(1);
		CAffect * pkAff = nullptr;
		if (!(pkAff = ch->FindAffect(AFFECT_HORSE_NAME))) {
			ch->SetQuestFlag("horse_name.valid_till", get_global_time() + 126144000);
			ch->AddAffect(AFFECT_HORSE_NAME, 0, 0, 0, 126144000, 0, true);
			std::string name = ecs::GetName(ch);
			name += " Horse";
			CHorseNameManager::instance().UpdateHorseName(ecs::GetPlayerID(ch), name.c_str(), true);

			if (ch->GetHorse()) {
				ch->HorseSummon(false, true);
				ch->HorseSummon(true, true);
			}
		}

		int job = ch->GetJob();
		if (job == JOB_ASSASSIN || job == JOB_SHAMAN) {
			if (!(pkAff = ch->FindAffect(AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_MONSTER].bPointType))) {
				ch->AddAffect(AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_MONSTER].bPointType, 10, 0, 126144000, 0, false);
			}

#ifdef ENABLE_STRONG_METIN
			if (!(pkAff = ch->FindAffect(AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_METIN].bPointType))) {
				ch->AddAffect(AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_METIN].bPointType, 10, 0, 126144000, 0, false);
			}
#endif

#ifdef ENABLE_STRONG_BOSS
			if (!(pkAff = ch->FindAffect(AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_BOSS].bPointType))) {
				ch->AddAffect(AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_BOSS].bPointType, 10, 0, 126144000, 0, false);
			}
#endif
		}

		ch->ComputePoints();
		ch->SkillLevelPacket();
	}
}
#endif

