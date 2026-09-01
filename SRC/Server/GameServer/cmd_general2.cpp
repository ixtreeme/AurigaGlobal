#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/AIHelpers.hpp"
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
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->BlockDrop();
}

ACMD(do_drop_unblock) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->UnblockDrop();
}
#endif

ACMD(do_remove_affect)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
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
				if (AffectSystem::IsAffectFlag(character, AFF_JEONGWIHON))
					AffectSystem::RemoveAffect(character, SKILL_JEONGWI);
			}
			break;
		case AFF_GEOMGYEONG:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_GEOMGYEONG))
					AffectSystem::RemoveAffect(character, SKILL_GEOMKYUNG);
			}
			break;
		case AFF_CHEONGEUN:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_CHEONGEUN))
					AffectSystem::RemoveAffect(character, SKILL_CHUNKEON);
			}
			break;
		case AFF_GYEONGGONG:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_GYEONGGONG))
					AffectSystem::RemoveAffect(character, SKILL_GYEONGGONG);
			}
			break;
		case AFF_GWIGUM:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_GWIGUM))
					AffectSystem::RemoveAffect(character, SKILL_GWIGEOM);
			}
			break;
		case AFF_TERROR:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_TERROR))
					AffectSystem::RemoveAffect(character, SKILL_TERROR);
			}
			break;
		case AFF_JUMAGAP:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_JUMAGAP))
					AffectSystem::RemoveAffect(character, SKILL_JUMAGAP);
			}
			break;
		case AFF_MUYEONG:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_MUYEONG))
					AffectSystem::RemoveAffect(character, SKILL_MUYEONG);
			}
			break;
		case AFF_MANASHIELD:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_MANASHIELD))
					AffectSystem::RemoveAffect(character, SKILL_MANASHILED);
			}
			break;
		case AFF_HOSIN:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_HOSIN))
					AffectSystem::RemoveAffect(character, SKILL_HOSIN);
			}
			break;
		case AFF_BOHO:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_BOHO))
					AffectSystem::RemoveAffect(character, SKILL_REFLECT);
			}
			break;
		case AFF_GICHEON:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_GICHEON))
					AffectSystem::RemoveAffect(character, SKILL_GICHEON);
			}
			break;
		case AFF_KWAESOK:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_KWAESOK))
					AffectSystem::RemoveAffect(character, SKILL_KWAESOK);
			}
			break;
		case AFF_JEUNGRYEOK:
			{
				if (AffectSystem::IsAffectFlag(character, AFF_JEUNGRYEOK))
					AffectSystem::RemoveAffect(character, SKILL_JEUNGRYEOK);
			}
			break;
		default:
			return;
	}
}

ACMD(do_stat2)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;

	if (ch->IsPolymorphed()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 314, "");
#endif
		return;
	}


	int64_t limit = 10;


	if (ecs::PointSystem::Get(character, POINT_STAT) < limit) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 851,

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

	if (ecs::PointSystem::GetReal(character, idx) >= MAX_STATUS_ALTERNATIVE)
		return;

	limit = ecs::PointSystem::GetReal(character, idx) + limit >= MAX_STATUS_ALTERNATIVE ? MAX_STATUS_ALTERNATIVE - ecs::PointSystem::GetReal(character, idx) : limit;
	ch->SetRealPoint(idx, ecs::PointSystem::GetReal(character, idx) + limit);
	ch->SetPoint(idx, ecs::PointSystem::Get(character, idx) + limit);
	ch->ComputePoints();
	ecs::PointSystem::Change(character, idx, 0);

	if (idx == POINT_IQ) {
		ecs::PointSystem::Change(character, POINT_MAX_HP, 0);
	}
	else if (idx == POINT_HT) {
		ecs::PointSystem::Change(character, POINT_MAX_SP, 0);
	}

	ecs::PointSystem::Change(character, POINT_STAT, -limit);
	ch->ComputePoints();
}

