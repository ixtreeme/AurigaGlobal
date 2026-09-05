#ifndef __INC_METIN_II_CHAR_H__
#define __INC_METIN_II_CHAR_H__

#include <unordered_map>
#include <chrono>
#include <span>
#include <vector>

#include <common/tables.h>
#include <common/CommonDefines.h>
#include <common/stl.h>
#include "entity.h"
#include "horse_rider.h"
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

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
class CMountSystem;
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

	


class CBuffOnAttributes;
class CPetSystem;
#ifdef __NEWPET_SYSTEM__
class CNewPetSystem;
#endif

#ifdef __ENABLE_NEW_OFFLINESHOP__
namespace offlineshop
{
	class CShop;
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
#ifdef ENABLE_WOLFMAN_CHARACTER
	MAIN_RACE_WOLFMAN_M,
#endif
	MAIN_RACE_MAX_NUM,
};

enum
{
	POISON_LENGTH = 30,
#ifdef ENABLE_WOLFMAN_CHARACTER
	BLEEDING_LENGTH = 30,
#endif
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
#ifdef ENABLE_WOLFMAN_CHARACTER
	DAMAGE_TYPE_BLEEDING,
#endif
};

enum DamageFlag
{
	DAMAGE_NORMAL = (1 << 0),
	DAMAGE_POISON = (1 << 1),
	DAMAGE_DODGE = (1 << 2),
	DAMAGE_BLOCK = (1 << 3),
	DAMAGE_PENETRATE = (1 << 4),
	DAMAGE_CRITICAL = (1 << 5),
#if defined(ENABLE_WOLFMAN_CHARACTER) && !defined(USE_MOB_BLEEDING_AS_POISON)
	DAMAGE_BLEEDING = (1 << 6),
#endif
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
	POINT_STAMINA,              // 9  ���׹̳�
	POINT_MAX_STAMINA,          // 10 �ִ� ���׹̳�

	POINT_GOLD,                 // 11
	POINT_ST,                   // 12 �ٷ�
	POINT_HT,                   // 13 ü��
	POINT_DX,                   // 14 ��ø��
	POINT_IQ,                   // 15 ���ŷ�
	POINT_DEF_GRADE,		// 16 ...
	POINT_ATT_SPEED,            // 17 ���ݼӵ�
	POINT_ATT_GRADE,		// 18 ���ݷ� MAX
	POINT_MOV_SPEED,            // 19 �̵��ӵ�
	POINT_CLIENT_DEF_GRADE,	// 20 �����
	POINT_CASTING_SPEED,        // 21 �ֹ��ӵ� (��ٿ�Ÿ��*100) / (100 + �̰�) = ���� ��ٿ� Ÿ��
	POINT_MAGIC_ATT_GRADE,      // 22 �������ݷ�
	POINT_MAGIC_DEF_GRADE,      // 23 ��������
	POINT_EMPIRE_POINT,         // 24 ��������
	POINT_LEVEL_STEP,           // 25 �� ���������� �ܰ�.. (1 2 3 �� �� ����, 4 �Ǹ� ���� ��)
	POINT_STAT,                 // 26 �ɷ�ġ �ø� �� �ִ� ����
	POINT_SUB_SKILL,		// 27 ���� ��ų ����Ʈ
	POINT_SKILL,		// 28 ��Ƽ�� ��ų ����Ʈ
	POINT_WEAPON_MIN,		// 29 ���� �ּ� ������
	POINT_WEAPON_MAX,		// 30 ���� �ִ� ������
	POINT_PLAYTIME,             // 31 �÷��̽ð�
	POINT_HP_REGEN,             // 32 HP ȸ����
	POINT_SP_REGEN,             // 33 SP ȸ����

	POINT_BOW_DISTANCE,         // 34 Ȱ �����Ÿ� ����ġ (meter)

	POINT_HP_RECOVERY,          // 35 ü�� ȸ�� ������
	POINT_SP_RECOVERY,          // 36 ���ŷ� ȸ�� ������

	POINT_POISON_PCT,           // 37 �� Ȯ��
	POINT_STUN_PCT,             // 38 ���� Ȯ��
	POINT_SLOW_PCT,             // 39 ���ο� Ȯ��
	POINT_CRITICAL_PCT,         // 40 ũ��Ƽ�� Ȯ��
	POINT_PENETRATE_PCT,        // 41 ����Ÿ�� Ȯ��
	POINT_CURSE_PCT,            // 42 ���� Ȯ��

	POINT_ATTBONUS_HUMAN,       // 43 �ΰ����� ����
	POINT_ATTBONUS_ANIMAL,      // 44 �������� ������ % ����
	POINT_ATTBONUS_ORC,         // 45 ���Ϳ��� ������ % ����
	POINT_ATTBONUS_MILGYO,      // 46 �б����� ������ % ����
	POINT_ATTBONUS_UNDEAD,      // 47 ��ü���� ������ % ����
	POINT_ATTBONUS_DEVIL,       // 48 ����(�Ǹ�)���� ������ % ����
	POINT_ATTBONUS_INSECT,      // 49 ������
	POINT_ATTBONUS_FIRE,        // 50 ȭ����
	POINT_ATTBONUS_ICE,         // 51 ������
	POINT_ATTBONUS_DESERT,      // 52 �縷��
	POINT_ATTBONUS_MONSTER,     // 53 ��� ���Ϳ��� ����
	POINT_ATTBONUS_WARRIOR,     // 54 ���翡�� ����
	POINT_ATTBONUS_ASSASSIN,	// 55 �ڰ����� ����
	POINT_ATTBONUS_SURA,		// 56 ���󿡰� ����
	POINT_ATTBONUS_SHAMAN,		// 57 ���翡�� ����
	POINT_ATTBONUS_TREE,     	// 58 �������� ���� 20050729.myevan UNUSED5

	POINT_RESIST_WARRIOR,		// 59 ���翡�� ����
	POINT_RESIST_ASSASSIN,		// 60 �ڰ����� ����
	POINT_RESIST_SURA,			// 61 ���󿡰� ����
	POINT_RESIST_SHAMAN,		// 62 ���翡�� ����

	POINT_STEAL_HP,             // 63 ������ ����
	POINT_STEAL_SP,             // 64 ���ŷ� ����

	POINT_MANA_BURN_PCT,        // 65 ���� ��

	/// ���ؽ� ���ʽ� ///

	POINT_DAMAGE_SP_RECOVER,    // 66 ���ݴ��� �� ���ŷ� ȸ�� Ȯ��

	POINT_BLOCK,                // 67 ������
	POINT_DODGE,                // 68 ȸ����

	POINT_RESIST_SWORD,         // 69
	POINT_RESIST_TWOHAND,       // 70
	POINT_RESIST_DAGGER,        // 71
	POINT_RESIST_BELL,          // 72
	POINT_RESIST_FAN,           // 73
	POINT_RESIST_BOW,           // 74  ȭ��   ����   : ����� ����
	POINT_RESIST_FIRE,          // 75  ȭ��   ����   : ȭ�����ݿ� ���� ����� ����
	POINT_RESIST_ELEC,          // 76  ����   ����   : ������ݿ� ���� ����� ����
	POINT_RESIST_MAGIC,         // 77  ����   ����   : �������� ���� ����� ����
	POINT_RESIST_WIND,          // 78  �ٶ�   ����   : �ٶ����ݿ� ���� ����� ����

	POINT_REFLECT_MELEE,        // 79 ���� �ݻ�

	/// Ư�� ���ؽ� ///
	POINT_REFLECT_CURSE,		// 80 ���� �ݻ�
	POINT_POISON_REDUCE,		// 81 �������� ����

	/// �� �Ҹ�� ///
	POINT_KILL_SP_RECOVER,		// 82 �� �Ҹ�� MP ȸ��
	POINT_EXP_DOUBLE_BONUS,		// 83
	POINT_GOLD_DOUBLE_BONUS,		// 84
	POINT_ITEM_DROP_BONUS,		// 85

	/// ȸ�� ���� ///
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
	POINT_MANASHIELD,			// 102 ��ż�ȣ ��ų�� ���� �������� ȿ�� ����

	POINT_PARTY_BUFFER_BONUS,		// 103
	POINT_PARTY_SKILL_MASTER_BONUS,	// 104

	POINT_HP_RECOVER_CONTINUE,		// 105
	POINT_SP_RECOVER_CONTINUE,		// 106

	POINT_STEAL_GOLD,			// 107
	POINT_POLYMORPH,			// 108 ������ ���� ��ȣ
	POINT_MOUNT,			// 109 Ÿ���ִ� ���� ��ȣ

	POINT_PARTY_HASTE_BONUS,		// 110
	POINT_PARTY_DEFENDER_BONUS,		// 111
	POINT_STAT_RESET_COUNT,		// 112 ���� �ܾ� ����� ���� ���� ���� ����Ʈ (1�� 1����Ʈ ���°���)

	POINT_HORSE_SKILL,			// 113

	POINT_MALL_ATTBONUS,		// 114 ���ݷ� +x%
	POINT_MALL_DEFBONUS,		// 115 ���� +x%
	POINT_MALL_EXPBONUS,		// 116 ����ġ +x%
	POINT_MALL_ITEMBONUS,		// 117 ������ ����� x/10��
	POINT_MALL_GOLDBONUS,		// 118 �� ����� x/10��

	POINT_MAX_HP_PCT,			// 119 �ִ������ +x%
	POINT_MAX_SP_PCT,			// 120 �ִ����ŷ� +x%

	POINT_SKILL_DAMAGE_BONUS,		// 121 ��ų ������ *(100+x)%
	POINT_NORMAL_HIT_DAMAGE_BONUS,	// 122 ��Ÿ ������ *(100+x)%

	// DEFEND_BONUS_ATTRIBUTES
	POINT_SKILL_DEFEND_BONUS,		// 123 ��ų ��� ������
	POINT_NORMAL_HIT_DEFEND_BONUS,	// 124 ��Ÿ ��� ������
	// END_OF_DEFEND_BONUS_ATTRIBUTES

	// PC_BANG_ITEM_ADD
	POINT_PC_BANG_EXP_BONUS,		// 125 PC�� ���� ����ġ ���ʽ�
	POINT_PC_BANG_DROP_BONUS,		// 126 PC�� ���� ��ӷ� ���ʽ�
	// END_PC_BANG_ITEM_ADD
	POINT_RAMADAN_CANDY_BONUS_EXP,			// �󸶴� ���� ����ġ ������

	POINT_ENERGY = 128,					// 128 ���

	// ��� ui ��.
	// �������� ���� �ʱ⸸, Ŭ���̾�Ʈ���� ����� �� �ð��� POINT�� �����ϱ� ������ �̷��� �Ѵ�.
	// �� �β�����
	POINT_ENERGY_END_TIME = 129,					// 129 ��� ���� �ð�

	POINT_COSTUME_ATTR_BONUS = 130,
	POINT_MAGIC_ATT_BONUS_PER = 131,
	POINT_MELEE_MAGIC_ATT_BONUS_PER = 132,

	// �߰� �Ӽ� ����
	POINT_RESIST_ICE = 133,          //   �ñ� ����   : �������ݿ� ���� ����� ����
	POINT_RESIST_EARTH = 134,        //   ���� ����   : �������ݿ� ���� ����� ����
	POINT_RESIST_DARK = 135,         //   ��� ����   : �������ݿ� ���� ����� ����

	POINT_RESIST_CRITICAL = 136,		// ũ��Ƽ�� ����	: ����� ũ��Ƽ�� Ȯ���� ����
	POINT_RESIST_PENETRATE = 137,		// ����Ÿ�� ����	: ����� ����Ÿ�� Ȯ���� ����

#ifdef ENABLE_WOLFMAN_CHARACTER
	POINT_BLEEDING_REDUCE = 138,
	POINT_BLEEDING_PCT = 139,

	POINT_ATTBONUS_WOLFMAN = 140,				// 140 ���������� ����
	POINT_RESIST_WOLFMAN = 141,				// 141 ���������� ����
	POINT_RESIST_CLAW = 142,					// 142 CLAW�� ����
#endif

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


/* �����ϴ� ������ */
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

/* ������� �ʴ� ĳ���� ������ */
typedef struct character_point_instant
{
	int64_t			points[POINT_MAX_NUM];

	// ��... ��¥ ���� �� �ҷ��� �� �� ���� ����.
	// char�� �κ��� uint8_t array�� grid�� �����ϰ�, exchange�� cube�� CGrid�� grid�� �����ϰ� ���� �̰�...
	// grid�� ����� ������ grid�� ���� ���̾�!!!
	// ���� ��ȥ�� �κ��� �Ȱ��� ���� ���� ���� �߸��ߴ� �Ф�

	// ��ȥ�� �κ��丮.
#ifdef ENABLE_EXTRA_INVENTORY
#endif
#ifdef ENABLE_SWITCHBOT
#endif
	// by mhh
#ifdef __ATTR_TRANSFER_SYSTEM__
#endif
#ifdef ENABLE_ACCE_SYSTEM
#endif

} CHARACTER_POINT_INSTANT;

#define TRIGGERPARAM		entt::entity ch, entt::entity causer


class CTrigger
{
public:
	CTrigger() : bType(0), pFunc(nullptr)
	{
	}

