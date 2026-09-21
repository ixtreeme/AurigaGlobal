#ifndef __INC_METIN_II_CHAR_H__
#define __INC_METIN_II_CHAR_H__

#include <unordered_map>
#include <chrono>
#include <span>
#include <vector>

#include <common/tables.h>
#include <common/CommonDefines.h>
#include <common/stl.h>
#include <entt/entity/entity.hpp>
#include "horse_rider.h"
#include "cmd.h"
#include "constants.h"
#include "affect.h"
#include "affect_flag.h"
#ifndef ENABLE_CUBE_RENEWAL_WORLDARD
#include "cube.h"
#else
#include "cuberenewal.h"
#endif
#include "mining.h"
#include "ecs/VIDRegistry.hpp"
#include "ecs/systems/AffectSystem.hpp"

#include "utils.h"
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
#include "wheel_of_destiny.h"
#endif

#ifdef ENABLE_BATTLE_PASS
#include "utils.h"
#endif

#ifdef __ATTR_TRANSFER_SYSTEM__
#include "attr_transfer.h"
#endif


using namespace std::literals::chrono_literals;


#define ENABLE_ANTI_CMD_FLOOD
#define ENABLE_OPEN_SHOP_WITH_ARMOR
enum eMountType { MOUNT_TYPE_NONE = 0, MOUNT_TYPE_NORMAL = 1, MOUNT_TYPE_COMBAT = 2, MOUNT_TYPE_MILITARY = 3 };
eMountType GetMountLevelByVnum(uint32_t dwMountVnum, bool IsNew);
const uint32_t GetRandomSkillVnum(uint8_t bJob = JOB_MAX_NUM);

	
#ifdef __ENABLE_NEW_OFFLINESHOP__
namespace offlineshop
{
		class CShopSafebox;
	class CAuction;
}
#endif
#define INSTANT_FLAG_DEATH_PENALTY		(1 << 0)
#define INSTANT_FLAG_SHOP			(1 << 1)
#define INSTANT_FLAG_EXCHANGE			(1 << 2)
#define INSTANT_FLAG_STUN			(1 << 3)
#define INSTANT_FLAG_NO_REWARD			(1 << 4)

#define AI_FLAG_NPC				(1 << 0)
#define AI_FLAG_AGGRESSIVE			(1 << 1)
#define AI_FLAG_HELPER				(1 << 2)
#define AI_FLAG_STAYZONE			(1 << 3)


extern int g_nPortalLimitTime;

enum
{
	MAIN_RACE_WARRIOR_M,
	MAIN_RACE_ASSASSIN_W,
	MAIN_RACE_SURA_M,
	MAIN_RACE_SHAMAN_W,
	MAIN_RACE_WARRIOR_W,
	MAIN_RACE_ASSASSIN_M,
	MAIN_RACE_SURA_W,
	MAIN_RACE_SHAMAN_M,
	MAIN_RACE_MAX_NUM,
};

enum
{
	POISON_LENGTH = 30,
	STAMINA_PER_STEP = 1,
	SAFEBOX_PAGE_SIZE = 9,
	AI_CHANGE_ATTACK_POISITION_TIME_NEAR = 10000,
	AI_CHANGE_ATTACK_POISITION_TIME_FAR = 1000,
	AI_CHANGE_ATTACK_POISITION_DISTANCE = 100,
	SUMMON_MONSTER_COUNT = 3,
};

enum
{
	FLY_NONE,
	FLY_EXP,
	FLY_HP_MEDIUM,
	FLY_HP_BIG,
	FLY_SP_SMALL,
	FLY_SP_MEDIUM,
	FLY_SP_BIG,
	FLY_FIREWORK1,
	FLY_FIREWORK2,
	FLY_FIREWORK3,
	FLY_FIREWORK4,
	FLY_FIREWORK5,
	FLY_FIREWORK6,
	FLY_FIREWORK_CHRISTMAS,
	FLY_CHAIN_LIGHTNING,
	FLY_HP_SMALL,
	FLY_SKILL_MUYEONG,
};

