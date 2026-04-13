#ifndef __INC_METIN_II_GAME_CONSTANTS_H__
#define __INC_METIN_II_GAME_CONSTANTS_H__

#include <span>
#include <common/tables.h>

typedef struct SMobRankStat
{
	int iGoldPercent;   // µ·ÀÌ ³ª¿Ã È®·ü
} TMobRankStat;

typedef struct SMobStat
{
	uint8_t	byLevel;
	uint16_t	HP;
	uint32_t	dwExp;
	uint16_t	wDefGrade;
} TMobStat;

typedef struct SBattleTypeStat
{
	int		AttGradeBias;
	int		DefGradeBias;
	int		MagicAttGradeBias;
	int		MagicDefGradeBias;
} TBattleTypeStat;

typedef struct SJobInitialPoints
{
	uint8_t		st, ht, dx, iq;
	int		max_hp, max_sp;
	int		hp_per_ht, sp_per_iq;
	int		hp_per_lv_begin, hp_per_lv_end;
	int		sp_per_lv_begin, sp_per_lv_end;
	int		max_stamina;
	int		stamina_per_con;
	int		stamina_per_lv_begin, stamina_per_lv_end;
} TJobInitialPoints;

typedef struct __coord
{
	int		x, y;
} Coord;

typedef struct SApplyInfo
{
	uint8_t	bPointType;                          // APPLY -> POINT
} TApplyInfo;

enum {
	FORTUNE_BIG_LUCK,
	FORTUNE_LUCK,
	FORTUNE_SMALL_LUCK,
	FORTUNE_NORMAL,
	FORTUNE_SMALL_BAD_LUCK,
	FORTUNE_BAD_LUCK,
	FORTUNE_BIG_BAD_LUCK,
	FORTUNE_MAX_NUM,
};

const int STONE_INFO_MAX_NUM = 31;
const int STONE_LEVEL_MAX_NUM = 4;

struct SStoneDropInfo
{
	uint32_t dwMobVnum;
	int iDropPct;
	int iLevelPct[STONE_LEVEL_MAX_NUM+1];
};

inline bool operator < (const SStoneDropInfo& l, uint32_t r)
{
	return l.dwMobVnum < r;
}

inline bool operator < (uint32_t l, const SStoneDropInfo& r)
{
	return l < r.dwMobVnum;
}

inline bool operator < (const SStoneDropInfo& l, const SStoneDropInfo& r)
{
	return l.dwMobVnum < r.dwMobVnum;
}

const int STONE_STOFA_INFO_MAX_NUM = 12;
const int STOFA_MAX_NUM = 7;

struct SStofaDropInfo
{
	uint32_t	dwMobVnum;
	int		iChance[STOFA_MAX_NUM];
};

inline bool operator < (const SStofaDropInfo& l, uint32_t r)
{
	return l.dwMobVnum < r;
}

inline bool operator < (uint32_t l, const SStofaDropInfo& r)
{
	return l < r.dwMobVnum;
}

inline bool operator < (const SStofaDropInfo& l, const SStofaDropInfo& r)
{
	return l.dwMobVnum < r.dwMobVnum;
}

const int STONE_SACCA_INFO_MAX_NUM = 25;
const int SACCA_MAX_NUM = 3;

struct SSaccaDropInfo
{
	uint32_t	dwMobVnum;
	int		iChance[SACCA_MAX_NUM];
};

inline bool operator < (const SSaccaDropInfo& l, uint32_t r)
{
	return l.dwMobVnum < r;
}

inline bool operator < (uint32_t l, const SSaccaDropInfo& r)
{
	return l < r.dwMobVnum;
}

inline bool operator < (const SSaccaDropInfo& l, const SSaccaDropInfo& r)
{
	return l.dwMobVnum < r.dwMobVnum;
}

extern const TApplyInfo		aApplyInfo[MAX_APPLY_NUM];
extern const TMobRankStat       MobRankStats[MOB_RANK_MAX_NUM];

extern TBattleTypeStat		BattleTypeStats[BATTLE_TYPE_MAX_NUM];

extern const uint32_t		party_exp_distribute_table[PLAYER_EXP_TABLE_MAX + 1];

extern const uint32_t		exp_table_common[PLAYER_MAX_LEVEL_CONST + 1];

extern const uint32_t*		exp_table;
#ifdef __NEWPET_SYSTEM__
//extern const uint32_t pets;
extern uint32_t		exppet_table_common[121];
extern uint32_t*		exppet_table;
extern const uint32_t		Pet_SKill_TablePerc[4][20];
extern const uint32_t		Pet_Skill_Table[12][23];
extern const uint32_t		Pet_Table[11][2];
#endif
extern const uint32_t		guild_exp_table[GUILD_MAX_LEVEL + 1];
extern const uint32_t		guild_exp_table2[GUILD_MAX_LEVEL + 1];