#ifdef ENABLE_BIOLOGIST_UI
ACMD(do_open_biologist) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch)
		return;

	int stat = ecs::QuestSystem::GetFlag(character, "biologist.stat");
	if (stat > 15) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 867, "");
#endif
		return;
	}
	int min = biologistMissionInfo[stat][12];
	if ((ecs::PointSystem::GetLevel(character)) < min) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 861, "%d", min);
#endif
		return;
	}
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_reward %d#%d#%d#%d#%d#%d#%d#%d#%d", biologistMissionInfo[stat][11], biologistMissionInfo[stat][3], biologistMissionInfo[stat][4], biologistMissionInfo[stat][5], biologistMissionInfo[stat][6], biologistMissionInfo[stat][7], biologistMissionInfo[stat][8], biologistMissionInfo[stat][9], biologistMissionInfo[stat][10]);
	int time = ecs::QuestSystem::GetFlag(character, "biologist.time");
	time = time > 0 ? time : 0;
	int count = ecs::QuestSystem::GetFlag(character, "biologist.delivered");
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist %d#%d#%d#%d#%d#%d#%d", stat, biologistMissionInfo[stat][0], biologistMissionInfo[stat][1], count, biologistMissionInfo[stat][2], time, biologistMissionInfo[stat][1]-count > 0 ? 0 : 1);
}

ACMD(do_delivery_biologist) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch)
		return;

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;

	int stat = ecs::QuestSystem::GetFlag(character, "biologist.stat");
	if (stat > 15) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 867, "");
#endif
		return;
	}
	int min = biologistMissionInfo[stat][12];
	if ((ecs::PointSystem::GetLevel(character)) < min) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 861, "%d", min);
#endif
		return;
	}

	int count = ecs::QuestSystem::GetFlag(character, "biologist.delivered");
	if (count >= biologistMissionInfo[stat][1]) {
		return;
	}

	int vnum = biologistMissionInfo[stat][0];
	if (ch->CountSpecifyItem(vnum) <= 0) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 866, "");
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

	int time = ecs::QuestSystem::GetFlag(character, "biologist.time") ;
	if (time > 0 && time - get_global_time() > 0 && !elisir && !potion) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 871, "");
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
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 863, "");
			} else {
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 862, "");
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

		ecs::QuestSystem::SetFlag(character, "biologist.delivered", count + 1);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 865, "");
#endif
		if (waittime != 0 && count + 1 >= biologistMissionInfo[stat][1]) {
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_delivered 1#%d#%d", count + 1, waittime - get_global_time());
		} else {
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_delivered 0#%d#%d", count + 1, waittime - get_global_time());
		}
	}
	else {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 864, "");
#endif
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_delivered 0#%d#%d", count, waittime - get_global_time());
	}
	ecs::QuestSystem::SetFlag(character, "biologist.time", waittime);
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_time %d", waittime);
}

ACMD(do_reward_biologist) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	int stat = ecs::QuestSystem::GetFlag(character, "biologist.stat");
	if (stat > 15) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 867, "");
#endif
		return;
	}
	int count = ecs::QuestSystem::GetFlag(character, "biologist.delivered");
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
	ecs::QuestSystem::SetFlag(character, "biologist.stat", newstat);
	ecs::QuestSystem::SetFlag(character, "biologist.time", 0);
	ecs::QuestSystem::SetFlag(character, "biologist.delivered", 0);
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
					AffectSystem::AddAffect(character, biologistMissionInfo[stat][14], bApplyOn, lApplyValue, 0, 315360000, 0, false);
				}
			}
		} else {
			iarg1 += 1;
			int j = 2 * iarg1;
			uint8_t bApplyOn = biologistMissionInfo[stat][j + 1];
			int32_t lApplyValue = biologistMissionInfo[stat][j + 2];
			if (bApplyOn != APPLY_NONE || lApplyValue != 0) {
				bApplyOn = aApplyInfo[bApplyOn].bPointType;
				AffectSystem::AddAffect(character, biologistMissionInfo[stat][14], bApplyOn, lApplyValue, 0, 315360000, 0, false);
			}
		}
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 870, "");
#endif
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 868, "");
#endif
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_close");
		return;
	}
	else if (newstat > 15) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 868, "");
