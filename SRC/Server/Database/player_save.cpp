#include "stdafx.h"
#include "ClientManager.h"
#include "Main.h"

uint64_t CreatePlayerSaveQuery(char * pszQuery, uint64_t querySize, TPlayerTable * pkTab)
{
	uint64_t queryLen;

	queryLen = snprintf(pszQuery, querySize,
			"UPDATE player%s SET "
			"job = %d, "
			"voice = %d, "
			"dir = %d, "
			"x = %d, "
			"y = %d, "
			"z = %d, "
			"map_index = %d, "
			"exit_x = %d, "
			"exit_y = %d, "
			"exit_map_index = %d, "
			"hp = %llu, "
			"mp = %llu, "
			"stamina = %d, "
			"random_hp = %d, "
			"random_sp = %d, "
			"playtime = %d, "
			"level = %d, "
			"level_step = %d, "
			"st = %d, "
			"ht = %d, "
			"dx = %d, "
			"iq = %d, "

			"gold = %lld, "

#ifdef ENABLE_GAYA_SYSTEM
			"gaya = %d, "
#endif
			"exp = %u, "
			"stat_point = %d, "
			"skill_point = %d, "
			"sub_skill_point = %d, "
			"stat_reset_count = %d, "
			"ip = '%s', "
			"part_main = %d, "
			"part_hair = %d, "
#ifdef ENABLE_ACCE_SYSTEM
			"part_acce = %d, "
#endif
			"last_play = NOW(), "
			"skill_group = %d, "
			"alignment = %u, "
			"horse_level = %d, "
			"horse_riding = %d, "
			"horse_hp = %d, "
			"horse_hp_droptime = %llu, "
			"horse_stamina = %d, "
			"horse_skill_point = %d, "
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
			"envanter = %d, "
#endif

#ifdef ENABLE_BATTLE_PASS
			"battle_pass_end = FROM_UNIXTIME(NULLIF(%lld, 0)), "
#endif
#ifdef ENABLE_RANKING
			"r0 = %lld, "
			"r1 = %lld, "
			"r2 = %lld, "
			"r3 = %lld, "
			"r4 = %lld, "
			"r5 = %lld, "
			"r6 = %lld, "
			"r7 = %lld, "
			"r8 = %lld, "
			"r9 = %lld, "
			"r10 = %lld, "
			"r11 = %lld, "
			"r12 = %lld, "
			"r13 = %lld, "
			"r14 = %lld, "
			"r15 = %lld, "
			"r16 = %lld, "
			"r17 = %lld, "
			"r18 = %lld, "
			"r19 = %lld, "
#endif
		, GetTablePostfix(),
		pkTab->job,
		pkTab->voice,
		pkTab->dir,
		pkTab->x,
		pkTab->y,
		pkTab->z,
		pkTab->lMapIndex,
		pkTab->lExitX,
		pkTab->lExitY,
		pkTab->lExitMapIndex,
		pkTab->hp,
		pkTab->sp,
		pkTab->stamina,
		pkTab->sRandomHP,
		pkTab->sRandomSP,
		pkTab->playtime,
		pkTab->level,
		pkTab->level_step,
		pkTab->st,
		pkTab->ht,
		pkTab->dx,
		pkTab->iq,
		pkTab->gold,
#ifdef ENABLE_GAYA_SYSTEM
		pkTab->gaya,
#endif
		pkTab->exp,
		pkTab->stat_point,
		pkTab->skill_point,
		pkTab->sub_skill_point,
		pkTab->stat_reset_count,
		pkTab->ip,
		pkTab->parts[PART_MAIN],
		pkTab->parts[PART_HAIR],
#ifdef ENABLE_ACCE_SYSTEM
		pkTab->parts[PART_ACCE],
#endif
		pkTab->skill_group,
		pkTab->lAlignment,
		pkTab->horse.bLevel,
		pkTab->horse.bRiding,
		pkTab->horse.sHealth,
		static_cast<unsigned long long>(pkTab->horse.dwHorseHealthDropTime),
		pkTab->horse.sStamina,
		pkTab->horse_skill_point
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
		, pkTab->envanter
#endif
#ifdef ENABLE_BATTLE_PASS
		,(long long)pkTab->dwBattlePassEndTime
#endif
#ifdef ENABLE_RANKING
		, pkTab->lRankPoints[0],
		pkTab->lRankPoints[1],
		pkTab->lRankPoints[2],
		pkTab->lRankPoints[3],
		pkTab->lRankPoints[4],
		pkTab->lRankPoints[5],
		pkTab->lRankPoints[6],
		pkTab->lRankPoints[7],
		pkTab->lRankPoints[8],
		pkTab->lRankPoints[9],
		pkTab->lRankPoints[10],
		pkTab->lRankPoints[11],
		pkTab->lRankPoints[12],
		pkTab->lRankPoints[13],
		pkTab->lRankPoints[14],
		pkTab->lRankPoints[15],
		pkTab->lRankPoints[16],
		pkTab->lRankPoints[17],
		pkTab->lRankPoints[18],
		pkTab->lRankPoints[19]
#endif
		);

	static char text[8192 + 1];

	CDBManager::instance().EscapeString(text, pkTab->skills, sizeof(pkTab->skills));
	queryLen += snprintf(pszQuery + queryLen, querySize - queryLen, "skill_level = '%s', ", text);

	CDBManager::instance().EscapeString(text, pkTab->quickslot, sizeof(pkTab->quickslot));
	queryLen += snprintf(pszQuery + queryLen, querySize - queryLen, "quickslot = '%s' ", text);

	queryLen += snprintf(pszQuery + queryLen, querySize - queryLen, " WHERE id=%u", pkTab->id);
	return queryLen;
}