enum EDamageType
{
	DAMAGE_TYPE_NONE,
	DAMAGE_TYPE_NORMAL,
	DAMAGE_TYPE_NORMAL_RANGE,
	//��ų
	DAMAGE_TYPE_MELEE,
	DAMAGE_TYPE_RANGE,
	DAMAGE_TYPE_FIRE,
	DAMAGE_TYPE_ICE,
	DAMAGE_TYPE_ELEC,
	DAMAGE_TYPE_MAGIC,
	DAMAGE_TYPE_POISON,
	DAMAGE_TYPE_SPECIAL,
};

enum DamageFlag
{
	DAMAGE_NORMAL = (1 << 0),
	DAMAGE_POISON = (1 << 1),
	DAMAGE_DODGE = (1 << 2),
	DAMAGE_BLOCK = (1 << 3),
	DAMAGE_PENETRATE = (1 << 4),
	DAMAGE_CRITICAL = (1 << 5),
};

enum EPointTypes
{
	POINT_NONE,                 // 0
	POINT_LEVEL,                // 1
	POINT_VOICE,                // 2
	POINT_EXP,                  // 3
	POINT_NEXT_EXP,             // 4
	POINT_HP,                   // 5
	POINT_MAX_HP,               // 6
	POINT_SP,                   // 7
	POINT_MAX_SP,               // 8
	POINT_STAMINA,              // 9 
	POINT_MAX_STAMINA,          // 10 

	POINT_GOLD,                 // 11
	POINT_ST,                   // 12 
	POINT_HT,                   // 13 
	POINT_DX,                   // 14 
	POINT_IQ,                   // 15 
	POINT_DEF_GRADE,		// 16 ...
	POINT_ATT_SPEED,            // 17 
	POINT_ATT_GRADE,		// 18 
	POINT_MOV_SPEED,            // 19
	POINT_CLIENT_DEF_GRADE,	// 20 
	POINT_CASTING_SPEED,        // 21 
	POINT_MAGIC_ATT_GRADE,      // 22 
	POINT_MAGIC_DEF_GRADE,      // 23 
	POINT_EMPIRE_POINT,         // 24 
	POINT_LEVEL_STEP,           // 25 
	POINT_STAT,                 // 26 
	POINT_SUB_SKILL,		// 27
	POINT_SKILL,		// 28
	POINT_WEAPON_MIN,		// 29 
	POINT_WEAPON_MAX,		// 30 
	POINT_PLAYTIME,             // 31
	POINT_HP_REGEN,             // 32 HP 
	POINT_SP_REGEN,             // 33 SP

	POINT_BOW_DISTANCE,         // 34

	POINT_HP_RECOVERY,          // 35 
	POINT_SP_RECOVERY,          // 36 

	POINT_POISON_PCT,           // 37 
	POINT_STUN_PCT,             // 38 
	POINT_SLOW_PCT,             // 39 
	POINT_CRITICAL_PCT,         // 40 
	POINT_PENETRATE_PCT,        // 41 
	POINT_CURSE_PCT,            // 42 

	POINT_ATTBONUS_HUMAN,       // 43 
	POINT_ATTBONUS_ANIMAL,      // 44 
	POINT_ATTBONUS_ORC,         // 45 
	POINT_ATTBONUS_MILGYO,      // 46 
	POINT_ATTBONUS_UNDEAD,      // 47 
	POINT_ATTBONUS_DEVIL,       // 48 
	POINT_ATTBONUS_INSECT,      // 49 
	POINT_ATTBONUS_FIRE,        // 50 
	POINT_ATTBONUS_ICE,         // 51 
	POINT_ATTBONUS_DESERT,      // 52 
	POINT_ATTBONUS_MONSTER,     // 53 
	POINT_ATTBONUS_WARRIOR,     // 54 
	POINT_ATTBONUS_ASSASSIN,	// 55 
	POINT_ATTBONUS_SURA,		// 56 
	POINT_ATTBONUS_SHAMAN,		// 57 
	POINT_ATTBONUS_TREE,     	// 58 