#endif
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_close");
		return;
	} else {
		int min = biologistMissionInfo[newstat][12];
		if ((ecs::PointSystem::GetLevel(character)) < min) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 869, "%d", min);
#endif
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_close");
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
						AffectSystem::AddAffect(character, biologistMissionInfo[stat][14], bApplyOn, lApplyValue, 0, 315360000, 0, false);
					}
				}
			} else {
				iarg1 += 1;
				int j = 2 * iarg1;
				uint8_t bApplyOn = biologistMissionInfo[stat][j + 1];
				int32_t lApplyValue = biologistMissionInfo[stat][j + 2];
				if (bApplyOn != APPLY_NONE || lApplyValue != 0) {
					bApplyOn = aApplyInfo[bApplyOn].bPointType;
					AffectSystem::AddAffect(character, biologistMissionInfo[stat][14], bApplyOn, lApplyValue, 0, 315360000, 0, false);
				}
			}

#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 870, "");
#endif
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_reward %d#%d#%d#%d#%d#%d#%d#%d#%d", biologistMissionInfo[newstat][11], biologistMissionInfo[newstat][3], biologistMissionInfo[newstat][4], biologistMissionInfo[newstat][5], biologistMissionInfo[newstat][6], biologistMissionInfo[newstat][7], biologistMissionInfo[newstat][8], biologistMissionInfo[newstat][9], biologistMissionInfo[newstat][10]);
			int time = ecs::QuestSystem::GetFlag(character, "biologist.time");
			time = time > 0 ? time - get_global_time() : 0;
			int count = ecs::QuestSystem::GetFlag(character, "biologist.delivered");
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologist_next %d#%d#%d#%d#%d#%d#%d", newstat, biologistMissionInfo[newstat][0], biologistMissionInfo[newstat][1], count, biologistMissionInfo[newstat][2], time, biologistMissionInfo[newstat][1]-count > 0 ? 0 : 1);
		}
	}
}

ACMD(do_change_biologist) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
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

	int stat = ecs::QuestSystem::GetFlag(character, "biologist.stat");
	if (stat <= iarg1)
		return;

	iarg2 = 3 + (iarg2 * 2);
	int type =  aApplyInfo[biologistMissionInfo[iarg1][iarg2]].bPointType;
	int idx = biologistMissionInfo[iarg1][14];
	CAffect * pkAff = AffectSystem::FindAffect(character, idx, type);
	if (pkAff) {
		return;
	}
	else {
		if (ch->CountSpecifyItem(164401) <= 0) {
			return;
		}

		ch->RemoveSpecifyItem(164401, 1);
		AffectSystem::RemoveAffect(character, idx);
		AffectSystem::AddAffect(character, idx, type, biologistMissionInfo[iarg1][iarg2 + 1], 0, 315360000, 0, false);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 873, "");
#endif
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologistch_close");
	}
}
#endif

ACMD(do_gotoxy)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
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
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1285, "");
#endif
		return;
	}

	if (!ecs::PlayerRuntime::CanWarp(character) || ecs::PlayerRuntime::IsObserverMode(character) || CombatSystem::IsDead(character) || CombatSystem::IsStun(character) || ecs::PlayerRuntime::GetMapIndex(character) >= 10000) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 528, "");
#endif
		return;
	}

	str_to_number(x, arg1);
	str_to_number(y, arg2);
	PIXEL_POSITION p;
	if (!SECTREE_MANAGER::instance().GetMapBasePosition(ecs::PlayerRuntime::GetX(character), ecs::PlayerRuntime::GetY(character), p))
		return;

	if (ecs::PointSystem::GetGold(character) < 1000000) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 232, "");