	uint8_t	bType;
	int	(*pFunc) (TRIGGERPARAM);
};

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
class CExchange;
class CSkillProto;
class CParty;
class CDungeon;
class CWarMap;
class CAffect;
class CGuild;
class CSafebox;
class CMountInventory;
class CArena;


class CShop;
typedef class CShop* LPSHOP;

class CMob;
class CMobInstance;
typedef struct SMobSkillInfo TMobSkillInfo;

//SKILL_POWER_BY_LEVEL
extern int GetSkillPowerByLevelFromType(int job, int skillgroup, int skilllevel);
//END_SKILL_POWER_BY_LEVEL

namespace marriage
{
	class WeddingMap;
}


#ifdef LEADERBOARD_RAZOR93
struct LeaderboardEntry
{
	std::string name;
	int level;
	std::string victim;
	int dmg;
};

#endif

class CHARACTER : public CEntity, public CHorseRider
{
protected:
	time_t m_lastFruitUse;
	time_t m_lastGoldFruitUse;

public:
	time_t GetLastFruitUse() const { return m_lastFruitUse; }
	void SetLastFruitUse(time_t t) { m_lastFruitUse = t; }

	time_t GetLastGoldFruitUse() const { return m_lastGoldFruitUse; }
	void SetLastGoldFruitUse(time_t t) { m_lastGoldFruitUse = t; }

protected:
	//////////////////////////////////////////////////////////////////////////////////
	// Entity ����
	//////////////////////////////////////////////////////////////////////////////////

public:
	int m_lastBeltMountCount;

#ifdef ENABLE_VOTE4BUFF
public:
	long long	GetVoteCoin();
	void		SetVoteCoin(long long amount);
#endif

public:
	uint32_t			GetAIFlag() const;
	int32_t			GetInstantFlag() const;

	void				SetAggressive();
	bool				IsAggressive() const;

	void				SetCoward();
	bool				IsCoward() const;
	void				CowardEscape();

	void				SetNoAttackShinsu();
	bool				IsNoAttackShinsu() const;

	void				SetNoAttackChunjo();
	bool				IsNoAttackChunjo() const;

	void				SetNoAttackJinno();
	bool				IsNoAttackJinno() const;

	void				SetAttackMob();
	bool				IsAttackMob() const;

	virtual void			BeginStateEmpty();
	virtual void			EndStateEmpty() {}

	void				RestartAtSamePos();

protected:
	uint32_t				m_dwStateDuration;
	//////////////////////////////////////////////////////////////////////////////////

public:
	CHARACTER();
	~CHARACTER() override;

	void			Create(const char* c_pszName, uint32_t vid, bool isPC);
	void			Destroy();

	void			Disconnect(const char* c_pszReason);

protected:
	void			Initialize();

	//////////////////////////////////////////////////////////////////////////////////
	// Basic Points
#ifdef __SEND_TARGET_INFO__
private:
	uint32_t			dwLastTargetInfoPulse;
uint8_t m_lastAlignmentGrade;
int32_t m_alignBonusHP;
int32_t m_alignBonusMonster;
int32_t m_alignBonusHuman;
int32_t m_alignBonusMetin;
int32_t m_alignBonusBoss;
int32_t m_alignBonusPvm;
int32_t m_alignBonusNormal;
int32_t m_alignBonusSkill;
int32_t m_alignAppliedHP;
int32_t m_alignAppliedMonster;
int32_t m_alignAppliedHuman;
int32_t m_alignAppliedMetin;
int32_t m_alignAppliedBoss;
int32_t m_alignAppliedPvm;
int32_t m_alignAppliedNormal;
int32_t m_alignAppliedSkill;

#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
private:
	std::string m_strLastSentDisplayedNameWithBelt;
#endif
public:
	uint32_t			GetLastTargetInfoPulse() const { return dwLastTargetInfoPulse; }
	void			SetLastTargetInfoPulse(uint32_t pulse) { dwLastTargetInfoPulse = pulse; }
#endif
public:
	uint32_t			GetPlayerID() const { return m_dwPlayerID; }
public:
#ifdef ENABLE_FAKE_SHOP_HEADER
	int GetBeltCount() const;
#endif
#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
	int GetBeltCount() const;
	std::string GetDisplayedNameWithBeltCount() const;
	void CHARACTER::UpdateMountCountOverhead(LPCHARACTER ch, bool force)
#endif



	void			SetPlayerProto(const TPlayerTable* table);
	void			CreatePlayerProto(TPlayerTable& tab);	// ���� �� ���

	void			SetProto(const CMob* c_pkMob);
	uint16_t			GetRaceNum() const;

	void			Save();		// DelayedSave
	void			SaveReal();	// ���� ����
	void			FlushDelayedSaveItem();

#ifdef ENABLE_MULTI_NAMES
	const char* GetName(uint8_t lang = DEFAULT_LANGUAGE) const;
#else
	const char* GetName() const;
#endif

	uint32_t		GetLegacyVID() const;
	entt::entity		GetEntityHandle() const { return m_entity; }
	void			SetEntityHandle(entt::entity e) { m_entity = e; }
	uint32_t		GetPacketVID() const;
	// char.h (public)

	void			SetCharType(uint8_t bType) { m_bCharType = bType; }
	//void SetName(const char* name) { m_stName = (name ? name : ""); }
	void			SetName(const std::string& name) { m_stName = name; }

	void			SetRace(uint8_t race);
	bool			ChangeSex();

	uint32_t			GetAID() const;
	int				GetChangeEmpireCount() const;
	void			SetChangeEmpireCount();
	int				ChangeEmpire(uint8_t empire);

	uint8_t			GetJob() const;
	uint8_t			GetCharType() const;

	bool			IsPC() const { return GetDesc() ? true : false; }
	bool			IsNPC()	const { return m_bCharType != CHAR_TYPE_PC; }
	bool			IsMonster()	const { return m_bCharType == CHAR_TYPE_MONSTER; }
	bool			IsStone() const { return m_bCharType == CHAR_TYPE_STONE; }
	bool			IsDoor() const { return m_bCharType == CHAR_TYPE_DOOR; }
	bool			IsBuilding() const { return m_bCharType == CHAR_TYPE_BUILDING; }
	bool			IsWarp() const { return m_bCharType == CHAR_TYPE_WARP; }
	bool			IsGoto() const { return m_bCharType == CHAR_TYPE_GOTO; }
	//		bool			IsPet() const		{ return m_bCharType == CHAR_TYPE_PET; }
#ifdef ENABLE_EVENT_MANAGER
	// DUNGEON_TICKET_LOOT_EVENT extra metin marker
	void SetDungeonTicketExtraMetin(bool b);
	bool IsDungeonTicketExtraMetin() const;
#endif

	uint32_t			GetLastShoutPulse() const;
	void			SetLastShoutPulse(uint32_t pulse);
	int				GetLevel() const;
	void			SetLevel(uint8_t level);

	uint8_t			GetGMLevel() const;
	BOOL 			IsGM() const;
	void			SetGMLevel();

	uint32_t			GetExp() const;
	void			SetExp(uint32_t exp);
#ifdef __ENABLE_BLOCK_EXP__
	bool			Block_Exp;
#endif
	uint32_t			GetNextExp() const;
#ifdef __NEWPET_SYSTEM__
	uint32_t			PetGetNextExp() const;
#endif
	LPCHARACTER		DistributeExp();

	// ���� ���� ���� ����� �����Ѵ�.
	void			DistributeHP(entt::entity killer);
	void			DistributeSP(entt::entity killer, int iMethod = 0);

	void			SetPosition(int pos);
	bool			IsPosition(int pos) const;
	int				GetPosition() const;

	void			SetPart(uint8_t bPartPos, uint16_t wVal);
	uint16_t			GetPart(uint8_t bPartPos) const;
	uint16_t			GetOriginalPart(uint8_t bPartPos) const;

	void			SetHP(int64_t hp);
	int64_t				GetHP() const;
	void			SetSP(int64_t sp);
	int64_t				GetSP() const;
	void			SetStamina(int stamina);
	int				GetStamina() const;
	void			SetMaxHP(int64_t iVal);
	int64_t				GetMaxHP() const;

	void			SetMaxSP(int64_t iVal);
	int64_t				GetMaxSP() const;

	void			SetMaxStamina(int64_t iVal);
	int64_t				GetMaxStamina() const;

	void			SetRandomHP(int v);
	void			SetRandomSP(int v);

	int				GetRandomHP() const;
	int				GetRandomSP() const;

	int				GetHPPct() const;

	void			SetRealPoint(uint8_t idx, int64_t val);
	int64_t			GetRealPoint(uint8_t idx) const;

	void			SetPoint(uint8_t idx, int64_t val);

	int64_t			GetPoint(uint8_t idx) const;

	int				GetLimitPoint(uint8_t idx) const;
	int				GetPolymorphPoint(uint8_t idx) const;

	const TMobTable& GetMobTable() const;
	uint8_t				GetMobRank() const;
	uint8_t				GetMobBattleType() const;
	uint8_t				GetMobSize() const;
	uint32_t				GetMobDamageMin() const;
	uint32_t				GetMobDamageMax() const;
	uint16_t				GetMobAttackRange() const;
	uint32_t				GetMobDropItemVnum() const;
	float				GetMobDamageMultiply() const;

	// NEWAI
	bool			IsBerserker() const;
	bool			IsBerserk() const;
	void			SetBerserk(bool mode);

	bool			IsStoneSkinner() const;

	bool			IsGodSpeeder() const;
	bool			IsGodSpeed() const;
	void			SetGodSpeed(bool mode);

	bool			IsDeathBlower() const;
	bool			IsDeathBlow() const;

	bool			IsReviver() const;
	bool			HasReviverInParty() const;
	bool			IsRevive() const;
	void			SetRevive(bool mode);
	// NEWAI END

	bool			IsRaceFlag(uint32_t dwBit) const;
	bool			IsSummonMonster() const;
	uint32_t			GetSummonVnum() const;

	uint32_t			GetPolymorphItemVnum() const;
	uint32_t			GetMonsterDrainSPPoint() const;

	void			ComputeAligin();
	void			ComputePoints();
	void			ComputeBattlePoints();

	void			PointChange(uint8_t type, int64_t amount, bool bAmount = false, bool bBroadcast = false
#ifdef __ENABLE_BLOCK_EXP__
		, bool bForceExp = false
#endif
	);

	void			ApplyPoint(uint8_t bApplyType, int iVal);
#ifdef __NEWPET_SYSTEM__
	void			SendPetLevelUpEffect(int vid, int type, int value, int amount);
#endif		
	void			CheckMaximumPoints();	// HP, SP ���� ���� ���� �ִ밪 ���� ������ �˻��ϰ� ���ٸ� �����.

	bool			Show(int32_t lMapIndex, int32_t x, int32_t y, int32_t z = LONG_MAX, bool bShowSpawnMotion = false);

	void			Sitdown(int is_ground);
	void			Standup();

#ifdef ENABLE_ANCIENT_PYRAMID
	void			SetRotation(float fRot, bool bForce = false);
#else
	void			SetRotation(float fRot);
#endif
	void			SetRotationToXY(int32_t x, int32_t y);
	float			GetRotation() const;

	void			MotionPacketEncode(uint8_t motion, entt::entity victim, struct packet_motion* packet);
	void			Motion(uint8_t motion, entt::entity victim = entt::null);

	void			SendGreetMessage();

	void			ResetPoint(int iLv);

	void			SetBlockMode(uint8_t bFlag);
	void			SetBlockModeForce(uint8_t bFlag);
	uint8_t			GetBlockMode() const;
	bool			IsBlockMode(uint8_t bFlag) const;

	bool			IsPolymorphed() const;
	bool			IsPolyMaintainStat() const;
	void			SetPolymorph(uint32_t dwRaceNum, bool bMaintainStat = false);
	uint32_t			GetPolymorphVnum() const;
	int				GetPolymorphPower() const;

	// FISING
	void			fishing();
	void			fishing_take();
	// END_OF_FISHING

	// MINING
	void			mining(entt::entity load);
	void			mining_cancel();
	void			mining_take();
	// END_OF_MINING

	void			ResetPlayTime(uint32_t dwTimeRemain = 0);

	void			CreateFly(uint8_t bType, entt::entity victim);