#define MAX_EXP_DELTA_OF_LEV	31
#define PERCENT_LVDELTA(me, victim) aiPercentByDeltaLev[MINMAX(0, (victim + 15) - me, MAX_EXP_DELTA_OF_LEV - 1)]
#define PERCENT_LVDELTA_BOSS(me, victim) aiPercentByDeltaLevForBoss[MINMAX(0, (victim + 15) - me, MAX_EXP_DELTA_OF_LEV - 1)]
#define CALCULATE_VALUE_LVDELTA(me, victim, val) ((val * PERCENT_LVDELTA(me, victim)) / 100)
extern const int		aiPercentByDeltaLev_euckr[MAX_EXP_DELTA_OF_LEV];
extern const int		aiPercentByDeltaLevForBoss_euckr[MAX_EXP_DELTA_OF_LEV];
extern const int *		aiPercentByDeltaLev;
extern const int *		aiPercentByDeltaLevForBoss;

#define ARROUND_COORD_MAX_NUM	161
extern Coord			aArroundCoords[ARROUND_COORD_MAX_NUM];
extern TJobInitialPoints	JobInitialPoints[JOB_MAX_NUM];

extern const int		aiMobEnchantApplyIdx[MOB_ENCHANTS_MAX_NUM];
extern const int		aiMobResistsApplyIdx[MOB_RESISTS_MAX_NUM];

extern const int		aSkillAttackAffectProbByRank[MOB_RANK_MAX_NUM];

extern const int aiItemMagicAttributePercentHigh[ITEM_ATTRIBUTE_MAX_LEVEL]; // 1°³±îÁö
extern const int aiItemMagicAttributePercentLow[ITEM_ATTRIBUTE_MAX_LEVEL];

extern const int aiItemAttributeAddPercent[ITEM_ATTRIBUTE_MAX_NUM];

extern const int aiWeaponSocketQty[WEAPON_NUM_TYPES];
extern const int aiArmorSocketQty[ARMOR_NUM_TYPES];
extern const int aiSocketPercentByQty[5][4];

extern const int aiExpLossPercents[PLAYER_EXP_TABLE_MAX + 1];

extern const int * aiSkillPowerByLevel;
extern const int aiSkillPowerByLevel_euckr[SKILL_MAX_LEVEL + 1];

extern const int aiPolymorphPowerByLevel[SKILL_MAX_LEVEL + 1];

extern const int aiSkillBookCountForLevelUp[10];
extern const int aiGrandMasterSkillBookCountForLevelUp[10];
extern const int aiGrandMasterSkillBookMinCount[10];
extern const int aiGrandMasterSkillBookMaxCount[10];
extern const int CHN_aiPartyBonusExpPercentByMemberCount[9];
extern const int KOR_aiPartyBonusExpPercentByMemberCount[9];
extern const int KOR_aiUniqueItemPartyBonusExpPercentByMemberCount[9];

typedef std::map<uint32_t, TItemAttrTable> TItemAttrMap;
extern TItemAttrMap g_map_itemAttr;
extern TItemAttrMap g_map_itemRare;

extern const int * aiChainLightningCountBySkillLevel;
extern const int aiChainLightningCountBySkillLevel_euckr[SKILL_MAX_LEVEL + 1];

extern const char * c_apszEmpireNames[EMPIRE_MAX_NUM];
extern const char * c_apszPrivNames[MAX_PRIV_NUM];

extern const SStoneDropInfo aStoneDrop[STONE_INFO_MAX_NUM];

#ifdef ENABLE_GUILD_ATTRIBUTE

	struct GuildAttr { uint8_t apply; int32_t value; };
	std::span<const GuildAttr> GetGuildAttributesByLevel(uint8_t level);

#endif


extern const SStofaDropInfo aStofaDrop[STONE_STOFA_INFO_MAX_NUM];
extern const SSaccaDropInfo aSaccaDrop[STONE_SACCA_INFO_MAX_NUM];

extern std::string m_closeText[LANGUAGE_MAX_NUM];
extern std::string m_horseText[LANGUAGE_MAX_NUM];

typedef struct
{
	int32_t lMapIndex;
	int iWarPrice;
	int iWinnerPotionRewardPctToWinner;
	int iLoserPotionRewardPctToWinner;
	int iInitialScore;
	int iEndScore;
} TGuildWarInfo;

extern TGuildWarInfo KOR_aGuildWarInfo[GUILD_WAR_TYPE_MAX_NUM];

// ACCESSORY_REFINE
enum
{
	ITEM_ACCESSORY_SOCKET_MAX_NUM = 3
};

extern const int aiAccessorySocketAddPct[ITEM_ACCESSORY_SOCKET_MAX_NUM];
extern const int aiAccessorySocketEffectivePct[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1];
extern const int aiAccessorySocketDegradeTime[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1];
extern const int aiAccessorySocketPutPct[ITEM_ACCESSORY_SOCKET_MAX_NUM + 1];
int32_t FN_get_apply_type(const char *apply_type_string);

// END_OF_ACCESSORY_REFINE


#endif