#endif
		return;
	} else {
		ecs::PointSystem::Change(character, POINT_GOLD, -1000000);
		x += p.x / 100;
		y += p.y / 100;
		x *= 100;
		y *= 100;
		ecs::MovementSystem::Show(character, ecs::PlayerRuntime::GetMapIndex(character), x, y, z);
		ecs::MovementSystem::Stop(character);
		ch->SetGoToXYTime();
	}
}

#ifdef ENABLE_SAVEPOINT_SYSTEM
ACMD(do_open_savepoint) {
	if (ecs::PlayerRuntime::IsObserverMode(character)) {
		return;
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "SELECT slot, name, map, x, y FROM player.savepoint WHERE id = %u", (ecs::PlayerRuntime::GetPlayerID(character)));
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
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "append_savepoint %d %s %d %d %d", id, name.c_str(), mapIdx, x, y);
			stat[id] = 1;
		}

		for (int i = 0; i < 6; i++) {
			if (stat[i] == 0) {
				ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "append_savepoint %d %s %d %d %d", i, "-", 0, 0, 0);
			}
		}
	} else {
		for (int i = 0; i < 6; i++) {
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "append_savepoint %d %s %d %d %d", i, "-", 0, 0, 0);
		}
	}

	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "open_savepoint");
}

ACMD(do_empty_savepoint) {
	if (ecs::PlayerRuntime::IsObserverMode(character)) {
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
	snprintf(query, sizeof(query), "DELETE FROM player.savepoint WHERE id = %u AND slot = %d", (ecs::PlayerRuntime::GetPlayerID(character)), slot);
	std::unique_ptr<SQLMsg> res(DBManager::instance().DirectQuery(query));
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "update_savepoint %d %s %d %d %d", slot, "-", 0, 0, 0);
}

ACMD(do_go_savepoint) {
	if (ecs::PlayerRuntime::IsObserverMode(character)) {
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

	if (!ecs::PlayerRuntime::CanWarp(character)) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 528, "");
#endif
		return;
	}

	if (ecs::PlayerRuntime::GetMapIndex(character) > 10000) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1288, "");
#endif
		return;
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "SELECT g_x, g_y, map FROM player.savepoint WHERE id = %u AND slot = %d", (ecs::PlayerRuntime::GetPlayerID(character)), slot);
	std::unique_ptr<SQLMsg> res(DBManager::instance().DirectQuery(query));
	if (res->Get()->uiNumRows > 0) {
		MYSQL_ROW data;
		while ((data = mysql_fetch_row(res->Get()->pSQLResult))) {
			int c = 0;
			int x = atoi(data[c++]), y = atoi(data[c++]), mapIdx = atoi(data[c++]);

			if (mapIdx == ecs::PlayerRuntime::GetMapIndex(character)) {
				int x2 = x - ecs::PlayerRuntime::GetX(character);
				int y2 = y - ecs::PlayerRuntime::GetY(character);
				double nDist = 0;
				const double nDistant = 5000.0;
				nDist = sqrt(pow((float)x2, 2) + pow((float)y2, 2));
				if (nDistant > nDist) {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1287, "");
#endif
					return;
				}
			}

			ecs::MovementSystem::WarpSet(character, x, y);
		}
	}
}

ACMD(do_save_savepoint) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ecs::PlayerRuntime::IsObserverMode(character)) {
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
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1286, "");
#endif
		return;
	}

	std::string name = arg2;
	if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_") != std::string::npos || name.find_first_not_of(' ') == std::string::npos) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 1289, "");