	void			ResetChatCounter();
	uint8_t			IncreaseChatCounter();
	uint8_t			GetChatCounter() const;

	void			ResetMountCounter();
	uint8_t			IncreaseMountCounter();
	uint8_t			GetMountCounter() const;

protected:
	uint32_t			m_dwPolymorphRace;
	bool			m_bPolyMaintainStat;
	uint32_t			m_dwLoginPlayTime;
	uint32_t			m_dwPlayerID;
	std::string		m_stName;
#ifdef __NEWPET_SYSTEM__
	uint8_t			m_stImmortalSt;
#endif
	uint8_t			m_bCharType;
#ifdef ENABLE_EVENT_MANAGER
#endif

#ifdef __NEWPET_SYSTEM__
	uint32_t			m_newpetskillcd[4];
#endif
	CHARACTER_POINT		m_points;
	CHARACTER_POINT_INSTANT	m_pointsInstant;

	int				m_iMoveCount;
	uint32_t			m_dwPlayStartTime;
	uint8_t			m_bAddChrState;
	bool			m_bSkipSave;
	std::string		m_stMobile;
	char			m_szMobileAuth[5];
	uint8_t			m_bChatCounter;

	uint8_t			m_bMountCounter;

	// End of Basic Points

	//////////////////////////////////////////////////////////////////////////////////
	// Move & Synchronize Positions
	//////////////////////////////////////////////////////////////////////////////////
public:
	int32_t	SetInvincible(bool arg);
	bool	GetInvincible();
	int32_t	IncreaseMobHP(int32_t lArg);
	int32_t	IncreaseMobRigHP(int32_t lArg);
	 
	void SetFakePlayer(bool b) { m_bFakePlayer = b; }
	bool IsFakePlayer() const { return m_bFakePlayer; }

	 
	//void			SetCharType(uint8_t bType) { m_bCharType = bType; }
private:
	bool	isInvincible;
	bool	m_bFakePlayer = false;
public:
	// Phase 15E-final.LPENTITY.4-architect.B.1.3:
	// IsWalking is the composite that includes the stamina-exhaustion
	// fallback; IsNowWalking is the pure walk-mode flag (used by the
	// HEADER_GC_WALK_MODE packet emission). Both read the ECS
	// MovementState component as the authoritative source. Per A.2 §2
	// m_bNowWalking row.
	// Bodies in MovementSystem.cpp.
	bool			IsWalking() const;
	bool			IsNowWalking() const;
	void			SetWalking(bool bWalkFlag) { m_bWalking = bWalkFlag; }
	void			SetNowWalking(bool bWalkFlag);
	void			ResetWalking() { SetNowWalking(m_bWalking); }

	bool			Goto(int32_t x, int32_t y);	// �ٷ� �̵� ��Ű�� �ʰ� ��ǥ ��ġ�� BLENDING ��Ų��.
	void			Stop();

	bool			CanMove() const;		// �̵��� �� �ִ°�?

	bool			Sync(int32_t x, int32_t y);	// ���� �� �޼ҵ�� �̵� �Ѵ� (�� �� ���ǿ� ���� �̵� �Ұ��� ����)
	bool			Move(int32_t x, int32_t y);	// ������ �˻��ϰ� Sync �޼ҵ带 ���� �̵� �Ѵ�.
	void			OnMove(bool bIsAttack = false);	// �����϶� �Ҹ���. Move() �޼ҵ� �̿ܿ����� �Ҹ� �� �ִ�.
	uint32_t			GetMotionMode() const;
	float			GetMoveMotionSpeed() const;
	float			GetMoveSpeed() const;
	void			CalculateMoveDuration();
	void			SendMovePacket(uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime = 0, float iRot = -1.0f);
	// Phase 15E-final.LPENTITY.4-architect.B.1.2:
	// GetCurrentMoveDuration / GetCurrentMoveStartTime now read the ECS
	// MovementState component as the authoritative source. Per A.2 §2
	// m_dwMoveStartTime / m_dwMoveDuration rows.
	// Bodies in MovementSystem.cpp; legacy field still written by
	// CalculateMoveDuration (Phase C will redirect).
	uint32_t			GetCurrentMoveDuration() const;
	uint32_t			GetCurrentMoveStartTime() const;
	// Phase 15E-final.LPENTITY.4-architect.B.1.4:
	// GetCurrentDestX / GetCurrentDestY now read the ECS
	// MovementDestination component. Per A.2 movement destination row.
	// When the component is absent (entity is not actively moving),
	// returns current position via GetX/GetY. This preserves legacy
	// semantic where the destination is set to current position by Stop()
	// (and similar settle sites) when no move is active. Bodies in
	// MovementSystem.cpp.
	int32_t			GetCurrentDestX() const;
	int32_t			GetCurrentDestY() const;
	uint32_t			GetWalkStartTime() const { return m_dwWalkStartTime; }
	uint32_t			GetLastMoveTime() const { return m_dwLastMoveTime; }
	uint32_t			GetLastAttackTime() const { return m_dwLastAttackTime; }
// Phase C.4: GetAddChrStateForAudit removed. Its consumer in
// CheckMovementDrift state_flags subsection deleted with the
// m_bAddChrState write migration. CheckMovementDrift body is now empty;
// shim deletes in Phase G alongside the legacy field declarations.

	// Phase 15E-final.LPENTITY.4-architect.B.1.5:
	// GetAddChrStateFlag composes the 4-bit bStateFlag byte from the ECS
	// StatusFlags component (isDead, isSpawnState, isKillerMode,
	// isPartyState). Per A.2 §2 m_bAddChrState row.
	// Body in MovementSystem.cpp.
	uint8_t				GetAddChrStateFlag() const;

	void			SetLastAttacked(uint32_t time);	// ���������� ���ݹ��� �ð� �� ��ġ�� ������

	bool			SetSyncOwner(entt::entity character, bool bRemoveFromList = true);
	bool			IsSyncOwner(entt::entity character) const;

	bool			WarpSet(int32_t x, int32_t y, int32_t lRealMapIndex = 0);


#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
	bool			SwitchChannel(int32_t newAddr, uint16_t newPort);
	bool			StartChannelSwitch(int32_t newAddr, uint16_t newPort);
#endif

	void			SetWarpLocation(int32_t lMapIndex, int32_t x, int32_t y);
	void			WarpEnd();
	const PIXEL_POSITION& GetWarpPosition() const { return m_posWarp; }
	bool			WarpToPID(uint32_t dwPID);

	void			SaveExitLocation();
	void			ExitToSavedLocation();

	void			StartStaminaConsume();
	void			StopStaminaConsume();
	bool			IsStaminaConsume() const;
	bool			IsStaminaHalfConsume() const;

	void			ResetStopTime();
	uint32_t			GetStopTime() const;

protected:
	void			ClearSync();


	PIXEL_POSITION	m_posStart;
	PIXEL_POSITION	m_posWarp;
	int32_t			m_lWarpMapIndex;

	PIXEL_POSITION	m_posExit;
	int32_t			m_lExitMapIndex;

	uint32_t			m_dwMoveStartTime;
	uint32_t			m_dwMoveDuration;

	uint32_t			m_dwLastMoveTime;
	uint32_t			m_dwLastAttackTime;
	uint32_t			m_dwWalkStartTime;
	uint32_t			m_dwStopTime;

	bool			m_bWalking;
	bool			m_bNowWalking;
	bool			m_bStaminaConsume;
	// End

	// Quickslot ����
public:
	void			SyncQuickslot(uint8_t bType, uint8_t bOldPos, uint8_t bNewPos);
	bool			GetQuickslot(uint8_t pos, TQuickslot** ppSlot);
	bool			SetQuickslot(uint8_t pos, TQuickslot& rSlot);
	bool			DelQuickslot(uint8_t pos);
	bool			SwapQuickslot(uint8_t a, uint8_t b);
	void			ChainQuickslotItem(entt::entity item, uint8_t bType, uint8_t bOldPos);
#ifdef __ENABLE_NEW_OFFLINESHOP__
public:
	offlineshop::CShop* GetOfflineShop() { return m_pkOfflineShop; }
	void					SetOfflineShop(offlineshop::CShop* pkShop) { m_pkOfflineShop = pkShop; }

	offlineshop::CShop* GetOfflineShopGuest() const { return m_pkOfflineShopGuest; }
	void					SetOfflineShopGuest(offlineshop::CShop* pkShop);

	offlineshop::CShopSafebox*
		GetShopSafebox() { return m_pkShopSafebox; }
	void					SetShopSafebox(offlineshop::CShopSafebox* pk);

	void					SetAuction(offlineshop::CAuction* pk) { m_pkAuction = pk; }
	void					SetAuctionGuest(offlineshop::CAuction* pk);

	offlineshop::CAuction* GetAuction() { return m_pkAuction; }
	offlineshop::CAuction* GetAuctionGuest() const { return m_pkAuctionGuest; }


	//offlineshop-updated 05/08/19
	void					SetLookingOfflineshopOfferList(bool is) { m_bIsLookingOfflineshopOfferList = is; }
	bool					IsLookingOfflineshopOfferList() { return m_bIsLookingOfflineshopOfferList; }
	int						GetOfflineShopUseTime() const { return m_iOfflineShopUseTime; }
	void					SetOfflineShopUseTime();

private:
	offlineshop::CShop* m_pkOfflineShop;
	offlineshop::CShop* m_pkOfflineShopGuest;
	offlineshop::CShopSafebox* m_pkShopSafebox;
	offlineshop::CAuction* m_pkAuction;
	offlineshop::CAuction* m_pkAuctionGuest;

	//offlineshop-updated 05/08/19
	bool	m_bIsLookingOfflineshopOfferList;
	// patch with warp check
	int		m_iOfflineShopUseTime = 0;
#endif

protected:
	TQuickslot		m_quickslot[QUICKSLOT_MAX_NUM];

	////////////////////////////////////////////////////////////////////////////////////////
	// Affect
public:
	void			StartAffectEvent();
	void			ClearAffect(bool bSave = false);
	void			ComputeAffect(CAffect* pkAff, bool bAdd);
	bool			AddAffect(uint32_t dwType, uint8_t bApplyOn, int32_t lApplyValue, uint32_t dwFlag, int32_t lDuration, int32_t lSPCost, bool bOverride, bool IsCube = false);
	void			RefreshAffect();
	bool			RemoveAffect(uint32_t dwType);
	bool			IsAffectFlag(uint32_t dwAff) const;

	bool			UpdateAffect();	// called from EVENT
	int				ProcessAffect();

	void			LoadAffect(uint32_t dwCount, TPacketAffectElement* pElements);
	void			SaveAffect();

	// Affect loading�� ���� �����ΰ�?
	bool			IsLoadedAffect() const { return m_bIsLoadedAffect; }

	bool			IsGoodAffect(uint8_t bAffectType) const;

	void			RemoveGoodAffect();
	void			RemoveBadAffect();

	CAffect* FindAffect(uint32_t dwType, uint8_t bApply = APPLY_NONE) const;
	const std::list<CAffect*>& GetAffectContainer() const { return m_list_pkAffect; }
	const TAffectFlag& GetAffectFlags() const { return m_afAffectFlag; }
	bool			RemoveAffect(CAffect* pkAff);

protected:
	bool			m_bIsLoadedAffect;
	TAffectFlag		m_afAffectFlag;
	std::list<CAffect*>	m_list_pkAffect;

#ifdef ENABLE_SKILLS_BUFF_ALTERNATIVE
public:
	void						ClearAffectSkills();
	void						SaveAffectSkills(uint32_t dwType, uint8_t bApplyOn, int32_t lApplyValue, uint32_t dwFlag, int32_t lDuration, int32_t lSPCost);
	void						LoadAffectSkills();

protected:
	std::vector<TAffectSkills>	m_list_pkAffectSkills;
#endif

public:
	// PARTY_JOIN_BUG_FIX
	void			SetParty(LPPARTY pkParty);
	LPPARTY			GetParty() const { return m_pkParty; }

	bool			RequestToParty(entt::entity leader);
	void			DenyToParty(entt::entity member);
	void			AcceptToParty(entt::entity member);

	/// �ڽ��� ��Ƽ�� �ٸ� character �� �ʴ��Ѵ�.
	/**
	 * @param	pchInvitee �ʴ��� ��� character. ��Ƽ�� ���� ������ �����̾�� �Ѵ�.
	 *
	 * ���� character �� ���°� ��Ƽ�� �ʴ��ϰ� �ʴ���� �� �ִ� ���°� �ƴ϶�� �ʴ��ϴ� ĳ���Ϳ��� �ش��ϴ� ä�� �޼����� �����Ѵ�.
	 */
	void			PartyInvite(entt::entity invitee);