	POINT_RESIST_WARRIOR,		// 59 
	POINT_RESIST_ASSASSIN,		// 60 
	POINT_RESIST_SURA,			// 61 
	POINT_RESIST_SHAMAN,		// 62 

	POINT_STEAL_HP,             // 63 
	POINT_STEAL_SP,             // 64 

	POINT_MANA_BURN_PCT,        // 65 



	POINT_DAMAGE_SP_RECOVER,    // 66 

	POINT_BLOCK,                // 67
	POINT_DODGE,                // 68

	POINT_RESIST_SWORD,         // 69
	POINT_RESIST_TWOHAND,       // 70
	POINT_RESIST_DAGGER,        // 71
	POINT_RESIST_BELL,          // 72
	POINT_RESIST_FAN,           // 73
	POINT_RESIST_BOW,           // 74 
	POINT_RESIST_FIRE,          // 75 
	POINT_RESIST_ELEC,          // 76 
	POINT_RESIST_MAGIC,         // 77 
	POINT_RESIST_WIND,          // 78 

	POINT_REFLECT_MELEE,        // 79 


	POINT_REFLECT_CURSE,		// 80 
	POINT_POISON_REDUCE,		// 8

	POINT_KILL_SP_RECOVER,		// 82 
	POINT_EXP_DOUBLE_BONUS,		// 83
	POINT_GOLD_DOUBLE_BONUS,		// 84
	POINT_ITEM_DROP_BONUS,		// 85


	POINT_POTION_BONUS,			// 86
	POINT_KILL_HP_RECOVERY,		// 87

	POINT_IMMUNE_STUN,			// 88
	POINT_IMMUNE_SLOW,			// 89
	POINT_IMMUNE_FALL,			// 90
	//////////////////

	POINT_PARTY_ATTACKER_BONUS,		// 91
	POINT_PARTY_TANKER_BONUS,		// 92

	POINT_ATT_BONUS,			// 93
	POINT_DEF_BONUS,			// 94

	POINT_ATT_GRADE_BONUS,		// 95
	POINT_DEF_GRADE_BONUS,		// 96
	POINT_MAGIC_ATT_GRADE_BONUS,	// 97
	POINT_MAGIC_DEF_GRADE_BONUS,	// 98

	POINT_RESIST_NORMAL_DAMAGE,		// 99

	POINT_HIT_HP_RECOVERY,		// 100
	POINT_HIT_SP_RECOVERY, 		// 101
	POINT_MANASHIELD,

	POINT_PARTY_BUFFER_BONUS,		// 103
	POINT_PARTY_SKILL_MASTER_BONUS,	// 104

	POINT_HP_RECOVER_CONTINUE,		// 105
	POINT_SP_RECOVER_CONTINUE,		// 106

	POINT_STEAL_GOLD,
	POINT_POLYMORPH,
	POINT_MOUNT,

	POINT_PARTY_HASTE_BONUS,
	POINT_PARTY_DEFENDER_BONUS,
	POINT_STAT_RESET_COUNT,

	POINT_HORSE_SKILL,

	POINT_MALL_ATTBONUS,
	POINT_MALL_DEFBONUS,
	POINT_MALL_EXPBONUS,
	POINT_MALL_ITEMBONUS,
	POINT_MALL_GOLDBONUS,

	POINT_MAX_HP_PCT,
	POINT_MAX_SP_PCT,

	POINT_SKILL_DAMAGE_BONUS,
	POINT_NORMAL_HIT_DAMAGE_BONUS,