#endif
		return;
	}

	char query[512] = {0};
	snprintf(query, sizeof(query), "SELECT * FROM player.savepoint WHERE id = %u AND slot = %d", (ecs::PlayerRuntime::GetPlayerID(character)), slot);
	std::unique_ptr<SQLMsg> res(DBManager::instance().DirectQuery(query));
	if (res->Get()->uiNumRows > 0) {
		LOG_ERROR("{} savepoint slot ({}) is not empty. Maybe a hacker?", ecs::PlayerRuntime::GetName(character).data(), slot);
	} else {
		int mapIdx = ecs::PlayerRuntime::GetMapIndex(character);
		if (mapIdx > 10000) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 531, "");
#endif
			return;
		}

		int arrayMapIndexBlocked[] = {81, 103, 105, 110, 113, 111};
		for (int i = 0; i < (int)_countof(arrayMapIndexBlocked); i++) {
			if (mapIdx == arrayMapIndexBlocked[i]) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 531, "");
#endif
				return;
			}
		}

		LPSECTREE_MAP map = SECTREE_MANAGER::instance().GetMap(mapIdx);
		if (!map) {
			return;
		}

		int x = (ecs::PlayerRuntime::GetX(character) - map->m_setting.iBaseX) / 100;
		int y = (ecs::PlayerRuntime::GetY(character) - map->m_setting.iBaseY) / 100;
		PIXEL_POSITION pos = ch->GetXYZ();

		char query2[512] = {0};
		snprintf(query2, sizeof(query2), "INSERT INTO player.savepoint (id, slot, name, map, x, y, g_x, g_y) VALUES(%u, %d, '%s', %d, %d, %d, %d, %d)", (ecs::PlayerRuntime::GetPlayerID(character)), slot, name.c_str(), mapIdx, x, y, pos.x, pos.y);
		std::unique_ptr<SQLMsg> res2(DBManager::instance().DirectQuery(query2));
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "update_savepoint %d %s %d %d %d", slot, name.c_str(), mapIdx, x, y);
		ch->SetSavePointTime();
	}
}
#endif

#ifdef ENABLE_CHOOSE_DOCTRINE_GUI
ACMD(do_doctrine_choose) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1 || (ecs::PointSystem::GetLevel(character)) < 5 || ch->GetSkillGroup() != 0) {
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
		if (!(pkAff = AffectSystem::FindAffect(character, AFFECT_HORSE_NAME))) {
			ecs::QuestSystem::SetFlag(character, "horse_name.valid_till", get_global_time() + 126144000);
			AffectSystem::AddAffect(character, AFFECT_HORSE_NAME, 0, 0, 0, 126144000, 0, true);
			std::string name = ecs::PlayerRuntime::GetName(character).data();
			name += " Horse";
			CHorseNameManager::instance().UpdateHorseName((ecs::PlayerRuntime::GetPlayerID(character)), name.c_str(), true);

			if (ch->GetHorse()) {
				ch->HorseSummon(false, true);
				ch->HorseSummon(true, true);
			}
		}

		int job = ch->GetJob();
		if (job == JOB_ASSASSIN || job == JOB_SHAMAN) {
			if (!(pkAff = AffectSystem::FindAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_MONSTER].bPointType))) {
				AffectSystem::AddAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_MONSTER].bPointType, 10, 0, 126144000, 0, false);
			}

#ifdef ENABLE_STRONG_METIN
			if (!(pkAff = AffectSystem::FindAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_METIN].bPointType))) {
				AffectSystem::AddAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_METIN].bPointType, 10, 0, 126144000, 0, false);
			}
#endif

#ifdef ENABLE_STRONG_BOSS
			if (!(pkAff = AffectSystem::FindAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_BOSS].bPointType))) {
				AffectSystem::AddAffect(character, AFFECT_PVM_RACE, aApplyInfo[APPLY_ATTBONUS_BOSS].bPointType, 10, 0, 126144000, 0, false);
			}
#endif
		}

		ch->ComputePoints();
		ch->SkillLevelPacket();
	}
}
#endif