	/// �ʴ��ߴ� character �� ������ ó���Ѵ�.
	/**
	 * @param	pchInvitee ��Ƽ�� ������ character. ��Ƽ�� ���������� �����̾�� �Ѵ�.
	 *
	 * pchInvitee �� ��Ƽ�� ������ �� �ִ� ��Ȳ�� �ƴ϶�� �ش��ϴ� ä�� �޼����� �����Ѵ�.
	 */
	void			PartyInviteAccept(entt::entity invitee);

	/// �ʴ��ߴ� character �� �ʴ� �źθ� ó���Ѵ�.
	/**
	 * @param [in]	dwPID �ʴ� �ߴ� character �� PID
	 */
	void			PartyInviteDeny(uint32_t dwPID);

	int				GetLeadershipSkillLevel() const;

	bool			CanSummon(int iLeaderShip);

	void			SetPartyRequestEvent(LPEVENT pkEvent) { m_pkPartyRequestEvent = pkEvent; }

protected:

	/// ��Ƽ�� �����Ѵ�.
	/**
	 * @param	pkLeader ������ ��Ƽ�� ����
	 */
	void			PartyJoin(entt::entity leader);

	/**
	 * ��Ƽ ������ �� �� ���� ����� �����ڵ�.
	 * Error code �� �ð��� �������ΰ��� ���� ���氡����(mutable) type �� ����(static) type ���� ������.
	 * Error code �� ���� PERR_SEPARATOR ���� ������ ���氡���� type �̰� ������ ���� type �̴�.
	 */
	enum PartyJoinErrCode {
		PERR_NONE = 0,	///< ó������
		PERR_SERVER,			///< ���������� ��Ƽ���� ó�� �Ұ�
		PERR_DUNGEON,			///< ĳ���Ͱ� ������ ����
		PERR_OBSERVER,			///< ���������
		PERR_LVBOUNDARY,		///< ��� ĳ���Ϳ� �������̰� ��
		PERR_LOWLEVEL,			///< �����Ƽ�� �ְ��������� 30���� ����
		PERR_HILEVEL,			///< �����Ƽ�� ������������ 30���� ����
		PERR_ALREADYJOIN,		///< ��Ƽ���� ��� ĳ���Ͱ� �̹� ��Ƽ��
		PERR_PARTYISFULL,		///< ��Ƽ�ο� ���� �ʰ�
		PERR_SEPARATOR,			///< Error type separator.
		PERR_DIFFEMPIRE,		///< ��� ĳ���Ϳ� �ٸ� ������
		PERR_MAX				///< Error code �ְ�ġ. �� �տ� Error code �� �߰��Ѵ�.
	};

	/// ��Ƽ �����̳� �Ἲ ������ ������ �˻��Ѵ�.
	/**
	 * @param 	pchLeader ��Ƽ�� leader �̰ų� �ʴ��� character
	 * @param	pchGuest �ʴ�޴� character
	 * @return	��� PartyJoinErrCode �� ��ȯ�� �� �ִ�.
	 */
	static PartyJoinErrCode	IsPartyJoinableCondition(entt::entity leader, entt::entity guest);

	/// ��Ƽ �����̳� �Ἲ ������ ������ ������ �˻��Ѵ�.
	/**
	 * @param 	pchLeader ��Ƽ�� leader �̰ų� �ʴ��� character
	 * @param	pchGuest �ʴ�޴� character
	 * @return	mutable type �� code �� ��ȯ�Ѵ�.
	 */
	static PartyJoinErrCode	IsPartyJoinableMutableCondition(entt::entity leader, entt::entity guest);

	LPPARTY			m_pkParty;
	uint32_t			m_dwLastDeadTime;
	LPEVENT			m_pkPartyRequestEvent;

	/**
	 * ��Ƽ��û Event map.
	 * key: �ʴ���� ĳ������ PID
	 * value: event�� pointer
	 *
	 * �ʴ��� ĳ���͵鿡 ���� event map.
	 */
	typedef std::map< uint32_t, LPEVENT >	EventMap;
	EventMap		m_PartyInviteEventMap;

	// END_OF_PARTY_JOIN_BUG_FIX

	////////////////////////////////////////////////////////////////////////////////////////
	// Dungeon
public:
	void			SetDungeon(LPDUNGEON pkDungeon);
	LPDUNGEON		GetDungeon() const { return m_pkDungeon; }
	LPDUNGEON		GetDungeonForce() const;
protected:
	LPDUNGEON	m_pkDungeon;
	int			m_iEventAttr;

	////////////////////////////////////////////////////////////////////////////////////////
	// Guild
public:
	void			SetGuild(CGuild* pGuild);
	CGuild* GetGuild() const { return m_pGuild; }

	void			SetWarMap(CWarMap* pWarMap);
	CWarMap* GetWarMap() const { return m_pWarMap; }

protected:
	CGuild* m_pGuild;
	uint32_t			m_dwUnderGuildWarInfoMessageTime;
	CWarMap* m_pWarMap;

	////////////////////////////////////////////////////////////////////////////////////////
	// Item related
public:
	bool			CanHandleItem(bool bSkipRefineCheck = false, bool bSkipObserver = false); // ������ ���� ������ �� �� �ִ°�?

	bool			IsItemLoaded() const { return m_bItemLoaded; }
	void			SetItemLoaded() { m_bItemLoaded = true; }

	void			ClearItem();

#ifdef ENABLE_SORT_INVEN	
	void			EditMyInven();
	void			EditMyExtraInven();
#endif

#ifdef __HIGHLIGHT_SYSTEM__
	void			SetItem(TItemPos Cell, entt::entity item, bool isHighLight = false);
#else
	void			SetItem(TItemPos Cell, entt::entity item);
#endif
	LPITEM			GetItem(TItemPos Cell) const;
	LPITEM			GetInventoryItem(uint16_t wCell) const;
	LPITEM			GetDragonSoulItem(uint16_t wCell) const;
	uint16_t			GetDragonSoulGrid(uint16_t wCell) const;
#ifdef ENABLE_SWITCHBOT
	LPITEM			GetSwitchbotItem(uint16_t wCell) const;
#endif
#ifdef ENABLE_EXTRA_INVENTORY
	LPITEM			GetExtraInventoryItem(uint16_t wCell) const;
	uint16_t			GetExtraInventoryGrid(uint16_t wCell) const;
	void			SetNextSortExtraInventoryPulse(int pulse) { m_sortExtraInventoryPulse = pulse; }
	int				GetSortExtraInventoryPulse() { return m_sortExtraInventoryPulse; }
	int				m_sortExtraInventoryPulse;
#endif
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	int		ExtraInventoryMaxSlots(int iArg1, bool bAuto = false) const;
	void	UnlockExtraInventory(uint8_t category);
#endif
	bool			IsEmptyItemGrid(TItemPos Cell, uint8_t size, int iExceptionCell = -1) const;

	void			SetWear(uint8_t bCell, entt::entity item);
	LPITEM			GetWear(uint8_t bCell) const;

	// MYSHOP_PRICE_LIST
	void			UseSilkBotary(void); 		/// ��� ������ �������� ���

	/// DB ĳ�÷� ���� �޾ƿ� �������� ����Ʈ�� �������� �����ϰ� ������ ������ ����� ó���Ѵ�.
	/**
	 * @param [in] p	�������� ����Ʈ ��Ŷ
	 *
	 * ������ �� ó�� ��� ������ ������ ��� �� UseSilkBotary ���� DB ĳ�÷� �������� ����Ʈ�� ��û�ϰ�
	 * ������� ������ �� �Լ����� ���� ��ܺ����� ����� ó���Ѵ�.
	 */
	void			UseSilkBotaryReal(const TPacketMyshopPricelistHeader* p);
	// END_OF_MYSHOP_PRICE_LIST

	bool			UseItemEx(LPITEM item, TItemPos DestCell);
	bool			UseItem(TItemPos Cell, TItemPos DestCell = NPOS);

	// ADD_REFINE_BUILDING
	bool			IsRefineThroughGuild() const;
	CGuild* GetRefineGuild() const;
	int64_t				ComputeRefineFee(int64_t iCost, int64_t iMultiply = 5) const;
	void			PayRefineFee(int64_t iTotalMoney);
	void			SetRefineNPC(entt::entity character);
	// END_OF_ADD_REFINE_BUILDING

	bool			RefineItem(LPITEM pkItem, LPITEM pkTarget);
	bool			DropItem(TItemPos Cell,
#ifdef ENABLE_NEW_STACK_LIMIT
		int
#else
		uint8_t
#endif
		bCount = 0);
	bool			DestroyItem(TItemPos Cell);
	void			ItemDivision(TItemPos Cell);
	bool			GiveRecallItem(LPITEM item);
	void			ProcessRecallItem(LPITEM item);

	//	void			PotionPacket(int iPotionType);

	void			EffectPacket(uint8_t enumEffectType);
	void			SpecificEffectPacket(const char filename[128]);
	// ADD_MONSTER_REFINE
	bool			DoRefine(LPITEM item, bool bMoneyOnly = false);
	// END_OF_ADD_MONSTER_REFINE

	bool			DoRefineWithScroll(LPITEM item);
	bool			RefineInformation(uint8_t bCell, uint8_t bType, int iAdditionalCell = -1);

	void			SetRefineMode(int iAdditionalCell = -1);
	void			ClearRefineMode();

	bool			GiveItem(entt::entity victim, TItemPos Cell);
	bool			CanReceiveItem(entt::entity from, LPITEM item) const;
	void			ReceiveItem(entt::entity from, LPITEM item);
	bool			GiveItemFromSpecialItemGroup(uint32_t dwGroupNum, std::vector <uint32_t>& dwItemVnums,
		std::vector <uint32_t>& dwItemCounts, std::vector<entt::entity>& item_gets, int& count);

	bool			MoveItem(TItemPos pos, TItemPos change_pos,
#ifdef ENABLE_NEW_STACK_LIMIT
		int
#else
		uint32_t
#endif
		count);
	bool			PickupItem(uint32_t vid);
	bool			EquipItem(LPITEM item, int iCandidateCell = -1);
	bool			UnequipItem(LPITEM item);
	bool			UnequipItem(entt::entity item);

	// ���� item�� ������ �� �ִ� �� Ȯ���ϰ�, �Ұ��� �ϴٸ� ĳ���Ϳ��� ������ �˷��ִ� �Լ�
	bool			CanEquipNow(const LPITEM item, const TItemPos& srcCell = NPOS, const TItemPos& destCell = NPOS);

	// �������� item�� ���� �� �ִ� �� Ȯ���ϰ�, �Ұ��� �ϴٸ� ĳ���Ϳ��� ������ �˷��ִ� �Լ�
	bool			CanUnequipNow(const LPITEM item, const TItemPos& srcCell = NPOS, const TItemPos& destCell = NPOS);

	bool			SwapItem(uint8_t bCell, uint8_t bDestCell);

	LPITEM			AutoGiveItem(uint32_t dwItemVnum,
#ifdef ENABLE_NEW_STACK_LIMIT
		int
#else
		uint8_t
#endif
		bCount = 1, int iRarePct = -1, bool bMsg = true
#ifdef __HIGHLIGHT_SYSTEM__
		, bool isHighLight = true
#endif
	);
	void			AutoGiveItem(LPITEM item, bool longOwnerShip = false
#ifdef __HIGHLIGHT_SYSTEM__
		, bool isHighLight = true
#endif
	);
#ifdef ENABLE_DS_REFINE_ALL
	bool	AutoGiveDS(LPITEM item, bool longOwnerShip = false);
#endif
	bool			CanTakeInventoryItem(entt::entity item, TItemPos* pos);

#ifdef ENABLE_EXTRA_INVENTORY
	int				GetEmptyExtraInventory(LPITEM pItem) const;
	int				GetEmptyExtraInventory(uint8_t size, uint8_t category) const; // needed for offline shop
#endif

	int				GetEmptyInventory(uint8_t size) const;
	int				GetEmptyDragonSoulInventory(LPITEM pItem) const;
	void			CopyDragonSoulItemGrid(std::vector<uint16_t>& vDragonSoulItemGrid) const;

	int				CountEmptyInventory() const;

	int 			CountSpecifyItemRenewal(uint32_t vnum) const;

	int				CountSpecifyItem(uint32_t vnum) const;
	void			RemoveSpecifyItem(uint32_t vnum, int count = 1, bool cuberenewal = false);
	LPITEM			FindSpecifyItem(uint32_t vnum
#ifdef ENABLE_EXTRA_INVENTORY
		, bool reinforce = false
#endif
	) const;
	LPITEM			FindItemByID(uint32_t id) const;

	int				CountSpecifyTypeItem(uint8_t type) const;
	void			RemoveSpecifyTypeItem(uint8_t type, int count = 1);

	bool			IsEquipUniqueItem(uint32_t dwItemVnum) const;