	// DEFEND_BONUS_ATTRIBUTES
	POINT_SKILL_DEFEND_BONUS,
	POINT_NORMAL_HIT_DEFEND_BONUS,
	// END_OF_DEFEND_BONUS_ATTRIBUTES

	// PC_BANG_ITEM_ADD
	POINT_PC_BANG_EXP_BONUS,
	POINT_PC_BANG_DROP_BONUS,
	// END_PC_BANG_ITEM_ADD
	POINT_RAMADAN_CANDY_BONUS_EXP,

	POINT_ENERGY = 128,

	POINT_ENERGY_END_TIME = 129,

	POINT_COSTUME_ATTR_BONUS = 130,
	POINT_MAGIC_ATT_BONUS_PER = 131,
	POINT_MELEE_MAGIC_ATT_BONUS_PER = 132,


	POINT_RESIST_ICE = 133,
	POINT_RESIST_EARTH = 134,
	POINT_RESIST_DARK = 135,

	POINT_RESIST_CRITICAL = 136,
	POINT_RESIST_PENETRATE = 137,


#ifdef ENABLE_ACCE_SYSTEM
	POINT_ACCEDRAIN_RATE = 143,
#endif
#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
	POINT_RESIST_MAGIC_REDUCTION = 144,
#endif

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	POINT_INVEN = 145,
#endif


#ifdef ELEMENT_NEW_BONUSES
	POINT_ATTBONUS_ELEC = 146,
	POINT_ATTBONUS_WIND = 147,
	POINT_ATTBONUS_EARTH = 148,
	POINT_ATTBONUS_DARK = 149,
#ifdef ENABLE_NEW_BONUS_TALISMAN
	POINT_ATTBONUS_IRR_SPADA = 150,
	POINT_ATTBONUS_IRR_SPADONE = 151,
	POINT_ATTBONUS_IRR_PUGNALE = 152,
	POINT_ATTBONUS_IRR_FRECCIA = 153,
	POINT_ATTBONUS_IRR_VENTAGLIO = 154,
	POINT_ATTBONUS_IRR_CAMPANA = 155,
	POINT_RESIST_MEZZIUOMINI = 156,
	POINT_DEF_TALISMAN = 157,
	POINT_ATTBONUS_FORT_ZODIAC = 158,
#endif
#endif
#ifdef ENABLE_STRONG_METIN
	POINT_ATTBONUS_METIN = 159,
#endif
#ifdef ENABLE_STRONG_BOSS
	POINT_ATTBONUS_BOSS = 160,
#endif
#ifdef ENABLE_RESIST_MONSTER
	POINT_RESIST_MONSTER = 161,
#endif
#ifdef ENABLE_MEDI_PVM
	POINT_ATTBONUS_MEDI_PVM = 162,
#endif

#ifdef ENABLE_GAYA_SYSTEM
	POINT_GAYA = 163,
#endif

#ifdef ENABLE_BATTLE_PASS
	POINT_BATTLE_PASS_ID,
#endif
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	POINT_EXTRA_INVENTORY1 = 165,
	POINT_EXTRA_INVENTORY2,
	POINT_EXTRA_INVENTORY3,
	POINT_EXTRA_INVENTORY4,
	POINT_EXTRA_INVENTORY5,
	POINT_EXTRA_INVENTORY6,
#endif
	POINT_PVM_CRITICAL_PCT = 171,
#ifdef ENABLE_DS_RUNE
	POINT_RUNE_MONSTERS = 172,
#endif
#ifdef ENABLE_NEW_COMMON_BONUSES
	POINT_DOUBLE_DROP_ITEM = 173,
	POINT_IRR_WEAPON_DEFENSE = 174,
#endif
	POINT_FISHING_RARE = 175,
#ifdef ENABLE_NEW_USE_POTION
	POINT_PARTY_DROPEXP = 176,
#endif