	// CHECK_UNIQUE_GROUP
	bool			IsEquipUniqueGroup(uint32_t dwGroupVnum) const;
	// END_OF_CHECK_UNIQUE_GROUP

	// End of Item

protected:

	void			SendMyShopPriceListCmd(uint32_t dwItemVnum, int64_t dwItemPrice);

	bool			m_bNoOpenedShop;	///< �̹� ���� �� ���λ����� �� ���� �ִ����� ����(������ ���� ���ٸ� true)

	bool			m_bItemLoaded;
	int				m_iRefineAdditionalCell;
	bool			m_bUnderRefine;
	uint32_t			m_dwRefineNPCVID;

public:
	////////////////////////////////////////////////////////////////////////////////////////
	// Money related

	int64_t				GetGold() const;
	void			SetGold(int64_t gold);
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	int				Inven_Point() const { return m_points.envanter; }
	int				Inventory_Size() const { return 90 + (5 * Inven_Point()); }
	void			Set_Inventory_Point(int black) { m_points.envanter = black; }
	bool			Update_Inven();
#endif
	bool			DropGold(int64_t gold);

	int64_t				GetAllowedGold() const;
	void			GiveGold(int64_t iAmount);	// ��Ƽ�� ������ ��Ƽ �й�, �α� ���� ó��

#ifdef ENABLE_PVP_ADVANCED
	int				GetDuel(const char* type) const;
	void			SetDuel(const char* type, int value);
#endif
#ifdef ENABLE_GAYA_SYSTEM
	int				GetGaya() const { return m_points.gaya; }
	void			SetGaya(int gaya) { m_points.gaya = gaya; }
#endif

	// End of Money

	////////////////////////////////////////////////////////////////////////////////////////
	// Shop related
public:
	void			SetShop(LPSHOP pkShop);
	LPSHOP			GetShop() const { return m_pkShop; }
	void			ShopPacket(uint8_t bSubHeader);

	void			SetShopOwner(entt::entity character);
	LPCHARACTER		GetShopOwner() const { return m_pkChrShopOwner; }

	void			OpenMyShop(const char* c_pszSign, TShopItemTable* pTable, uint8_t bItemCount
#ifdef KASMIR_PAKET_SYSTEM
		, uint32_t KasmirNpc, uint8_t KasmirBaslik
#endif
	);
#ifdef KASMIR_PAKET_SYSTEM
	void			UseSilkBotaryKasmir(void);
#endif
	LPSHOP			GetMyShop() const { return m_pkMyShop; }
	void			CloseMyShop();
#ifdef ENABLE_PVP_ADVANCED
	void			DestroyPvP();
#endif

protected:

	LPSHOP			m_pkShop;
	LPSHOP			m_pkMyShop;
#ifdef KASMIR_PAKET_SYSTEM
	uint8_t			m_bKasmirPaketBaslik;
	bool			m_bKasmirPaketDurum;
#endif
	std::string		m_stShopSign;
	LPCHARACTER		m_pkChrShopOwner;
	// End of shop

#ifdef __SKILL_COLOR_SYSTEM__
public:
	void			SetSkillColor(uint32_t* dwSkillColor);
	uint32_t* GetSkillColor() { return m_dwSkillColor[0]; }

protected:
	uint32_t			m_dwSkillColor[ESkillColorLength::MAX_SKILL_COUNT + ESkillColorLength::MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
#endif

	////////////////////////////////////////////////////////////////////////////////////////
	// Exchange related
public:
	bool			ExchangeStart(entt::entity victim);
	void			SetExchange(CExchange* pkExchange);
	CExchange* GetExchange() const { return m_pkExchange; }
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
	public:
		void SetWheelDestiny(std::shared_ptr<CWheelDestiny> pt);
		std::shared_ptr<CWheelDestiny> GetWheelDestiny() const { return pWheelDestiny; }
		void SetWheelFreeCount(const int count) { SetQuestFlag("wheel.free", GetWheelFreeCount() + count); }
		int GetWheelFreeCount() const { return GetQuestFlag("wheel.free"); }

	private:
		std::shared_ptr<CWheelDestiny> pWheelDestiny = nullptr;
#endif
protected:
	CExchange* m_pkExchange;
	// End of Exchange

#ifdef __DUNGEON_INFO_SYSTEM__
public:
	void				SetQuestDamage(int race, int dmg);
	uint64_t					GetQuestDamage(int race);

private:
	std::map<int, int>	dungeonDamage;
#endif

	////////////////////////////////////////////////////////////////////////////////////////
	// Battle
public:
	struct TBattleInfo
	{
		uint64_t iTotalDamage;
		int iAggro;

		TBattleInfo(int iTot, int iAggr)
			: iTotalDamage(iTot), iAggro(iAggr)
		{
		}
	};
	typedef std::map<entt::entity, TBattleInfo> TDamageMap;

	typedef struct SAttackLog
	{
		uint32_t	dwVID;
		uint32_t	dwTime;
	} AttackLog;


#ifdef __ENABLE_BERAN_ADDONS__
	bool				IsBeranMap(int lMapIndex);
#endif


	bool				Damage(entt::entity attacker, int64_t dam, EDamageType type = DAMAGE_TYPE_NORMAL);
	void				DeathPenalty(uint8_t bExpLossPercent);
	void				ReviveInvisible(int iDur);

	bool				Attack(entt::entity victim, uint8_t bType = 0);
	bool				IsAlive() const;
	bool				CanFight() const;

	bool				CanBeginFight() const;
	void				BeginFight(entt::entity victim); // pkVictimr�� �ο�� �����Ѵ�. (��������, ������ �� �ֳ� üũ�Ϸ��� CanBeginFight�� ���)


	bool				IsStun() const;
	void				Stun();
	bool				IsDead() const;
	void				Dead(entt::entity killer = entt::null, bool bImmediateDead = false);
#ifdef __NEWPET_SYSTEM__
	//int GetBeltCount() const;//#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
	void				SetImmortal(int st) { m_stImmortalSt = st; };
	bool				IsImmortal() { return 1 == m_stImmortalSt; };
	void				SetNewPetSkillCD(int s, uint32_t time) { m_newpetskillcd[s] = time; };
	uint32_t				GetNewPetSkillCD(int s) { return m_newpetskillcd[s]; };
#endif
	void				Reward(bool bItemDrop);
	void				RewardGold(entt::entity attacker);

	bool				Shoot(uint8_t bType);
	void				FlyTarget(uint32_t dwTargetVID, int32_t x, int32_t y, uint8_t bHeader);

	void				ForgetMyAttacker();
	void				AggregateMonster();
#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
	void AggregateMonsterPlus();
#endif
	void				AttractRanger();
	void				PullMonster();

	int					GetArrowAndBow(entt::entity* ppkBow, entt::entity* ppkArrow, int iArrowCount = 1);
#ifdef ENABLE_RANKING

	//void SendLeaderboardData();
	//void SendLeaderboardNews();
	//static void LeaderboardLoop();
#endif
	void				UseArrow(entt::entity pkArrow, uint32_t dwArrowCount);
#ifdef LEADERBOARD_RAZOR93

	void SendLeaderboardData();
	void SendLeaderboardDataSkillMob(entt::entity viewer);
	void SendLeaderboardDataGuild();
	static std::vector<LeaderboardEntry> FetchTop10SkillMob();
	static void CheckLeaderboardSkillMobChanges();
#endif
	void				AttackedByPoison(entt::entity attacker);
	void				RemovePoison();
#ifdef ENABLE_WOLFMAN_CHARACTER
	void				AttackedByBleeding(entt::entity attacker);
	void				RemoveBleeding();
#endif
	void				AttackedByFire(entt::entity attacker, int amount, int count);
	void				RemoveFire();

	uint8_t GetAlignmentGrade() const;

	void ClearAlignmentBonus();

	void ApplyAlignmentBonus();

	void				UpdateAlignment(uint32_t iAmount);
	uint32_t					GetAlignment() const;
	//int GetBeltCount() const;
#ifdef ENABLE_FAKE_SHOP_HEADER
	int GetMountCount() const;
	void UpdateMountInventoryCountOverhead(entt::entity viewer);
	void UpdateMountCountOverheadToViewers();
	//void UpdateMountCountOverhead(LPCHARACTER ch);
#ifdef DISABLE_CORE_PULSE_RAZOR93

	bool IsNextMountPulse() const;

	void UpdateMountPulse();
#endif
#endif
	//����ġ ���
	uint32_t					GetRealAlignment() const;
	//void				ShowAlignment(bool bShow);

	void				SetKillerMode(bool bOn);
	bool				IsKillerMode() const;
	void				UpdateKillerMode();

	uint8_t				GetPKMode() const;
	void				SetPKMode(uint8_t bPKMode);

	void				ItemDropPenalty(entt::entity killer);

	void				UpdateAggrPoint(entt::entity character, EDamageType type, int dam);

	//
	// HACK
	//
public:
	void SetComboSequence(uint8_t seq);
	uint8_t GetComboSequence() const;

	void SetLastComboTime(uint32_t time);
	uint32_t GetLastComboTime() const;

	int GetValidComboInterval() const;
	void SetValidComboInterval(int interval);

	uint8_t GetComboIndex() const;

	void IncreaseComboHackCount(int k = 1);
	void ResetComboHackCount();
	void SkipComboAttackByTime(int interval);
	uint32_t GetSkipComboAttackByTime() const;

protected:
	uint8_t m_bComboSequence;
	uint32_t m_dwLastComboTime;
	int m_iValidComboInterval;
	uint8_t m_bComboIndex;
	int m_iComboHackCount;
	uint32_t m_dwSkipComboAttackByTime;

protected:
	void				UpdateAggrPointEx(entt::entity character, EDamageType type, int dam, TBattleInfo& info);
	void				ChangeVictimByAggro(int iNewAggro, entt::entity newVictim);

	uint32_t				m_dwFlyTargetID;
	std::vector<uint32_t>	m_vec_dwFlyTargets;
	TDamageMap			m_map_kDamage;	// � ĳ���Ͱ� ������ �󸶸�ŭ�� �������� �־��°�?
	//		AttackLog			m_kAttackLog;
	uint32_t				m_dwKillerPID;

	uint32_t					m_iAlignment;
	uint32_t					m_iRealAlignment;
	int					m_iKillerModePulse;

	// Aggro
	uint32_t				m_dwLastVictimSetTime;
	int					m_iMaxAggro;
	// End of Battle

	// Stone
public:
	void				SetStone(entt::entity stone);
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
	void ClearStone(entt::entity killer = entt::null);
	void RegisterDamageForExp(entt::entity attacker, int iDamage = 1);
#else
	void				ClearStone();
#endif
#ifdef ENABLE_ITEMSHOP
	uint32_t			GetDragonCoin();
	void				SetDragonCoin(uint32_t amount);
	void SetProtectTime(const std::string& flagname, int value);
	int GetProtectTime(const std::string& flagname) const;
#endif


	void				DetermineDropMetinStone();
	uint32_t				GetDropMetinStoneVnum() const { return m_dwDropMetinStone; }
	uint8_t				GetDropMetinStonePct() const { return m_bDropMetinStonePct; }
	void				DetermineDropMetinStofa();
	uint32_t				GetDropMetinStofaVnum() const { return m_dwDropMetinStofa; }
	uint8_t				GetDropMetinStofaPct() const { return m_bDropMetinStofaPct; }
	void				DetermineDropMetinSacca();
	uint32_t				GetDropMetinSaccaVnum() const { return m_dwDropMetinSacca; }
	uint8_t				GetDropMetinSaccaPct() const { return m_bDropMetinSaccaPct; }

protected:
	LPCHARACTER			m_pkChrStone;		// ���� ������ ��
	CHARACTER_SET		m_set_pkChrSpawnedBy;	// ���� ������ ���
	uint32_t				m_dwDropMetinStone;
	uint8_t				m_bDropMetinStonePct;
	uint32_t				m_dwDropMetinStofa;
	uint8_t				m_bDropMetinStofaPct;
	uint32_t				m_dwDropMetinSacca;
	uint8_t				m_bDropMetinSaccaPct;
	std::map<std::string, int>  m_protection_Time;

#ifdef ENABLE_RANKING
protected:
	long long	m_lRankPoints[RANKING_MAX_CATEGORIES];
#ifdef LEADERBOARD_RAZOR93
	bool m_bSkillHit;
#endif
public:
#ifdef LEADERBOARD_RAZOR93
	void SetSkillHit(bool b) { m_bSkillHit = b; }
	bool IsSkillHit() const { return m_bSkillHit; }
#endif
	long long	GetRankPoints(int iArg);
	void		SetRankPoints(int iArg, long long lPoint);
	void		RankingSubcategory(int iArg);
#endif
#ifdef ENABLE_ATTR_COSTUMES
public:
	void	SetAttrDialogRemove(int iArg) { attrdialog_remove = iArg; }
	int		GetAttrDialogRemove() const { return attrdialog_remove; }

protected:
	int	attrdialog_remove;
#endif
#ifdef ENABLE_NEW_PET_EDITS
public:
	void	SetPetEnchant(int iArg) { petenchant = iArg; }
	int		GetPetEnchant() const { return petenchant; }

protected:
	int	petenchant;
#endif
public:
	enum
	{
		SKILL_UP_BY_POINT,
		SKILL_UP_BY_BOOK,
		SKILL_UP_BY_TRAIN,

		// ADD_GRANDMASTER_SKILL
		SKILL_UP_BY_QUEST,
		// END_OF_ADD_GRANDMASTER_SKILL
	};

#ifdef ENABLE_NEW_PASSIVE_SKILLS
	bool				SkillCanUp(uint32_t dwVnum, bool book = false);
#endif

	void				SkillLevelPacket();
	void				SkillLevelUp(uint32_t dwVnum, uint8_t bMethod = SKILL_UP_BY_POINT);
	bool				SkillLevelDown(uint32_t dwVnum);
	// ADD_GRANDMASTER_SKILL
	bool				UseSkill(uint32_t dwVnum, entt::entity victim, bool bUseGrandMaster = true);
	void				ResetSkill();
	void				SetSkillLevel(uint32_t dwVnum, uint8_t bLev);
	int					GetUsedSkillMasterType(uint32_t dwVnum);

	bool				IsLearnableSkill(uint32_t dwSkillVnum) const;
	// END_OF_ADD_GRANDMASTER_SKILL

	bool				CheckSkillHitCount(const uint8_t SkillID, entt::entity target);
	bool				CanUseSkill(uint32_t dwSkillVnum) const;
	bool				IsUsableSkillMotion(uint32_t dwMotionIndex) const;
	int					GetSkillLevel(uint32_t dwVnum) const;
	int					GetSkillMasterType(uint32_t dwVnum) const;
	int					GetSkillPower(uint32_t dwVnum, uint8_t bLevel = 0) const;

	time_t				GetSkillNextReadTime(uint32_t dwVnum) const;
	void				SetSkillNextReadTime(uint32_t dwVnum, time_t time);
	void				SkillLearnWaitMoreTimeMessage(uint32_t dwVnum);

	void				ComputePassiveSkill(uint32_t dwVnum);
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
	int					ComputeGyeongGongSkill(uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel = 0);
#endif
	int					ComputeSkill(uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel = 0);
#ifdef GROUP_BUFF
	int					ComputeSkillParty(uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel = 0);
#endif
	int					ComputeSkillAtPosition(uint32_t dwVnum, const PIXEL_POSITION& posTarget, uint8_t bSkillLevel = 0);
	void				ComputeSkillPoints();

	void				SetSkillGroup(uint8_t bSkillGroup);
	uint8_t				GetSkillGroup() const;

	int					ComputeCooltime(int time);

	void				GiveRandomSkillBook();

	void				DisableCooltime();
	bool				LearnSkillByBook(uint32_t dwSkillVnum, uint8_t bProb = 0);
	bool				LearnGrandMasterSkill(uint32_t dwSkillVnum);

private:
	bool				m_bDisableCooltime;
	uint32_t				m_dwLastSkillTime;	///< ���������� skill �� �� �ð�(millisecond).
	// End of Skill
#ifdef DISABLE_CORE_PULSE_RAZOR93

#endif
	// MOB_SKILL
public:
	bool				HasMobSkill() const;
	size_t				CountMobSkill() const;
	const TMobSkillInfo* GetMobSkill(unsigned int idx) const;
	bool				CanUseMobSkill(unsigned int idx) const;
	bool				UseMobSkill(unsigned int idx);
	void				ResetMobSkillCooltime();
protected:
	uint32_t				m_adwMobSkillCooltime[MOB_SKILL_MAX_NUM];
	// END_OF_MOB_SKILL

	// for SKILL_MUYEONG
public:
	void				StartMuyeongEvent();
	void				StopMuyeongEvent();
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
	void				StartGyeongGongEvent();
	void				StopGyeongGongEvent();
#endif

private:
	LPEVENT				m_pkMuyeongEvent;
#ifdef ENABLE_NEW_GYEONGGONG_SKILL
	LPEVENT				m_pkGyeongGongEvent;
#endif

	// for SKILL_CHAIN lighting
public:
	// Not CHARACTER_SET: that typedef is shared with char_manager, dungeon, war_map
	// and wedding. Not ENTITY_SET either - typedef.h uses that name for the
	// sectree entity, std::unordered_set<LPENTITY>.
	typedef std::unordered_set<entt::entity> TChainLightningExceptContainer;

	int					GetChainLightningIndex() const { return m_iChainLightingIndex; }
	void				IncChainLightningIndex() { ++m_iChainLightingIndex; }
	void				AddChainLightningExcept(entt::entity ch) { m_setExceptChainLighting.insert(ch); }
	void				ResetChainLightningIndex() { m_iChainLightingIndex = 0; m_setExceptChainLighting.clear(); }
	int					GetChainLightningMaxCount() const;
	const TChainLightningExceptContainer& GetChainLightingExcept() const { return m_setExceptChainLighting; }

private:
	int					m_iChainLightingIndex;
	TChainLightningExceptContainer m_setExceptChainLighting;

	// for SKILL_EUNHYUNG
public:
	void				SetAffectedEunhyung();
	void				ClearAffectedEunhyung() { m_dwAffectedEunhyungLevel = 0; }
	bool				GetAffectedEunhyung() const { return m_dwAffectedEunhyungLevel; }

private:
	uint32_t				m_dwAffectedEunhyungLevel;

	//
	// Skill levels
	//
protected:
	TPlayerSkill* m_pSkillLevels;
	std::unordered_map<uint8_t, int>		m_SkillDamageBonus;
	std::map<int, TSkillUseInfo>	m_SkillUseInfo;

	////////////////////////////////////////////////////////////////////////////////////////
	// AI related
public:
	void			AssignTriggers(const TMobTable* table);
	LPCHARACTER		GetVictim() const;	// ������ ��� ����
	void			SetVictim(entt::entity victim);
	LPCHARACTER		GetNearestVictim(entt::entity chr);
	LPCHARACTER		GetProtege() const;	// ��ȣ�ؾ� �� ��� ����
	virtual void			StateBattle();
	virtual void			StateIdle();

protected:
	void				__StateIdle_Monster();
	void				__StateIdle_NPC();

public:
	bool			Follow(entt::entity chr, float fMinimumDistance = 150.0f);
	bool			Return();
	bool			IsGuardNPC() const;
	bool			IsChangeAttackPosition(entt::entity target) const;
	void			ResetChangeAttackPositionTime() { m_dwLastChangeAttackPositionTime = get_dword_time() - AI_CHANGE_ATTACK_POISITION_TIME_NEAR; }
	void			SetChangeAttackPositionTime() { m_dwLastChangeAttackPositionTime = get_dword_time(); }

	bool			OnIdle();

	void			OnClick(entt::entity causer);
	CTrigger&		GetTriggerOnClick() { return m_triggerOnClick; }
	const CTrigger&	GetTriggerOnClick() const { return m_triggerOnClick; }

	entt::entity	 m_entity { entt::null };
	uint32_t		 m_dwLegacyVID { 0 };
	entt::entity	 m_eVictim { entt::null };

protected:
	uint32_t			m_dwLastChangeAttackPositionTime;
	CTrigger		m_triggerOnClick;
	// End of AI

	////////////////////////////////////////////////////////////////////////////////////////
	// Target
protected:

public:
	void				SetTarget(entt::entity target);
	void				BroadcastTargetPacket();
	void				ClearTarget();
	void				CheckTarget();

	////////////////////////////////////////////////////////////////////////////////////////
	// Safebox
public:
	int					GetSafeboxSize() const;
	void				QuerySafeboxSize();
	void				SetSafeboxSize(int size);

	CSafebox* GetSafebox() const;
	void				LoadSafebox(int iSize, uint32_t dwGold, int iItemCount, TPlayerItem* pItems);
	void				ChangeSafeboxSize(uint8_t bSize);
	void				CloseSafebox();

	CMountInventory* GetMountInventory() const;
	void				QueryMountInventory();
	void				LoadMountInventory(const std::vector<TMountInventoryItemTable>& items);
	void                SendMountInventory();

	/// â�� ���� ��û
	/**
	 * @param [in]	pszPassword 1�� �̻� 6�� ������ â�� ��й�ȣ
	 *
	 * DB �� â�����⸦ ��û�Ѵ�.
	 * â���� �ߺ����� ���� ���ϸ�, �ֱ� â���� ���� �ð����� ���� 10�� �̳����� �� �� ���Ѵ�.
	 */
	void				ReqSafeboxLoad(const char* pszPassword);

	/// â�� ���� ��û�� ���
	/**
	 * ReqSafeboxLoad �� ȣ���ϰ� CloseSafebox ���� �ʾ��� �� �� �Լ��� ȣ���ϸ� â���� �� �� �ִ�.
	 * â�������� ��û�� DB �������� ���������� �޾��� ��� �� �Լ��� ����ؼ� ��û�� �� �� �ְ� ���ش�.
	 */
	void				CancelSafeboxLoad(void) { m_bOpeningSafebox = false; }

	void				SetMallLoadTime(int t) { m_iMallLoadTime = t; }
	int					GetMallLoadTime() const { return m_iMallLoadTime; }

	CSafebox* GetMall() const;
	void				LoadMall(int iItemCount, TPlayerItem* pItems);
	void				CloseMall();

	void				SetSafeboxOpenPosition();
	float				GetDistanceFromSafeboxOpen() const;

protected:
	CSafebox* m_pkSafebox;
	int					m_iSafeboxSize;
	int					m_iSafeboxLoadTime;
	bool				m_bOpeningSafebox;	///< â���� ���� ��û ���̰ų� �����ִ°� ����, true �� ��� �����û�̰ų� ��������.

	bool				 m_bMountInventoryLoaded;

	CSafebox* m_pkMall;
	int					m_iMallLoadTime;

	PIXEL_POSITION		m_posSafeboxOpen;

	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	// Mounting
public:
	void				MountVnum(uint32_t vnum);
	uint32_t				GetMountVnum() const { return m_dwMountVnum; }
	uint32_t				GetLastMountTime() const { return m_dwMountTime; }

	bool				CanUseHorseSkill();

	// Horse
	virtual	void		SetHorseLevel(int iLevel);

	virtual	bool		StartRiding();
	virtual	bool		StopRiding();

	virtual	uint32_t		GetMyHorseVnum() const;

	virtual	void		HorseDie();
	virtual bool		ReviveHorse();

	virtual void		SendHorseInfo();
	virtual	void		ClearHorseInfo();

	void				HorseSummon(bool bSummon, bool bFromFar = false, uint32_t dwVnum = 0, const char* name = nullptr);

	LPCHARACTER			GetHorse() const;
	LPCHARACTER			GetRider() const; // rider on horse
	void				SetRider(entt::entity character);

	bool				IsRiding() const;
#ifdef __ATTR_TRANSFER_SYSTEM__
public:
	std::span<entt::entity> GetAttrTransferItem();
	bool IsAttrTransferOpen() const;
	void SetAttrTransferNpc(entt::entity npc);
	bool CanDoAttrTransfer() const;
#endif
#ifdef __PET_SYSTEM__
public:
	CPetSystem* GetPetSystem() { return m_petSystem; }

protected:
	CPetSystem* m_petSystem;

public:
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
public:
	CMountSystem* GetMountSystem() { return m_mountSystem; }

	void 				MountSummon(entt::entity mountItem);
	void 				MountUnsummon(entt::entity mountItem);
	void 				CheckMount();
	bool 				IsRidingMount();

protected:
	CMountSystem* m_mountSystem;
#endif

#ifdef __NEWPET_SYSTEM__
public:
	CNewPetSystem* GetNewPetSystem() { return m_newpetSystem; }

protected:
	CNewPetSystem* m_newpetSystem;

public:
#endif
#ifdef ENABLE_COSTUME_PET
public:
	void	UpdatePetSkin();
	uint32_t	GetPetSkinVnum();

#endif
#ifdef ENABLE_COSTUME_MOUNT
public:
	void	UpdateMountSkin();
	uint32_t	GetMountSkinVnum();
#endif
protected:
	LPCHARACTER			m_chRider;

	uint32_t				m_dwMountVnum;
	uint32_t				m_dwMountTime;


	////////////////////////////////////////////////////////////////////////////////////////
	// Detailed Log
public:
	void				DetailLog() { m_bDetailLog = !m_bDetailLog; }
	void				ToggleMonsterLog();
	void				MonsterLog(const char* format, ...);
private:
	bool				m_bDetailLog;
	bool				m_bMonsterLog;

	////////////////////////////////////////////////////////////////////////////////////////
	// Empire

public:
	void 				SetEmpire(uint8_t bEmpire);
	uint8_t				GetEmpire() const;

protected:
	uint8_t				m_bEmpire;

	////////////////////////////////////////////////////////////////////////////////////////
	// Regen
public:
	void				SetRegen(LPREGEN pkRegen);

protected:
	PIXEL_POSITION			m_posRegen;
	float				m_fRegenAngle;
	LPREGEN				m_pkRegen;
	size_t				regen_id_; // to help dungeon regen identification
	// End of Regen

	////////////////////////////////////////////////////////////////////////////////////////
	// Resists & Proofs
public:
	bool				CannotMoveByAffect() const;	// Ư�� ȿ���� ���� ������ �� ���� �����ΰ�?
	bool				IsImmune(uint32_t dwImmuneFlag);
	void			SetImmuneFlag(uint32_t dw);
	uint32_t			GetImmuneFlag() const;

protected:
	void				ApplyMobAttribute(const TMobTable* table);
	// End of Resists & Proofs

	////////////////////////////////////////////////////////////////////////////////////////
	// QUEST
	//
public:
	void				SetQuestNPCID(uint32_t vid);
	uint32_t				GetQuestNPCID() const { return m_dwQuestNPCVID; }
	LPCHARACTER			GetQuestNPC() const;

	void				SetQuestItemPtr(entt::entity item);
	void				ClearQuestItemPtr();
	entt::entity		GetQuestItemEntity() const;
	LPITEM				GetQuestItemPtr() const;

	void				SetQuestBy(uint32_t dwQuestVnum);
	uint32_t				GetQuestBy() const;

	int					GetQuestFlag(const std::string& flag) const;
	void				SetQuestFlag(const std::string& flag, int value);


private:
	uint32_t				m_dwQuestNPCVID;
	uint32_t				m_dwQuestByVnum;

	// Events
public:
	bool				StartStateMachine(int iPulse = 1);
	void				StopStateMachine();
	void				UpdateStateMachine(uint32_t dwPulse);
	void				SetNextStatePulse(int iPulseNext);

	// ĳ���� �ν��Ͻ� ������Ʈ �Լ�. ������ �̻��� ��ӱ����� CFSM::Update �Լ��� ȣ���ϰų� UpdateStateMachine �Լ��� ����ߴµ�, ������ ������Ʈ �Լ� �߰���.
	void				UpdateCharacter(uint32_t dwPulse);

protected:
	uint32_t				m_dwNextStatePulse;

	// Marriage
public:
	LPCHARACTER			GetMarryPartner() const;
	void				SetMarryPartner(entt::entity character);
	int					GetMarriageBonus(uint32_t dwItemVnum, bool bSum = true);

	void				SetWeddingMap(marriage::WeddingMap* pMap);
	marriage::WeddingMap* GetWeddingMap() const { return m_pWeddingMap; }

private:
	marriage::WeddingMap* m_pWeddingMap;
	LPCHARACTER			m_pkChrMarried;

	// Warp Character
public:
	void				StartWarpNPCEvent();

public:
	void				StartSaveEvent();
	void				StartRecoveryEvent();
	void				StartDestroyWhenIdleEvent();

	LPEVENT				m_pkSaveEvent;
	LPEVENT				m_pkTimedEvent;
	LPEVENT				m_pkAffectEvent;
	LPEVENT				GetTimedEvent() const { return m_pkTimedEvent; }
	LPEVENT&			GetTimedEventRef() { return m_pkTimedEvent; }
	LPEVENT				m_pkWarpNPCEvent;
	//DELAYED_WARP
	//END_DELAYED_WARP

	// MINING
	LPEVENT				m_pkMiningEvent;
	// END_OF_MINING
	LPEVENT				m_pkWarpEvent;
	LPEVENT				m_pkDestroyWhenIdleEvent;
	LPEVENT				m_pkPetSystemUpdateEvent;
#ifdef __NEWPET_SYSTEM__
	LPEVENT				m_pkNewPetSystemUpdateEvent;
	LPEVENT				m_pkNewPetSystemExpireEvent;
#endif
	bool IsWarping() const { return m_pkWarpEvent ? true : false; }


	const CMob* m_pkMobData;
	CMobInstance* m_pkMobInst;
	const CMob* GetMobData() const { return m_pkMobData; }

	std::map<int, LPEVENT> m_mapMobSkillEvent;

	friend struct FuncSplashDamage;
	friend struct FuncSplashAffect;
	friend class CFuncShoot;

public:
	int				GetPremiumRemainSeconds(uint8_t bType) const;

private:
	int				m_aiPremiumTimes[PREMIUM_MAX_NUM];

	// CHANGE_ITEM_ATTRIBUTES
	// static const uint32_t		msc_dwDefaultChangeItemAttrCycle;	///< ����Ʈ ������ �Ӽ����� ���� �ֱ�
	static const char		msc_szLastChangeItemAttrFlag[];		///< �ֱ� ������ �Ӽ��� ������ �ð��� Quest Flag �̸�
	// static const char		msc_szChangeItemAttrCycleFlag[];		///< ������ �Ӽ����� ���� �ֱ��� Quest Flag �̸�
	// END_OF_CHANGE_ITEM_ATTRIBUTES

	// PC_BANG_ITEM_ADD
private:
	bool m_isinPCBang;

public:
	bool SetPCBang(bool flag);
	bool IsPCBang() const { return m_isinPCBang; }
	// END_PC_BANG_ITEM_ADD

	// NEW_HAIR_STYLE_ADD
public:
	bool ItemProcess_Hair(LPITEM item, int iDestCell);
	// END_NEW_HAIR_STYLE_ADD

public:
	void ClearSkill();
	void ClearSubSkill();

	// RESET_ONE_SKILL
	bool ResetOneSkill(uint32_t dwVnum);
	// END_RESET_ONE_SKILL


	// ARENA
private:
	CArena* m_pArena;
	bool m_ArenaObserver;

public:
	void 	SetArena(CArena* pArena);
	void	SetArenaObserverMode(bool flag);

	CArena* GetArena() const;
	bool	GetArenaObserverMode() const;

	void	SetPotionLimit(int count);
	int		GetPotionLimit() const;
	// END_ARENA

		//PREVENT_TRADE_WINDOW
public:
	bool	IsOpenSafebox() const { return m_isOpenSafebox ? true : false; }
	void 	SetOpenSafebox(bool b);

	int		GetSafeboxLoadTime() const { return m_iSafeboxLoadTime; }
	void	SetSafeboxLoadTime();
	//END_PREVENT_TRADE_WINDOW
private:
	bool	m_isOpenSafebox;

public:
	int		GetSkillPowerByLevel(int level, bool bMob = false) const;

	//PREVENT_REFINE_HACK
	int		GetRefineTime() const { return m_iRefineTime; }
	void	SetRefineTime();
	int		m_iRefineTime;
	//END_PREVENT_REFINE_HACK

	//RESTRICT_USE_SEED_OR_MOONBOTTLE
	int 	GetUseSeedOrMoonBottleTime() const { return m_iSeedTime; }
	void  	SetUseSeedOrMoonBottleTime() { m_iSeedTime = thecore_pulse(); }
	int 	m_iSeedTime;
	//END_RESTRICT_USE_SEED_OR_MOONBOTTLE

	//PREVENT_PORTAL_AFTER_EXCHANGE
	int		GetExchangeTime() const { return m_iExchangeTime; }
	void	SetExchangeTime();
	int		m_iExchangeTime;
	//END_PREVENT_PORTAL_AFTER_EXCHANGE

	int 	m_iMyShopTime;
	int		GetMyShopTime() const { return m_iMyShopTime; }
	void	SetMyShopTime();

	// Hack ������ ���� üũ.
	bool	IsHack(bool bSendMsg = true, bool bCheckShopOwner = true, int limittime = g_nPortalLimitTime);

	void Say(const std::string& s);

public:
	bool ItemProcess_Polymorph(LPITEM item);

	// by mhh
	std::span<entt::entity> GetCubeItem();
	bool IsCubeOpen() const;
	void SetCubeNpc(entt::entity npc);
	bool CanDoCube() const;


private:
	int		m_deposit_pulse;

public:
	void	UpdateDepositPulse();
	bool	CanDeposit() const;

private:
	void	__OpenPrivateShop(
#ifdef KASMIR_PAKET_SYSTEM
		bool bKasmir = false
#endif
	);

public:
	struct AttackedLog
	{
		uint32_t 	dwPID;
		uint32_t	dwAttackedTime;

		AttackedLog() : dwPID(0), dwAttackedTime(0)
		{
		}
	};

	AttackLog	m_kAttackLog;
	AttackedLog m_AttackedLog;
	int			m_speed_hack_count;
	const AttackLog& GetAttackLog() const { return m_kAttackLog; }
	AttackLog& GetAttackLogRef() { return m_kAttackLog; }
	const AttackedLog& GetAttackedLog() const { return m_AttackedLog; }
	AttackedLog& GetAttackedLogRef() { return m_AttackedLog; }
	int GetSpeedHackCount() const { return m_speed_hack_count; }
	int& GetSpeedHackCountRef() { return m_speed_hack_count; }

private:
	std::string m_strNewName;

public:
	const std::string GetNewName() const;
	void SetNewName(const std::string name);

public:
	void GoHome();

private:

public:
	void SendGuildName(CGuild* pGuild);
	void SendGuildName(uint32_t dwGuildID);

private:
	uint32_t m_dwLogOffInterval;

public:
	uint32_t GetLogOffInterval() const { return m_dwLogOffInterval; }

public:
	bool UnEquipSpecialRideUniqueItem();

	bool CanWarp() const;

private:
	uint32_t m_dwLastGoldDropTime;
#ifdef ENABLE_NEWSTUFF
	uint32_t m_dwLastBoxUseTime;
	uint32_t m_dwLastBuySellTime;
public:
	uint32_t GetLastBuySellTime() const { return m_dwLastBuySellTime; }
	void SetLastBuySellTime(uint32_t dwLastBuySellTime) { m_dwLastBuySellTime = dwLastBuySellTime; }
#endif
public:
	void AutoRecoveryItemProcess(const EAffectTypes);
#ifdef ENABLE_RECALL
	void AutoRecallProcess();
#endif

public:
	void BuffOnAttr_AddBuffsFromItem(LPITEM pItem);
	void BuffOnAttr_RemoveBuffsFromItem(LPITEM pItem);

private:
public:
	// Driven by the point-change flow, which lives in ecs::PointSystem now.
	void BuffOnAttr_ValueChange(uint8_t bType, uint8_t bOldValue, uint8_t bNewValue);
	uint32_t GetPlayStartTime() const { return m_dwPlayStartTime; }
private:
	void BuffOnAttr_ClearAll();

	// ���� : ��Ȱ�� �׽�Ʈ�� ���Ͽ�.
public:
	void SetArmada() { cannot_dead = true; }
	void ResetArmada() { cannot_dead = false; }
private:
	bool cannot_dead;
#ifdef __PET_SYSTEM__
private:
	bool m_bIsPet;
public:
	void SetPet();
	bool IsPet() { return m_bIsPet; }
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
private:
	bool m_bIsMount;
public:
	void SetMount();
	bool IsMount() { return m_bIsMount; }
#endif

#ifdef __NEWPET_SYSTEM__
private:
	bool m_bIsNewPet;
	int m_eggvid;
public:
	void SetNewPet();
	bool IsNewPet() const { return m_bIsNewPet ? true : false; }
	void SetEggVid(int vid);
	int GetEggVid() const;

#endif

	//���� ������ ����.
private:
	float m_fAttMul;
	float m_fDamMul;
public:
	float GetAttMul() { return this->m_fAttMul; }
	void SetAttMul(float newAttMul) { this->m_fAttMul = newAttMul; }
	float GetDamMul() { return this->m_fDamMul; }
	void SetDamMul(float newDamMul) { this->m_fDamMul = newDamMul; }

private:
	bool IsValidItemPosition(TItemPos Pos) const;

public:
	//��ȥ��

	// ĳ������ affect, quest�� load �Ǳ� ���� DragonSoul_Initialize�� ȣ���ϸ� �ȵȴ�.
	// affect�� ���� �������� �ε�Ǿ� LoadAffect���� ȣ����.
	void	DragonSoul_Initialize();

	int		DragonSoul_GetActiveDeck() const;
	bool	DragonSoul_IsDeckActivated() const;
	bool	DragonSoul_ActivateDeck(int deck_idx);

	void	DragonSoul_DeactivateAll();
	// �ݵ�� ClearItem ���� �ҷ��� �Ѵ�.
	// �ֳ��ϸ�....
	// ��ȥ�� �ϳ� �ϳ��� deactivate�� ������ ���� active�� ��ȥ���� �ִ��� Ȯ���ϰ�,
	// active�� ��ȥ���� �ϳ��� ���ٸ�, ĳ������ ��ȥ�� affect��, Ȱ�� ���¸� �����Ѵ�.
	//
	// ������ ClearItem ��, ĳ���Ͱ� �����ϰ� �ִ� ��� �������� unequip�ϴ� �ٶ���,
	// ��ȥ�� Affect�� ���ŵǰ�, �ᱹ �α��� ��, ��ȥ���� Ȱ��ȭ���� �ʴ´�.
	// (Unequip�� ������ �α׾ƿ� ��������, �ƴ��� �� �� ����.)
	// ��ȥ���� deactivate��Ű�� ĳ������ ��ȥ�� �� Ȱ�� ���´� �ǵ帮�� �ʴ´�.
	void	DragonSoul_CleanUp();
	// ��ȥ�� ��ȭâ
public:
	bool		DragonSoul_RefineWindow_Open(LPENTITY pEntity);
	bool		DragonSoul_RefineWindow_Close();
	LPENTITY	DragonSoul_RefineWindow_GetOpener();
	bool		DragonSoul_RefineWindow_CanRefine();
#if defined(BL_OFFLINE_MESSAGE)
protected:
	uint32_t				dwLastOfflinePMTime;
public:
	uint32_t				GetLastOfflinePMTime() const { return dwLastOfflinePMTime; }
	void				SetLastOfflinePMTime() { dwLastOfflinePMTime = get_dword_time(); }
	void				SendOfflineMessage(const char* To, const char* Message);
	void				ReadOfflineMessages();
#endif
	//���� ���� ��� ��Ŷ �ӽ� ����
private:
	unsigned int itemAward_vnum;
	char		 itemAward_cmd[20];
	//bool		 itemAward_flag;
public:
	unsigned int GetItemAward_vnum() { return itemAward_vnum; }
	char* GetItemAward_cmd() { return itemAward_cmd; }
	//bool		 GetItemAward_flag() { return itemAward_flag; }
	void		 SetItemAward_vnum(unsigned int vnum);
	void		 SetItemAward_cmd(char* cmd);
	//void		 SetItemAward_flag(bool flag) { itemAward_flag = flag; }
#ifdef ENABLE_ANTI_CMD_FLOOD
private:
	int m_dwCmdAntiFloodPulse;
	uint32_t m_dwCmdAntiFloodCount;
public:
	int GetCmdAntiFloodPulse() { return m_dwCmdAntiFloodPulse; }
	uint32_t GetCmdAntiFloodCount() { return m_dwCmdAntiFloodCount; }
	uint32_t IncreaseCmdAntiFloodCount() { return ++m_dwCmdAntiFloodCount; }
	void SetCmdAntiFloodPulse(int dwPulse) { m_dwCmdAntiFloodPulse = dwPulse; }
	void SetCmdAntiFloodCount(uint32_t dwCount) { m_dwCmdAntiFloodCount = dwCount; }
#endif
private:
	// SyncPosition�� �ǿ��Ͽ� Ÿ������ �̻��� ������ ������ �� ����ϱ� ���Ͽ�,
	// SyncPosition�� �Ͼ ���� ���.
	int			m_iSyncHackCount;
public:
	void			SetLastSyncTime(const timeval& tv);
	const timeval& GetLastSyncTime() const;
	void			SetSyncHackCount(int iCount) { m_iSyncHackCount = iCount; }
	int				GetSyncHackCount() { return m_iSyncHackCount; }

#ifdef ENABLE_ACCE_SYSTEM
protected:
	bool	m_bAcceCombination, m_bAcceAbsorption;

public:
	bool	isAcceOpened(bool bCombination) { return bCombination ? m_bAcceCombination : m_bAcceAbsorption; }
	void	OpenAcce(bool bCombination);
	void	CloseAcce();
	void	ClearAcceMaterials();
	bool	CleanAcceAttr(entt::entity item, entt::entity target);
	std::span<entt::entity> GetAcceMaterials();
	bool	AcceIsSameGrade(int32_t lGrade);
	uint32_t	GetAcceCombinePrice(int32_t lGrade
#ifdef ENABLE_STOLE_COSTUME
		, bool isCostume
#endif
	);
	void	GetAcceCombineResult(uint32_t& dwItemVnum, uint32_t& dwMinAbs, uint32_t& dwMaxAbs);
	uint8_t	CheckEmptyMaterialSlot();
	void	AddAcceMaterial(TItemPos tPos, uint8_t bPos);
	void	RemoveAcceMaterial(uint8_t bPos);
	uint8_t	CanRefineAcceMaterials();
	void	RefineAcceMaterials();
	bool	IsAcceOpen() const { return m_bAcceCombination || m_bAcceAbsorption; }
#endif



#ifdef __HIDE_COSTUME_SYSTEM__
public:
	void SetBodyCostumeHidden(bool hidden, bool pass = false);
	bool IsBodyCostumeHidden() const;

	void SetHairCostumeHidden(bool hidden, bool pass = false);
	bool IsHairCostumeHidden() const;
#ifdef ENABLE_FREE_PASS_RAZOR93

	bool HasBattlePassBoost(uint8_t bBattlePassId);
	uint32_t GetBattlePassAdjustedTotal(uint32_t dwMissionID, uint32_t dwBattlePassID, uint32_t dwBaseTotal);
	void ApplyBattlePassBoostRecalc(uint8_t bBattlePassId);

	void EnsureFreeBattlePassActive();

#endif

#ifdef ENABLE_ACCE_SYSTEM
	void SetAcceCostumeHidden(bool hidden, bool pass = false);
	bool IsAcceCostumeHidden() const;
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	void SetWeaponCostumeHidden(bool hidden, bool pass = false);
	bool IsWeaponCostumeHidden() const;
#endif

private:
	bool m_bHideBodyCostume;
	bool m_bHideHairCostume;
#ifdef ENABLE_ACCE_SYSTEM
	bool m_bHideAcceCostume;
#endif
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
	//uint32_t m_dwBattlePassStayOnlineNextTick;

	LPEVENT m_pkBattlePassStayOnlineEvent;

#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	bool m_bHideWeaponCostume;
#endif
#endif



#ifdef ENABLE_GAYA_SYSTEM
public:
	bool CheckItemsFull();
	void UpdateItemsGayaMarker0();
	void UpdateItemsGayaMarker();
	void InfoGayaMarker();
	void ClearGayaMarket();
	bool CheckSlotGayaMarket(int slot);
	void UpdateSlotGayaMarket(int slot);
	void BuyItemsGayaMarket(int slot);
	void RefreshItemsGayaMarket();
	void CraftGayaItems(int slot);
	void MarketGayaItems(int slot);
	void RefreshGayaItems();
	void LOAD_GAYA();
	int GetGayaState(const std::string& state) const;
	void SetGayaState(const std::string& state, int szValue);
	void StartCheckTimeMarket();
#endif



#ifdef ENABLE_SOUL_SYSTEM
public:
	bool 		DoRefineItemSoul(LPITEM item);
	int 		GetSoulItemDamage(entt::entity victim, int iDamage, uint8_t bSoulType);
#endif

#ifdef ENABLE_BATTLE_PASS
public:
	void LoadBattlePass(uint32_t dwCount, TPlayerBattlePassMission* data);
	uint32_t GetMissionProgress(uint32_t dwMissionID, uint32_t dwBattlePassID);
	void UpdateMissionProgress(uint32_t dwMissionID, uint32_t dwBattlePassID, uint32_t dwUpdateValue, uint32_t dwTotalValue, bool isOverride = false);
	bool IsCompletedMission(uint8_t bMissionType);
	bool IsLoadedBattlePass() const { return m_bIsLoadedBattlePass; }
	uint8_t GetBattlePassId();

private:
	bool m_bIsLoadedBattlePass;
	std::list<TPlayerBattlePassMission*> m_listBattlePass;

public:
	int 			GetSecondsTillNextMonth();
	int 			GetBattlePassEndTime() { return (m_dwBattlePassEndTime - get_global_time()); };
protected:
	uint32_t			m_dwBattlePassEndTime;

#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE		
public:
	LPEVENT				m_pkStayOnlineEvent;
	void	CancelStayOnlineEvent();
	//void 	LoadStayActiveBattlePass();
#endif
#endif

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
	std::string GetLang();
#endif
#ifdef ENABLE_RUNE_SYSTEM
public:
	uint16_t	GetRuneEffect();
#endif
#ifdef TEXTS_IMPROVEMENT
public:
#endif
#ifdef ENABLE_NEW_FISHING_SYSTEM
public:
	void fishing_new_start();
	void fishing_new_stop();
	void fishing_new_catch();
	void fishing_new_catch_failed();
	void fishing_catch_decision(uint32_t itemVnum);
#endif
public:
	int		GetGoToXYTime() const { return m_iGoToXYTime; }
	void	SetGoToXYTime() { m_iGoToXYTime = thecore_pulse(); }

protected:
	int		m_iGoToXYTime;

#ifdef ENABLE_SAVEPOINT_SYSTEM
public:
	int		GetSavePointTime() const { return m_iSavePointTime; }
	void	SetSavePointTime() { m_iSavePointTime = thecore_pulse(); }

protected:
	int		m_iSavePointTime;
#endif

#ifdef ENABLE_SORT_INVEN
public:
	int		GetSortInv1Time() const { return m_iSortInv1Time; }
	void	SetSortInv1Time() { m_iSortInv1Time = thecore_pulse(); }
	int		GetSortInv2Time() const { return m_iSortInv2Time; }
	void	SetSortInv2Time() { m_iSortInv2Time = thecore_pulse(); }

protected:
	int		m_iSortInv1Time;
	int		m_iSortInv2Time;
#endif

#ifdef ENABLE_LIMIT_BUY_SPEED
public:
	int			GetLastBuyTime() const { return m_iLastBuyTime; }
	void		SetLastBuyTime() { m_iLastBuyTime = thecore_pulse(); }

protected:
	int			m_iLastBuyTime;
#endif

#ifdef ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
public:
	bool	GetDeadByMonster() const { return m_deadByMonster; }
	void	SetDeadByMonster(bool flag) { m_deadByMonster = flag; }

protected:
	bool	m_deadByMonster;
#endif

#ifdef ENABLE_SPAM_CHECK
public:
	int32_t	GetLastUnlock() const { return m_iLastUnlock; }
	void	SetLastUnlock() { m_iLastUnlock = get_global_time() + 3; }

protected:
	int32_t	m_iLastUnlock;
#endif

public:
#ifdef ENABLE_BIOLOGIST_UI
	void CheckBiologistReward();
#endif
#ifdef ENABLE_ANTICHEAT
	void ClearCheatChecks();
	void ProcessCheatCheck(int32_t time);
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
	void BlockProcessed();
	void BlockDrop();
	void UnblockDrop();
	void SetDropStatus();
	void ComputeMountInventoryBonuses();
#endif

#ifdef __DEFENSE_WAVE__
	bool IsDefanceWaweMastAttackMob(int32_t vnum) const { return (vnum >= 3401 && vnum <= 3405) || (vnum >= 3601 && vnum <= 3605) || (vnum >= 3950 && vnum <= 3964); }
#endif
//#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
//	void SetWheelDestiny(std::shared_ptr<CWheelDestiny> pt) { pWheelDestiny = std::move(pt); };
//	std::shared_ptr<CWheelDestiny> GetWheelDestiny() const { return pWheelDestiny; }
//#endif

protected:
#ifdef ENABLE_ANTICHEAT
	int32_t m_firstReward, m_rewardCount, m_checkRepeated, m_dropitemcount, m_lastdropitem;
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
	LPEVENT m_pkDropEvent;
#endif

#ifdef ENABLE_USEITEM_COOLDOWN
private:
	int m_dwItemUseAntiFloodPulse;
	uint32_t m_dwItemUseAntiFloodCount;
//#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
//	std::shared_ptr<CWheelDestiny> pWheelDestiny = nullptr;
//#endif
public:
	int GetItemUseAntiFloodPulse() { return m_dwItemUseAntiFloodPulse; }
	uint32_t GetItemUseAntiFloodCount() { return m_dwItemUseAntiFloodCount; }
	uint32_t IncreaseItemUseAntiFloodCount() { return ++m_dwItemUseAntiFloodCount; }
	void SetItemUseAntiFloodPulse(int dwPulse) { m_dwItemUseAntiFloodPulse = dwPulse; }
	void SetItemUseAntiFloodCount(uint32_t dwCount) { m_dwItemUseAntiFloodCount = dwCount; }
#endif


};

ESex GET_SEX(LPCHARACTER ch);

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