	//POINT_MAX_NUM = 129	common/length.h
		POINT_ALIGNMENT_HP,
		POINT_ALIGNMENT_MONSTER,
		POINT_ALIGNMENT_HUMAN,
		POINT_ALIGNMENT_METIN,
		POINT_ALIGNMENT_BOSS,
		POINT_ALIGNMENT_PVM,

};

enum EPKModes
{
	PK_MODE_PEACE,
	PK_MODE_REVENGE,
	PK_MODE_FREE,
	PK_MODE_PROTECT,
	PK_MODE_GUILD,
	PK_MODE_MAX_NUM
};

enum EPositions
{
	POS_DEAD,
	POS_SLEEPING,
	POS_RESTING,
	POS_SITTING,
	POS_FISHING,
	POS_FIGHTING,
	POS_MOUNTING,
	POS_STANDING
};

enum EBlockAction
{
	BLOCK_EXCHANGE = (1 << 0),
	BLOCK_PARTY_INVITE = (1 << 1),
	BLOCK_GUILD_INVITE = (1 << 2),
	BLOCK_WHISPER = (1 << 3),
	BLOCK_MESSENGER_INVITE = (1 << 4),
	BLOCK_PARTY_REQUEST = (1 << 5),
};

// <Factor> Dynamically evaluated CHARACTER* equivalent.
// Referring to SCharDeadEventInfo.



typedef struct character_point
{

	int64_t			points[POINT_MAX_NUM];

	uint8_t			job;
	uint8_t			voice;

#ifdef ENABLE_GAYA_SYSTEM
	int				gaya;
#endif

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	int 			envanter;
#endif

	int				iRandomHP;
	int				iRandomSP;

	uint8_t			skill_group;
} CHARACTER_POINT;


#define TRIGGERPARAM		entt::entity ch, entt::entity causer


EVENTINFO(char_event_info)
{
	entt::entity ch { entt::null };
};

typedef std::map<entt::entity, size_t> target_map;
struct TSkillUseInfo
{
	int	    iHitCount;
	int	    iMaxHitCount;
	int	    iSplashCount;
	uint32_t   dwNextSkillUsableTime;
	int	    iRange;
	bool    bUsed;
	entt::entity   dwVID;
	bool    isGrandMaster;

	target_map TargetVIDMap;

	TSkillUseInfo()
		: iHitCount(0), iMaxHitCount(0), iSplashCount(0), dwNextSkillUsableTime(0), iRange(0), bUsed(false),
		dwVID(entt::null), isGrandMaster(false)
	{
	}

	bool    HitOnce(uint32_t dwVnum = 0);

	bool    UseSkill(bool isGrandMaster, entt::entity vid, uint32_t dwCooltime, int splashcount = 1, int hitcount = -1, int range = -1);
	entt::entity   GetMainTargetVID() const { return dwVID; }
	void    SetMainTargetVID(entt::entity vid) { dwVID = vid; }
	void    ResetHitCount() { if (iSplashCount) { iHitCount = iMaxHitCount; iSplashCount--; } }
};

typedef struct packet_party_update TPacketGCPartyUpdate;
class CSkillProto;
class CWarMap;
class CAffect;
class CGuild;
class CArena;



class CMob;
class CMobInstance;
typedef struct SMobSkillInfo TMobSkillInfo;

//SKILL_POWER_BY_LEVEL
extern int GetSkillPowerByLevelFromType(int job, int skillgroup, int skilllevel);
//END_SKILL_POWER_BY_LEVEL


ESex GET_SEX(entt::entity ch);

#ifdef ENABLE_BLOCK_MULTIFARM
EVENTINFO(drop_event_info) {
	entt::entity ch { entt::null };
	time_t time;
	bool drop;
};
#endif

#ifdef ENABLE_NEW_FISHING_SYSTEM
EVENTINFO(fishingnew_event_info)
{
	uint32_t pid, vnum, chance, sec;
	fishingnew_event_info() : pid(0), vnum(0), chance(0), sec(0) {}
};
#endif
#endif
