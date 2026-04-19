#ifndef __INC_METIN_II_CHAR_H__
#define __INC_METIN_II_CHAR_H__

#include <unordered_map>
#include <chrono>
#include <vector>

#include <common/tables.h>
#include <common/CommonDefines.h>
#include <common/stl.h>
#include "entity.h"
#include "FSM.h"
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
	//스킬
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
	POINT_STAMINA,              // 9  스테미너
	POINT_MAX_STAMINA,          // 10 최대 스테미너

	POINT_GOLD,                 // 11
	POINT_ST,                   // 12 근력
	POINT_HT,                   // 13 체력
	POINT_DX,                   // 14 민첩성
	POINT_IQ,                   // 15 정신력
	POINT_DEF_GRADE,		// 16 ...
	POINT_ATT_SPEED,            // 17 공격속도
	POINT_ATT_GRADE,		// 18 공격력 MAX
	POINT_MOV_SPEED,            // 19 이동속도
	POINT_CLIENT_DEF_GRADE,	// 20 방어등급
	POINT_CASTING_SPEED,        // 21 주문속도 (쿨다운타임*100) / (100 + 이값) = 최종 쿨다운 타임
	POINT_MAGIC_ATT_GRADE,      // 22 마법공격력
	POINT_MAGIC_DEF_GRADE,      // 23 마법방어력
	POINT_EMPIRE_POINT,         // 24 제국점수
	POINT_LEVEL_STEP,           // 25 한 레벨에서의 단계.. (1 2 3 될 때 보상, 4 되면 레벨 업)
	POINT_STAT,                 // 26 능력치 올릴 수 있는 개수
	POINT_SUB_SKILL,		// 27 보조 스킬 포인트
	POINT_SKILL,		// 28 액티브 스킬 포인트
	POINT_WEAPON_MIN,		// 29 무기 최소 데미지
	POINT_WEAPON_MAX,		// 30 무기 최대 데미지
	POINT_PLAYTIME,             // 31 플레이시간
	POINT_HP_REGEN,             // 32 HP 회복률
	POINT_SP_REGEN,             // 33 SP 회복률

	POINT_BOW_DISTANCE,         // 34 활 사정거리 증가치 (meter)

	POINT_HP_RECOVERY,          // 35 체력 회복 증가량
	POINT_SP_RECOVERY,          // 36 정신력 회복 증가량

	POINT_POISON_PCT,           // 37 독 확률
	POINT_STUN_PCT,             // 38 기절 확률
	POINT_SLOW_PCT,             // 39 슬로우 확률
	POINT_CRITICAL_PCT,         // 40 크리티컬 확률
	POINT_PENETRATE_PCT,        // 41 관통타격 확률
	POINT_CURSE_PCT,            // 42 저주 확률

	POINT_ATTBONUS_HUMAN,       // 43 인간에게 강함
	POINT_ATTBONUS_ANIMAL,      // 44 동물에게 데미지 % 증가
	POINT_ATTBONUS_ORC,         // 45 웅귀에게 데미지 % 증가
	POINT_ATTBONUS_MILGYO,      // 46 밀교에게 데미지 % 증가
	POINT_ATTBONUS_UNDEAD,      // 47 시체에게 데미지 % 증가
	POINT_ATTBONUS_DEVIL,       // 48 마귀(악마)에게 데미지 % 증가
	POINT_ATTBONUS_INSECT,      // 49 벌레족
	POINT_ATTBONUS_FIRE,        // 50 화염족
	POINT_ATTBONUS_ICE,         // 51 빙설족
	POINT_ATTBONUS_DESERT,      // 52 사막족
	POINT_ATTBONUS_MONSTER,     // 53 모든 몬스터에게 강함
	POINT_ATTBONUS_WARRIOR,     // 54 무사에게 강함
	POINT_ATTBONUS_ASSASSIN,	// 55 자객에게 강함
	POINT_ATTBONUS_SURA,		// 56 수라에게 강함
	POINT_ATTBONUS_SHAMAN,		// 57 무당에게 강함
	POINT_ATTBONUS_TREE,     	// 58 나무에게 강함 20050729.myevan UNUSED5

	POINT_RESIST_WARRIOR,		// 59 무사에게 저항
	POINT_RESIST_ASSASSIN,		// 60 자객에게 저항
	POINT_RESIST_SURA,			// 61 수라에게 저항
	POINT_RESIST_SHAMAN,		// 62 무당에게 저항

	POINT_STEAL_HP,             // 63 생명력 흡수
	POINT_STEAL_SP,             // 64 정신력 흡수

	POINT_MANA_BURN_PCT,        // 65 마나 번

	/// 피해시 보너스 ///

	POINT_DAMAGE_SP_RECOVER,    // 66 공격당할 시 정신력 회복 확률

	POINT_BLOCK,                // 67 블럭율
	POINT_DODGE,                // 68 회피율

	POINT_RESIST_SWORD,         // 69
	POINT_RESIST_TWOHAND,       // 70
	POINT_RESIST_DAGGER,        // 71
	POINT_RESIST_BELL,          // 72
	POINT_RESIST_FAN,           // 73
	POINT_RESIST_BOW,           // 74  화살   저항   : 대미지 감소
	POINT_RESIST_FIRE,          // 75  화염   저항   : 화염공격에 대한 대미지 감소
	POINT_RESIST_ELEC,          // 76  전기   저항   : 전기공격에 대한 대미지 감소
	POINT_RESIST_MAGIC,         // 77  술법   저항   : 모든술법에 대한 대미지 감소
	POINT_RESIST_WIND,          // 78  바람   저항   : 바람공격에 대한 대미지 감소

	POINT_REFLECT_MELEE,        // 79 공격 반사

	/// 특수 피해시 ///
	POINT_REFLECT_CURSE,		// 80 저주 반사
	POINT_POISON_REDUCE,		// 81 독데미지 감소

	/// 적 소멸시 ///
	POINT_KILL_SP_RECOVER,		// 82 적 소멸시 MP 회복
	POINT_EXP_DOUBLE_BONUS,		// 83
	POINT_GOLD_DOUBLE_BONUS,		// 84
	POINT_ITEM_DROP_BONUS,		// 85

	/// 회복 관련 ///
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
	POINT_MANASHIELD,			// 102 흑신수호 스킬에 의한 마나쉴드 효과 정도

	POINT_PARTY_BUFFER_BONUS,		// 103
	POINT_PARTY_SKILL_MASTER_BONUS,	// 104

	POINT_HP_RECOVER_CONTINUE,		// 105
	POINT_SP_RECOVER_CONTINUE,		// 106

	POINT_STEAL_GOLD,			// 107
	POINT_POLYMORPH,			// 108 변신한 몬스터 번호
	POINT_MOUNT,			// 109 타고있는 몬스터 번호

	POINT_PARTY_HASTE_BONUS,		// 110
	POINT_PARTY_DEFENDER_BONUS,		// 111
	POINT_STAT_RESET_COUNT,		// 112 피의 단약 사용을 통한 스텟 리셋 포인트 (1당 1포인트 리셋가능)

	POINT_HORSE_SKILL,			// 113

	POINT_MALL_ATTBONUS,		// 114 공격력 +x%
	POINT_MALL_DEFBONUS,		// 115 방어력 +x%
	POINT_MALL_EXPBONUS,		// 116 경험치 +x%
	POINT_MALL_ITEMBONUS,		// 117 아이템 드롭율 x/10배
	POINT_MALL_GOLDBONUS,		// 118 돈 드롭율 x/10배

	POINT_MAX_HP_PCT,			// 119 최대생명력 +x%
	POINT_MAX_SP_PCT,			// 120 최대정신력 +x%

	POINT_SKILL_DAMAGE_BONUS,		// 121 스킬 데미지 *(100+x)%
	POINT_NORMAL_HIT_DAMAGE_BONUS,	// 122 평타 데미지 *(100+x)%

	// DEFEND_BONUS_ATTRIBUTES
	POINT_SKILL_DEFEND_BONUS,		// 123 스킬 방어 데미지
	POINT_NORMAL_HIT_DEFEND_BONUS,	// 124 평타 방어 데미지
	// END_OF_DEFEND_BONUS_ATTRIBUTES

	// PC_BANG_ITEM_ADD
	POINT_PC_BANG_EXP_BONUS,		// 125 PC방 전용 경험치 보너스
	POINT_PC_BANG_DROP_BONUS,		// 126 PC방 전용 드롭률 보너스
	// END_PC_BANG_ITEM_ADD
	POINT_RAMADAN_CANDY_BONUS_EXP,			// 라마단 사탕 경험치 증가용

	POINT_ENERGY = 128,					// 128 기력

	// 기력 ui 용.
	// 서버에서 쓰지 않기만, 클라이언트에서 기력의 끝 시간을 POINT로 관리하기 때문에 이렇게 한다.
	// 아 부끄럽다
	POINT_ENERGY_END_TIME = 129,					// 129 기력 종료 시간

	POINT_COSTUME_ATTR_BONUS = 130,
	POINT_MAGIC_ATT_BONUS_PER = 131,
	POINT_MELEE_MAGIC_ATT_BONUS_PER = 132,

	// 추가 속성 저항
	POINT_RESIST_ICE = 133,          //   냉기 저항   : 얼음공격에 대한 대미지 감소
	POINT_RESIST_EARTH = 134,        //   대지 저항   : 얼음공격에 대한 대미지 감소
	POINT_RESIST_DARK = 135,         //   어둠 저항   : 얼음공격에 대한 대미지 감소

	POINT_RESIST_CRITICAL = 136,		// 크리티컬 저항	: 상대의 크리티컬 확률을 감소
	POINT_RESIST_PENETRATE = 137,		// 관통타격 저항	: 상대의 관통타격 확률을 감소

#ifdef ENABLE_WOLFMAN_CHARACTER
	POINT_BLEEDING_REDUCE = 138,
	POINT_BLEEDING_PCT = 139,

	POINT_ATTBONUS_WOLFMAN = 140,				// 140 수인족에게 강함
	POINT_RESIST_WOLFMAN = 141,				// 141 수인족에게 저항
	POINT_RESIST_CLAW = 142,					// 142 CLAW에 저항
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
struct DynamicCharacterPtr {
	DynamicCharacterPtr() : is_pc(false), id(0) {}
	DynamicCharacterPtr(const DynamicCharacterPtr& o)
		: is_pc(o.is_pc), id(o.id) {
	}

	// Returns the LPCHARACTER found in CHARACTER_MANAGER.
	LPCHARACTER Get() const;
	// Clears the current settings.
	void Reset() {
		is_pc = false;
		id = 0;
	}

	// Basic assignment operator.
	DynamicCharacterPtr& operator=(const DynamicCharacterPtr& rhs) {
		is_pc = rhs.is_pc;
		id = rhs.id;
		return *this;
	}
	// Supports assignment with LPCHARACTER type.
	DynamicCharacterPtr& operator=(LPCHARACTER character);
	// Supports type casting to LPCHARACTER.
	operator LPCHARACTER() const {
		return Get();
	}

	bool is_pc;
	uint32_t id;
};


/* 저장하는 데이터 */
typedef struct character_point
{

	int64_t			points[POINT_MAX_NUM];

	uint8_t			job;
	uint8_t			voice;

	uint8_t			level;
	uint32_t			exp;

	int64_t		gold;

#ifdef ENABLE_GAYA_SYSTEM
	int				gaya;
#endif

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	int 			envanter;
#endif

	int64_t				hp;
	int64_t				sp;

	int				iRandomHP;
	int				iRandomSP;

	int				stamina;

	uint8_t			skill_group;
} CHARACTER_POINT;

/* 저장되지 않는 캐릭터 데이터 */
typedef struct character_point_instant
{
	int64_t			points[POINT_MAX_NUM];

	float			fRot;

	int64_t				iMaxHP;
	int64_t				iMaxSP;

	int32_t			position;

	int32_t			instant_flag;
	uint32_t			dwAIFlag;
	uint32_t			dwImmuneFlag;
	uint32_t			dwLastShoutPulse;

	uint16_t			parts[PART_MAX_NUM];

	// 아... 진짜 욕을 안 할래야 안 할 수가 없다.
	// char는 인벤을 uint8_t array로 grid를 관리하고, exchange나 cube는 CGrid로 grid를 관리하고 뭐냐 이거...
	// grid를 만들어 놨으면 grid를 쓰란 말이야!!!
	// ㅅㅂ 용혼석 인벤을 똑같이 따라서 만든 나도 잘못했다 ㅠㅠ
	LPITEM			pItems[INVENTORY_AND_EQUIP_SLOT_MAX];
	uint16_t			bItemGrid[INVENTORY_AND_EQUIP_SLOT_MAX];

	// 용혼석 인벤토리.
	LPITEM			pDSItems[DRAGON_SOUL_INVENTORY_MAX_NUM];
	uint16_t			wDSItemGrid[DRAGON_SOUL_INVENTORY_MAX_NUM];
#ifdef ENABLE_EXTRA_INVENTORY
#endif
#ifdef ENABLE_SWITCHBOT
	LPITEM			pSwitchbotItems[SWITCHBOT_SLOT_COUNT];
#endif
	// by mhh
#ifdef __ATTR_TRANSFER_SYSTEM__
#endif
#ifdef ENABLE_ACCE_SYSTEM
	LPITEM				pAcceMaterials[ACCE_WINDOW_MAX_MATERIALS];
#endif
	LPCHARACTER			battle_victim;

	uint8_t			gm_level;

	uint8_t			bBasePart;	// 평상복 번호

	int64_t				iMaxStamina;

	uint8_t			bBlockMode;

	int				iDragonSoulActiveDeck;
	LPENTITY		m_pDragonSoulRefineWindowOpener;
} CHARACTER_POINT_INSTANT;

#define TRIGGERPARAM		LPCHARACTER ch, LPCHARACTER causer

typedef struct trigger
{
	uint8_t	type;
	int		(*func) (TRIGGERPARAM);
	int32_t	value;
} TRIGGER;

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
	DynamicCharacterPtr ch;
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

class CHARACTER : public CEntity, public CFSM, public CHorseRider
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
	// Entity 관련
	virtual void	EncodeInsertPacket(LPENTITY entity);
	virtual void	EncodeRemovePacket(LPENTITY entity);
	//////////////////////////////////////////////////////////////////////////////////

public:
	LPCHARACTER			FindCharacterInView(const char* name, bool bFindPCOnly);
	void				UpdatePacket();
	int m_lastBeltMountCount;

#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
public:
	std::string			GetItemOnTitlePrefix() const;
	std::string			GetDisplayedNameWithItemOnTitle() const;
	void				UpdateItemOnTitleName(bool bForce = false);
	void				SendItemOnTitleNameToDesc(LPDESC d) const;

private:
	std::string			m_lastItemOnTitlePrefix;
#endif


#ifdef ENABLE_VOTE4BUFF
public:
	long long	GetVoteCoin();
	void		SetVoteCoin(long long amount);
#endif

public:
	uint32_t GetAIFlag() const { return m_pointsInstant.dwAIFlag; }

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
	void			CreatePlayerProto(TPlayerTable& tab);	// 저장 시 사용

	void			SetProto(const CMob* c_pkMob);
	uint16_t			GetRaceNum() const;

	void			Save();		// DelayedSave
	void			SaveReal();	// 실제 저장
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
	void SetDungeonTicketExtraMetin(bool b) { m_bDungeonTicketExtraMetin = b; }
	bool IsDungeonTicketExtraMetin() const { return m_bDungeonTicketExtraMetin; }
#endif

	uint32_t			GetLastShoutPulse() const { return m_pointsInstant.dwLastShoutPulse; }
	void			SetLastShoutPulse(uint32_t pulse) { m_pointsInstant.dwLastShoutPulse = pulse; }
	int				GetLevel() const { return m_points.level; }
	void			SetLevel(uint8_t level);

	uint8_t			GetGMLevel() const;
	BOOL 			IsGM() const;
	void			SetGMLevel();

	uint32_t			GetExp() const { return m_points.exp; }
	void			SetExp(uint32_t exp) { m_points.exp = exp; }

#ifdef __ENABLE_BLOCK_EXP__
	bool			Block_Exp;
#endif
	uint32_t			GetNextExp() const;
#ifdef __NEWPET_SYSTEM__
	uint32_t			PetGetNextExp() const;
#endif
	LPCHARACTER		DistributeExp();

	// 제일 많이 때린 사람을 리턴한다.
	void			DistributeHP(LPCHARACTER pkKiller);
	void			DistributeSP(LPCHARACTER pkKiller, int iMethod = 0);

	void			SetPosition(int pos);
	bool			IsPosition(int pos) const { return m_pointsInstant.position == pos ? true : false; }
	int				GetPosition() const { return m_pointsInstant.position; }

	void			SetPart(uint8_t bPartPos, uint16_t wVal);
	uint16_t			GetPart(uint8_t bPartPos) const;
	uint16_t			GetOriginalPart(uint8_t bPartPos) const;

	void			SetHP(int64_t hp) { m_points.hp = hp; }
	int64_t				GetHP() const { return m_points.hp; }

	void			SetSP(int64_t sp) { m_points.sp = sp; }
	int64_t				GetSP() const { return m_points.sp; }

	void			SetStamina(int stamina) { m_points.stamina = stamina; }
	int				GetStamina() const { return m_points.stamina; }

	void			SetMaxHP(int64_t iVal) { m_pointsInstant.iMaxHP = iVal; }
	int64_t				GetMaxHP() const { return m_pointsInstant.iMaxHP; }

	void			SetMaxSP(int64_t iVal) { m_pointsInstant.iMaxSP = iVal; }
	int64_t				GetMaxSP() const { return m_pointsInstant.iMaxSP; }

	void			SetMaxStamina(int64_t iVal) { m_pointsInstant.iMaxStamina = iVal; }
	int64_t				GetMaxStamina() const { return m_pointsInstant.iMaxStamina; }

	void			SetRandomHP(int v) { m_points.iRandomHP = v; }
	void			SetRandomSP(int v) { m_points.iRandomSP = v; }

	int				GetRandomHP() const { return m_points.iRandomHP; }
	int				GetRandomSP() const { return m_points.iRandomSP; }

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

	void			MainCharacterPacket();	// 내가 메인캐릭터라고 보내준다.

	void			ComputeAligin();
	void			ComputePoints();
	void			ComputeBattlePoints();

	void			PointChange(uint8_t type, int64_t amount, bool bAmount = false, bool bBroadcast = false
#ifdef __ENABLE_BLOCK_EXP__
		, bool bForceExp = false
#endif
	);

	void			PointsPacket();
	void			ApplyPoint(uint8_t bApplyType, int iVal);
#ifdef __NEWPET_SYSTEM__
	void			SendPetLevelUpEffect(int vid, int type, int value, int amount);
#endif		
	void			CheckMaximumPoints();	// HP, SP 등의 현재 값이 최대값 보다 높은지 검사하고 높다면 낮춘다.

	bool			Show(int32_t lMapIndex, int32_t x, int32_t y, int32_t z = LONG_MAX, bool bShowSpawnMotion = false);

	void			Sitdown(int is_ground);
	void			Standup();

#ifdef ENABLE_ANCIENT_PYRAMID
	void			SetRotation(float fRot, bool bForce = false);
#else
	void			SetRotation(float fRot);
#endif
	void			SetRotationToXY(int32_t x, int32_t y);
	float			GetRotation() const { return m_pointsInstant.fRot; }

	void			MotionPacketEncode(uint8_t motion, LPCHARACTER victim, struct packet_motion* packet);
	void			Motion(uint8_t motion, LPCHARACTER victim = nullptr);

	void			ChatPacket(uint8_t type, const char* format, ...);
	void			SendGreetMessage();

	void			ResetPoint(int iLv);

	void			SetBlockMode(uint8_t bFlag);
	void			SetBlockModeForce(uint8_t bFlag);
	bool			IsBlockMode(uint8_t bFlag) const { return (m_pointsInstant.bBlockMode & bFlag) ? true : false; }

	bool			IsPolymorphed() const { return m_dwPolymorphRace > 0; }
	bool			IsPolyMaintainStat() const { return m_bPolyMaintainStat; } // 이전 스텟을 유지하는 폴리모프.
	void			SetPolymorph(uint32_t dwRaceNum, bool bMaintainStat = false);
	uint32_t			GetPolymorphVnum() const { return m_dwPolymorphRace; }
	int				GetPolymorphPower() const;

	// FISING
	void			fishing();
	void			fishing_take();
	// END_OF_FISHING

	// MINING
	void			mining(LPCHARACTER chLoad);
	void			mining_cancel();
	void			mining_take();
	// END_OF_MINING

	void			ResetPlayTime(uint32_t dwTimeRemain = 0);

	void			CreateFly(uint8_t bType, LPCHARACTER pkVictim);

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
	bool			m_bDungeonTicketExtraMetin;
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
	bool			IsWalking() const { return m_bNowWalking || GetStamina() <= 0; }
	void			SetWalking(bool bWalkFlag) { m_bWalking = bWalkFlag; }
	void			SetNowWalking(bool bWalkFlag);
	void			ResetWalking() { SetNowWalking(m_bWalking); }

	bool			Goto(int32_t x, int32_t y);	// 바로 이동 시키지 않고 목표 위치로 BLENDING 시킨다.
	void			Stop();

	bool			CanMove() const;		// 이동할 수 있는가?

	void			SyncPacket();
	bool			Sync(int32_t x, int32_t y);	// 실제 이 메소드로 이동 한다 (각 종 조건에 의한 이동 불가가 없음)
	bool			Move(int32_t x, int32_t y);	// 조건을 검사하고 Sync 메소드를 통해 이동 한다.
	void			OnMove(bool bIsAttack = false);	// 움직일때 불린다. Move() 메소드 이외에서도 불릴 수 있다.
	uint32_t			GetMotionMode() const;
	float			GetMoveMotionSpeed() const;
	float			GetMoveSpeed() const;
	void			CalculateMoveDuration();
	void			SendMovePacket(uint8_t bFunc, uint8_t bArg, uint32_t x, uint32_t y, uint32_t dwDuration, uint32_t dwTime = 0, float iRot = -1.0f);
	uint32_t			GetCurrentMoveDuration() const { return m_dwMoveDuration; }
	uint32_t			GetWalkStartTime() const { return m_dwWalkStartTime; }
	uint32_t			GetLastMoveTime() const { return m_dwLastMoveTime; }
	uint32_t			GetLastAttackTime() const { return m_dwLastAttackTime; }

	void			SetLastAttacked(uint32_t time);	// 마지막으로 공격받은 시간 및 위치를 저장함

	bool			SetSyncOwner(LPCHARACTER ch, bool bRemoveFromList = true);
	bool			IsSyncOwner(LPCHARACTER ch) const;

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

	float			m_fSyncTime;
	LPCHARACTER		m_pkChrSyncOwner;
	CHARACTER_LIST	m_kLst_pkChrSyncOwned;	// 내가 SyncOwner인 자들

	PIXEL_POSITION	m_posDest;
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

	// Quickslot 관련
public:
	void			SyncQuickslot(uint8_t bType, uint8_t bOldPos, uint8_t bNewPos);
	bool			GetQuickslot(uint8_t pos, TQuickslot** ppSlot);
	bool			SetQuickslot(uint8_t pos, TQuickslot& rSlot);
	bool			DelQuickslot(uint8_t pos);
	bool			SwapQuickslot(uint8_t a, uint8_t b);
	void			ChainQuickslotItem(LPITEM pItem, uint8_t bType, uint8_t bOldPos);
#ifdef __ENABLE_NEW_OFFLINESHOP__
public:
	offlineshop::CShop* GetOfflineShop() { return m_pkOfflineShop; }
	void					SetOfflineShop(offlineshop::CShop* pkShop) { m_pkOfflineShop = pkShop; }

	offlineshop::CShop* GetOfflineShopGuest() const { return m_pkOfflineShopGuest; }
	void					SetOfflineShopGuest(offlineshop::CShop* pkShop) { m_pkOfflineShopGuest = pkShop; }

	offlineshop::CShopSafebox*
		GetShopSafebox() { return m_pkShopSafebox; }
	void					SetShopSafebox(offlineshop::CShopSafebox* pk);

	void					SetAuction(offlineshop::CAuction* pk) { m_pkAuction = pk; }
	void					SetAuctionGuest(offlineshop::CAuction* pk) { m_pkAuctionGuest = pk; }

	offlineshop::CAuction* GetAuction() { return m_pkAuction; }
	offlineshop::CAuction* GetAuctionGuest() const { return m_pkAuctionGuest; }


	//offlineshop-updated 05/08/19
	void					SetLookingOfflineshopOfferList(bool is) { m_bIsLookingOfflineshopOfferList = is; }
	bool					IsLookingOfflineshopOfferList() { return m_bIsLookingOfflineshopOfferList; }
	int						GetOfflineShopUseTime() const { return m_iOfflineShopUseTime; }
	void					SetOfflineShopUseTime() { m_iOfflineShopUseTime = thecore_pulse(); }

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

	// Affect loading이 끝난 상태인가?
	bool			IsLoadedAffect() const { return m_bIsLoadedAffect; }

	bool			IsGoodAffect(uint8_t bAffectType) const;

	void			RemoveGoodAffect();
	void			RemoveBadAffect();

	CAffect* FindAffect(uint32_t dwType, uint8_t bApply = APPLY_NONE) const;
	const std::list<CAffect*>& GetAffectContainer() const { return m_list_pkAffect; }
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

	bool			RequestToParty(LPCHARACTER leader);
	void			DenyToParty(LPCHARACTER member);
	void			AcceptToParty(LPCHARACTER member);

	/// 자신의 파티에 다른 character 를 초대한다.
	/**
	 * @param	pchInvitee 초대할 대상 character. 파티에 참여 가능한 상태이어야 한다.
	 *
	 * 양측 character 의 상태가 파티에 초대하고 초대받을 수 있는 상태가 아니라면 초대하는 캐릭터에게 해당하는 채팅 메세지를 전송한다.
	 */
	void			PartyInvite(LPCHARACTER pchInvitee);

	/// 초대했던 character 의 수락을 처리한다.
	/**
	 * @param	pchInvitee 파티에 참여할 character. 파티에 참여가능한 상태이어야 한다.
	 *
	 * pchInvitee 가 파티에 가입할 수 있는 상황이 아니라면 해당하는 채팅 메세지를 전송한다.
	 */
	void			PartyInviteAccept(LPCHARACTER pchInvitee);

	/// 초대했던 character 의 초대 거부를 처리한다.
	/**
	 * @param [in]	dwPID 초대 했던 character 의 PID
	 */
	void			PartyInviteDeny(uint32_t dwPID);

	bool			BuildUpdatePartyPacket(TPacketGCPartyUpdate& out);
	int				GetLeadershipSkillLevel() const;

	bool			CanSummon(int iLeaderShip);

	void			SetPartyRequestEvent(LPEVENT pkEvent) { m_pkPartyRequestEvent = pkEvent; }

protected:

	/// 파티에 가입한다.
	/**
	 * @param	pkLeader 가입할 파티의 리더
	 */
	void			PartyJoin(LPCHARACTER pkLeader);

	/**
	 * 파티 가입을 할 수 없을 경우의 에러코드.
	 * Error code 는 시간에 의존적인가에 따라 변경가능한(mutable) type 과 정적(static) type 으로 나뉜다.
	 * Error code 의 값이 PERR_SEPARATOR 보다 낮으면 변경가능한 type 이고 높으면 정적 type 이다.
	 */
	enum PartyJoinErrCode {
		PERR_NONE = 0,	///< 처리성공
		PERR_SERVER,			///< 서버문제로 파티관련 처리 불가
		PERR_DUNGEON,			///< 캐릭터가 던전에 있음
		PERR_OBSERVER,			///< 관전모드임
		PERR_LVBOUNDARY,		///< 상대 캐릭터와 레벨차이가 남
		PERR_LOWLEVEL,			///< 상대파티의 최고레벨보다 30레벨 낮음
		PERR_HILEVEL,			///< 상대파티의 최저레벨보다 30레벨 높음
		PERR_ALREADYJOIN,		///< 파티가입 대상 캐릭터가 이미 파티중
		PERR_PARTYISFULL,		///< 파티인원 제한 초과
		PERR_SEPARATOR,			///< Error type separator.
		PERR_DIFFEMPIRE,		///< 상대 캐릭터와 다른 제국임
		PERR_MAX				///< Error code 최고치. 이 앞에 Error code 를 추가한다.
	};

	/// 파티 가입이나 결성 가능한 조건을 검사한다.
	/**
	 * @param 	pchLeader 파티의 leader 이거나 초대한 character
	 * @param	pchGuest 초대받는 character
	 * @return	모든 PartyJoinErrCode 가 반환될 수 있다.
	 */
	static PartyJoinErrCode	IsPartyJoinableCondition(const LPCHARACTER pchLeader, const LPCHARACTER pchGuest);

	/// 파티 가입이나 결성 가능한 동적인 조건을 검사한다.
	/**
	 * @param 	pchLeader 파티의 leader 이거나 초대한 character
	 * @param	pchGuest 초대받는 character
	 * @return	mutable type 의 code 만 반환한다.
	 */
	static PartyJoinErrCode	IsPartyJoinableMutableCondition(const LPCHARACTER pchLeader, const LPCHARACTER pchGuest);

	LPPARTY			m_pkParty;
	uint32_t			m_dwLastDeadTime;
	LPEVENT			m_pkPartyRequestEvent;

	/**
	 * 파티초청 Event map.
	 * key: 초대받은 캐릭터의 PID
	 * value: event의 pointer
	 *
	 * 초대한 캐릭터들에 대한 event map.
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
	bool			CanHandleItem(bool bSkipRefineCheck = false, bool bSkipObserver = false); // 아이템 관련 행위를 할 수 있는가?

	bool			IsItemLoaded() const { return m_bItemLoaded; }
	void			SetItemLoaded() { m_bItemLoaded = true; }

	void			ClearItem();

#ifdef ENABLE_SORT_INVEN	
	void			EditMyInven();
	void			EditMyExtraInven();
#endif

#ifdef __HIGHLIGHT_SYSTEM__
	void			SetItem(TItemPos Cell, LPITEM item, bool isHighLight = false);
#else
	void			SetItem(TItemPos Cell, LPITEM item);
#endif
	LPITEM			GetItem(TItemPos Cell) const;
	LPITEM			GetInventoryItem(uint16_t wCell) const;
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

	void			SetWear(uint8_t bCell, LPITEM item);
	LPITEM			GetWear(uint8_t bCell) const;

	// MYSHOP_PRICE_LIST
	void			UseSilkBotary(void); 		/// 비단 보따리 아이템의 사용

	/// DB 캐시로 부터 받아온 가격정보 리스트를 유저에게 전송하고 보따리 아이템 사용을 처리한다.
	/**
	 * @param [in] p	가격정보 리스트 패킷
	 *
	 * 접속한 후 처음 비단 보따리 아이템 사용 시 UseSilkBotary 에서 DB 캐시로 가격정보 리스트를 요청하고
	 * 응답받은 시점에 이 함수에서 실제 비단보따리 사용을 처리한다.
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
	void			SetRefineNPC(LPCHARACTER ch);
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

	bool			GiveItem(LPCHARACTER victim, TItemPos Cell);
	bool			CanReceiveItem(LPCHARACTER from, LPITEM item) const;
	void			ReceiveItem(LPCHARACTER from, LPITEM item);
	bool			GiveItemFromSpecialItemGroup(uint32_t dwGroupNum, std::vector <uint32_t>& dwItemVnums,
		std::vector <uint32_t>& dwItemCounts, std::vector <LPITEM>& item_gets, int& count);

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

	// 현재 item을 착용할 수 있는 지 확인하고, 불가능 하다면 캐릭터에게 이유를 알려주는 함수
	bool			CanEquipNow(const LPITEM item, const TItemPos& srcCell = NPOS, const TItemPos& destCell = NPOS);

	// 착용중인 item을 벗을 수 있는 지 확인하고, 불가능 하다면 캐릭터에게 이유를 알려주는 함수
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
	bool			CanTakeInventoryItem(LPITEM item, TItemPos* pos);

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

	void			SendEquipment(LPCHARACTER ch);
	// End of Item

protected:

	void			SendMyShopPriceListCmd(uint32_t dwItemVnum, int64_t dwItemPrice);

	bool			m_bNoOpenedShop;	///< 이번 접속 후 개인상점을 연 적이 있는지의 여부(열었던 적이 없다면 true)

	bool			m_bItemLoaded;
	int				m_iRefineAdditionalCell;
	bool			m_bUnderRefine;
	uint32_t			m_dwRefineNPCVID;

public:
	////////////////////////////////////////////////////////////////////////////////////////
	// Money related

	int64_t				GetGold() const { return m_points.gold; }
	void			SetGold(int64_t gold) { m_points.gold = gold; }

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	int				Inven_Point() const { return m_points.envanter; }
	int				Inventory_Size() const { return 90 + (5 * Inven_Point()); }
	void			Set_Inventory_Point(int black) { m_points.envanter = black; }
	bool			Update_Inven();
#endif
	bool			DropGold(int64_t gold);

	int64_t				GetAllowedGold() const;
	void			GiveGold(int64_t iAmount);	// 파티가 있으면 파티 분배, 로그 등의 처리

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

	void			SetShopOwner(LPCHARACTER ch) { m_pkChrShopOwner = ch; }
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
	bool			ExchangeStart(LPCHARACTER victim);
	void			SetExchange(CExchange* pkExchange);
	CExchange* GetExchange() const { return m_pkExchange; }
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
	public:
		void SetWheelDestiny(std::shared_ptr<CWheelDestiny> pt) { pWheelDestiny = std::move(pt); };
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


	bool				Damage(LPCHARACTER pAttacker, int64_t dam, EDamageType type = DAMAGE_TYPE_NORMAL);
	bool				__Profile__Damage(LPCHARACTER pAttacker, int dam, EDamageType type = DAMAGE_TYPE_NORMAL);
	void				DeathPenalty(uint8_t bExpLossPercent);
	void				ReviveInvisible(int iDur);

	bool				Attack(LPCHARACTER pkVictim, uint8_t bType = 0);
	bool				IsAlive() const { return m_pointsInstant.position == POS_DEAD ? false : true; }
	bool				CanFight() const;

	bool				CanBeginFight() const;
	void				BeginFight(LPCHARACTER pkVictim); // pkVictimr과 싸우기 시작한다. (강제적임, 시작할 수 있나 체크하려면 CanBeginFight을 사용)

	bool				CounterAttack(LPCHARACTER pkChr); // 반격하기 (몬스터만 사용)

	bool				IsStun() const;
	void				Stun();
	bool				IsDead() const;
	void				Dead(LPCHARACTER pkKiller = nullptr, bool bImmediateDead = false);
#ifdef __NEWPET_SYSTEM__
	//int GetBeltCount() const;//#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
	void				SetImmortal(int st) { m_stImmortalSt = st; };
	bool				IsImmortal() { return 1 == m_stImmortalSt; };
	void				SetNewPetSkillCD(int s, uint32_t time) { m_newpetskillcd[s] = time; };
	uint32_t				GetNewPetSkillCD(int s) { return m_newpetskillcd[s]; };
#endif
	void				Reward(bool bItemDrop);
	void				RewardGold(LPCHARACTER pkAttacker);

	bool				Shoot(uint8_t bType);
	void				FlyTarget(uint32_t dwTargetVID, int32_t x, int32_t y, uint8_t bHeader);

	void				ForgetMyAttacker();
	void				AggregateMonster();
#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
	void AggregateMonsterPlus();
#endif
	void				AttractRanger();
	void				PullMonster();

	int					GetArrowAndBow(LPITEM* ppkBow, LPITEM* ppkArrow, int iArrowCount = 1);
#ifdef ENABLE_RANKING

	//void SendLeaderboardData();
	//void SendLeaderboardNews();
	//static void LeaderboardLoop();
#endif
	void				UseArrow(LPITEM pkArrow, uint32_t dwArrowCount);
#ifdef LEADERBOARD_RAZOR93

	void SendLeaderboardData();
	void SendLeaderboardDataSkillMob(LPCHARACTER viewer);
	void SendLeaderboardDataGuild();
	static std::vector<LeaderboardEntry> FetchTop10SkillMob();
	static void CheckLeaderboardSkillMobChanges();
#endif
	void				AttackedByPoison(LPCHARACTER pkAttacker);
	void				RemovePoison();
#ifdef ENABLE_WOLFMAN_CHARACTER
	void				AttackedByBleeding(LPCHARACTER pkAttacker);
	void				RemoveBleeding();
#endif
	void				AttackedByFire(LPCHARACTER pkAttacker, int amount, int count);
	void				RemoveFire();

	uint8_t GetAlignmentGrade() const;

	void ClearAlignmentBonus();

	void ApplyAlignmentBonus();

	void				UpdateAlignment(uint32_t iAmount);
	uint32_t					GetAlignment() const;
	//int GetBeltCount() const;
#ifdef ENABLE_FAKE_SHOP_HEADER
	int GetMountCount() const;
	void UpdateMountInventoryCountOverhead(LPCHARACTER viewer);
	void UpdateMountCountOverheadToViewers();
	//void UpdateMountCountOverhead(LPCHARACTER ch);
#ifdef DISABLE_CORE_PULSE_RAZOR93

	bool IsNextMountPulse() const;

	void UpdateMountPulse();
#endif
#endif
	//선악치 얻기
	uint32_t					GetRealAlignment() const;
	//void				ShowAlignment(bool bShow);

	void				SetKillerMode(bool bOn);
	bool				IsKillerMode() const;
	void				UpdateKillerMode();

	uint8_t				GetPKMode() const;
	void				SetPKMode(uint8_t bPKMode);

	void				ItemDropPenalty(LPCHARACTER pkKiller);

	void				UpdateAggrPoint(LPCHARACTER ch, EDamageType type, int dam);

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
	void				UpdateAggrPointEx(LPCHARACTER ch, EDamageType type, int dam, TBattleInfo& info);
	void				ChangeVictimByAggro(int iNewAggro, LPCHARACTER pNewVictim);

	uint32_t				m_dwFlyTargetID;
	std::vector<uint32_t>	m_vec_dwFlyTargets;
	TDamageMap			m_map_kDamage;	// 어떤 캐릭터가 나에게 얼마만큼의 데미지를 주었는가?
	//		AttackLog			m_kAttackLog;
	uint32_t				m_dwKillerPID;

	uint32_t					m_iAlignment;
	uint32_t					m_iRealAlignment;
	int					m_iKillerModePulse;
	uint8_t				m_bPKMode;

	// Aggro
	uint32_t				m_dwLastVictimSetTime;
	int					m_iMaxAggro;
	// End of Battle

	// Stone
public:
	void				SetStone(LPCHARACTER pkChrStone);
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
	void ClearStone(LPCHARACTER pkKiller = nullptr);
	void RegisterDamageForExp(LPCHARACTER pkAttacker, int iDamage = 1);
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
	LPCHARACTER			m_pkChrStone;		// 나를 스폰한 돌
	CHARACTER_SET		m_set_pkChrSpawnedBy;	// 내가 스폰한 놈들
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
	bool				UseSkill(uint32_t dwVnum, LPCHARACTER pkVictim, bool bUseGrandMaster = true);
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
	int					ComputeGyeongGongSkill(uint32_t dwVnum, LPCHARACTER pkVictim, uint8_t bSkillLevel = 0);
#endif
	int					ComputeSkill(uint32_t dwVnum, LPCHARACTER pkVictim, uint8_t bSkillLevel = 0);
#ifdef GROUP_BUFF
	int					ComputeSkillParty(uint32_t dwVnum, LPCHARACTER pkVictim, uint8_t bSkillLevel = 0);
#endif
	int					ComputeSkillAtPosition(uint32_t dwVnum, const PIXEL_POSITION& posTarget, uint8_t bSkillLevel = 0);
	void				ComputeSkillPoints();

	void				SetSkillGroup(uint8_t bSkillGroup);
	uint8_t				GetSkillGroup() const { return m_points.skill_group; }

	int					ComputeCooltime(int time);

	void				GiveRandomSkillBook();

	void				DisableCooltime();
	bool				LearnSkillByBook(uint32_t dwSkillVnum, uint8_t bProb = 0);
	bool				LearnGrandMasterSkill(uint32_t dwSkillVnum);

private:
	bool				m_bDisableCooltime;
	uint32_t				m_dwLastSkillTime;	///< 마지막으로 skill 을 쓴 시간(millisecond).
	// End of Skill
#ifdef DISABLE_CORE_PULSE_RAZOR93

	int m_mountPulse = 0;//razor93
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
	int					GetChainLightningIndex() const { return m_iChainLightingIndex; }
	void				IncChainLightningIndex() { ++m_iChainLightingIndex; }
	void				AddChainLightningExcept(LPCHARACTER ch) { m_setExceptChainLighting.insert(ch); }
	void				ResetChainLightningIndex() { m_iChainLightingIndex = 0; m_setExceptChainLighting.clear(); }
	int					GetChainLightningMaxCount() const;
	const CHARACTER_SET& GetChainLightingExcept() const { return m_setExceptChainLighting; }

private:
	int					m_iChainLightingIndex;
	CHARACTER_SET m_setExceptChainLighting;

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
	LPCHARACTER		GetVictim() const;	// 공격할 대상 리턴
	void			SetVictim(LPCHARACTER pkVictim);
	LPCHARACTER		GetNearestVictim(LPCHARACTER pkChr);
	LPCHARACTER		GetProtege() const;	// 보호해야 할 대상 리턴
	virtual void			StateBattle();
	virtual void			StateIdle();

protected:
	void				__StateIdle_Monster();
	void				__StateIdle_NPC();

public:
	bool			Follow(LPCHARACTER pkChr, float fMinimumDistance = 150.0f);
	bool			Return();
	bool			IsGuardNPC() const;
	bool			IsChangeAttackPosition(LPCHARACTER target) const;
	void			ResetChangeAttackPositionTime() { m_dwLastChangeAttackPositionTime = get_dword_time() - AI_CHANGE_ATTACK_POISITION_TIME_NEAR; }
	void			SetChangeAttackPositionTime() { m_dwLastChangeAttackPositionTime = get_dword_time(); }

	bool			OnIdle();

	void			OnAttack(LPCHARACTER pkChrAttacker);
	void			OnClick(LPCHARACTER pkChrCauser);
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
	LPCHARACTER				m_pkChrTarget;		// 내 타겟
	CHARACTER_SET	m_set_pkChrTargetedBy;	// 나를 타겟으로 가지고 있는 사람들

public:
	void				SetTarget(LPCHARACTER pkChrTarget);
	void				BroadcastTargetPacket();
	void				ClearTarget();
	void				CheckTarget();
	LPCHARACTER			GetTarget() const { return m_pkChrTarget; }

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

	/// 창고 열기 요청
	/**
	 * @param [in]	pszPassword 1자 이상 6자 이하의 창고 비밀번호
	 *
	 * DB 에 창고열기를 요청한다.
	 * 창고는 중복으로 열지 못하며, 최근 창고를 닫은 시간으로 부터 10초 이내에는 열 지 못한다.
	 */
	void				ReqSafeboxLoad(const char* pszPassword);

	/// 창고 열기 요청의 취소
	/**
	 * ReqSafeboxLoad 를 호출하고 CloseSafebox 하지 않았을 때 이 함수를 호출하면 창고를 열 수 있다.
	 * 창고열기의 요청이 DB 서버에서 실패응답을 받았을 경우 이 함수를 사용해서 요청을 할 수 있게 해준다.
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
	bool				m_bOpeningSafebox;	///< 창고가 열기 요청 중이거나 열려있는가 여부, true 일 경우 열기요청이거나 열려있음.

	CMountInventory* m_pkMountInventory;
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

	LPCHARACTER			GetHorse() const { return m_chHorse; }	 // 현재 소환중인 말
	LPCHARACTER			GetRider() const; // rider on horse
	void				SetRider(LPCHARACTER ch);

	bool				IsRiding() const;
#ifdef __ATTR_TRANSFER_SYSTEM__
public:
	LPITEM* GetAttrTransferItem();
	bool IsAttrTransferOpen() const;
	void SetAttrTransferNpc(LPCHARACTER npc);
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

	void 				MountSummon(LPITEM mountItem);
	void 				MountUnsummon(LPITEM mountItem);
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
	LPCHARACTER			m_chHorse;
	LPCHARACTER			m_chRider;

	uint32_t				m_dwMountVnum;
	uint32_t				m_dwMountTime;

	uint8_t				m_bSendHorseLevel;
	uint8_t				m_bSendHorseHealthGrade;
	uint8_t				m_bSendHorseStaminaGrade;

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
	uint8_t				GetEmpire() const { return m_bEmpire; }

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
	bool				CannotMoveByAffect() const;	// 특정 효과에 의해 움직일 수 없는 상태인가?
	bool				IsImmune(uint32_t dwImmuneFlag);
	void				SetImmuneFlag(uint32_t dw) { m_pointsInstant.dwImmuneFlag = dw; }
	uint32_t			GetImmuneFlag() const { return m_pointsInstant.dwImmuneFlag; }

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

	void				SetQuestItemPtr(LPITEM item);
	void				ClearQuestItemPtr();
	LPITEM				GetQuestItemPtr() const;

	void				SetQuestBy(uint32_t dwQuestVnum) { m_dwQuestByVnum = dwQuestVnum; }
	uint32_t				GetQuestBy() const { return m_dwQuestByVnum; }

	int					GetQuestFlag(const std::string& flag) const;
	void				SetQuestFlag(const std::string& flag, int value);

	void				ConfirmWithMsg(const char* szMsg, int iTimeout, uint32_t dwRequestPID);

private:
	uint32_t				m_dwQuestNPCVID;
	uint32_t				m_dwQuestByVnum;
	LPITEM				m_pQuestItem;

	// Events
public:
	bool				StartStateMachine(int iPulse = 1);
	void				StopStateMachine();
	void				UpdateStateMachine(uint32_t dwPulse);
	void				SetNextStatePulse(int iPulseNext);

	// 캐릭터 인스턴스 업데이트 함수. 기존엔 이상한 상속구조로 CFSM::Update 함수를 호출하거나 UpdateStateMachine 함수를 사용했는데, 별개의 업데이트 함수 추가함.
	void				UpdateCharacter(uint32_t dwPulse);

protected:
	CStateTemplate<CHARACTER>	m_stateBattle;
	CStateTemplate<CHARACTER>	m_stateIdle;
	uint32_t				m_dwNextStatePulse;

	// Marriage
public:
	LPCHARACTER			GetMarryPartner() const;
	void				SetMarryPartner(LPCHARACTER ch);
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

	LPEVENT				m_pkDeadEvent;
	LPEVENT				m_pkStunEvent;
	LPEVENT				m_pkSaveEvent;
	LPEVENT				m_pkRecoveryEvent;
	LPEVENT				m_pkTimedEvent;
	LPEVENT				m_pkFishingEvent;
	LPEVENT				m_pkAffectEvent;
	LPEVENT				m_pkPoisonEvent;
	LPEVENT				GetTimedEvent() const { return m_pkTimedEvent; }
	LPEVENT&			GetTimedEventRef() { return m_pkTimedEvent; }
	LPEVENT				GetFishingEvent() const { return m_pkFishingEvent; }
	LPEVENT&			GetFishingEventRef() { return m_pkFishingEvent; }
#ifdef ENABLE_WOLFMAN_CHARACTER
	LPEVENT				m_pkBleedingEvent;
#endif
	LPEVENT				m_pkFireEvent;
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

	bool				m_bHasPoisoned;
#ifdef ENABLE_WOLFMAN_CHARACTER
	bool				m_bHasBled;
#endif

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
	// static const uint32_t		msc_dwDefaultChangeItemAttrCycle;	///< 디폴트 아이템 속성변경 가능 주기
	static const char		msc_szLastChangeItemAttrFlag[];		///< 최근 아이템 속성을 변경한 시간의 Quest Flag 이름
	// static const char		msc_szChangeItemAttrCycleFlag[];		///< 아이템 속성병경 가능 주기의 Quest Flag 이름
	// END_OF_CHANGE_ITEM_ATTRIBUTES

	// PC_BANG_ITEM_ADD
private:
	bool m_isinPCBang;

public:
	bool SetPCBang(bool flag) { m_isinPCBang = flag; return m_isinPCBang; }
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

private:
	void SendDamagePacket(LPCHARACTER pAttacker, int Damage, uint8_t DamageFlag);

	// ARENA
private:
	CArena* m_pArena;
	bool m_ArenaObserver;
	int m_nPotionLimit;

public:
	void 	SetArena(CArena* pArena) { m_pArena = pArena; }
	void	SetArenaObserverMode(bool flag) { m_ArenaObserver = flag; }

	CArena* GetArena() const { return m_pArena; }
	bool	GetArenaObserverMode() const { return m_ArenaObserver; }

	void	SetPotionLimit(int count) { m_nPotionLimit = count; }
	int		GetPotionLimit() const { return m_nPotionLimit; }
	// END_ARENA

		//PREVENT_TRADE_WINDOW
public:
	bool	IsOpenSafebox() const { return m_isOpenSafebox ? true : false; }
	void 	SetOpenSafebox(bool b) { m_isOpenSafebox = b; }

	int		GetSafeboxLoadTime() const { return m_iSafeboxLoadTime; }
	void	SetSafeboxLoadTime() { m_iSafeboxLoadTime = thecore_pulse(); }
	//END_PREVENT_TRADE_WINDOW
private:
	bool	m_isOpenSafebox;

public:
	int		GetSkillPowerByLevel(int level, bool bMob = false) const;

	//PREVENT_REFINE_HACK
	int		GetRefineTime() const { return m_iRefineTime; }
	void	SetRefineTime() { m_iRefineTime = thecore_pulse(); }
	int		m_iRefineTime;
	//END_PREVENT_REFINE_HACK

	//RESTRICT_USE_SEED_OR_MOONBOTTLE
	int 	GetUseSeedOrMoonBottleTime() const { return m_iSeedTime; }
	void  	SetUseSeedOrMoonBottleTime() { m_iSeedTime = thecore_pulse(); }
	int 	m_iSeedTime;
	//END_RESTRICT_USE_SEED_OR_MOONBOTTLE

	//PREVENT_PORTAL_AFTER_EXCHANGE
	int		GetExchangeTime() const { return m_iExchangeTime; }
	void	SetExchangeTime() { m_iExchangeTime = thecore_pulse(); }
	int		m_iExchangeTime;
	//END_PREVENT_PORTAL_AFTER_EXCHANGE

	int 	m_iMyShopTime;
	int		GetMyShopTime() const { return m_iMyShopTime; }
	void	SetMyShopTime() { m_iMyShopTime = thecore_pulse(); }

	// Hack 방지를 위한 체크.
	bool	IsHack(bool bSendMsg = true, bool bCheckShopOwner = true, int limittime = g_nPortalLimitTime);

	void Say(const std::string& s);

public:
	bool ItemProcess_Polymorph(LPITEM item);

	// by mhh
	LPITEM* GetCubeItem();
	bool IsCubeOpen() const;
	void SetCubeNpc(LPCHARACTER npc);
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
	const std::string GetNewName() const { return this->m_strNewName; }
	void SetNewName(const std::string name) { this->m_strNewName = name; }

public:
	void GoHome();

private:
	std::set<uint32_t>	m_known_guild;

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
	void BuffOnAttr_ValueChange(uint8_t bType, uint8_t bOldValue, uint8_t bNewValue);
	void BuffOnAttr_ClearAll();

	typedef std::map <uint8_t, CBuffOnAttributes*> TMapBuffOnAttrs;
	TMapBuffOnAttrs m_map_buff_on_attrs;
	// 무적 : 원활한 테스트를 위하여.
public:
	void SetArmada() { cannot_dead = true; }
	void ResetArmada() { cannot_dead = false; }
private:
	bool cannot_dead;
#ifdef __PET_SYSTEM__
private:
	bool m_bIsPet;
public:
	void SetPet() { m_bIsPet = true; }
	bool IsPet() { return m_bIsPet; }
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
private:
	bool m_bIsMount;
public:
	void SetMount() { m_bIsMount = true; }
	bool IsMount() { return m_bIsMount; }
#endif

#ifdef __NEWPET_SYSTEM__
private:
	bool m_bIsNewPet;
	int m_eggvid;
public:
	void SetNewPet() { m_bIsNewPet = true; }
	bool IsNewPet() const { return m_bIsNewPet ? true : false; }
	void SetEggVid(int vid) { m_eggvid = vid; }
	int GetEggVid() { return m_eggvid; }

#endif

	//최종 데미지 보정.
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
	//용혼석

	// 캐릭터의 affect, quest가 load 되기 전에 DragonSoul_Initialize를 호출하면 안된다.
	// affect가 가장 마지막에 로드되어 LoadAffect에서 호출함.
	void	DragonSoul_Initialize();

	int		DragonSoul_GetActiveDeck() const;
	bool	DragonSoul_IsDeckActivated() const;
	bool	DragonSoul_ActivateDeck(int deck_idx);

	void	DragonSoul_DeactivateAll();
	// 반드시 ClearItem 전에 불러야 한다.
	// 왜냐하면....
	// 용혼석 하나 하나를 deactivate할 때마다 덱에 active인 용혼석이 있는지 확인하고,
	// active인 용혼석이 하나도 없다면, 캐릭터의 용혼석 affect와, 활성 상태를 제거한다.
	//
	// 하지만 ClearItem 시, 캐릭터가 착용하고 있는 모든 아이템을 unequip하는 바람에,
	// 용혼석 Affect가 제거되고, 결국 로그인 시, 용혼석이 활성화되지 않는다.
	// (Unequip할 때에는 로그아웃 상태인지, 아닌지 알 수 없다.)
	// 용혼석만 deactivate시키고 캐릭터의 용혼석 덱 활성 상태는 건드리지 않는다.
	void	DragonSoul_CleanUp();
	// 용혼석 강화창
public:
	bool		DragonSoul_RefineWindow_Open(LPENTITY pEntity);
	bool		DragonSoul_RefineWindow_Close();
	LPENTITY	DragonSoul_RefineWindow_GetOpener() { return  m_pointsInstant.m_pDragonSoulRefineWindowOpener; }
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
	//독일 선물 기능 패킷 임시 저장
private:
	unsigned int itemAward_vnum;
	char		 itemAward_cmd[20];
	//bool		 itemAward_flag;
public:
	unsigned int GetItemAward_vnum() { return itemAward_vnum; }
	char* GetItemAward_cmd() { return itemAward_cmd; }
	//bool		 GetItemAward_flag() { return itemAward_flag; }
	void		 SetItemAward_vnum(unsigned int vnum) { itemAward_vnum = vnum; }
	void		 SetItemAward_cmd(char* cmd) { strcpy(itemAward_cmd, cmd); }
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
	// SyncPosition을 악용하여 타유저를 이상한 곳으로 보내는 핵 방어하기 위하여,
	// SyncPosition이 일어날 때를 기록.
	timeval			m_tvLastSyncTime;
	int			m_iSyncHackCount;
public:
	void			SetLastSyncTime(const timeval& tv) { memcpy(&m_tvLastSyncTime, &tv, sizeof(timeval)); }
	const timeval& GetLastSyncTime() { return m_tvLastSyncTime; }
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
	bool	CleanAcceAttr(LPITEM pkItem, LPITEM pkTarget);
	LPITEM* GetAcceMaterials() { return m_pointsInstant.pAcceMaterials; }
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
	bool IsBodyCostumeHidden() const { return m_bHideBodyCostume; };

	void SetHairCostumeHidden(bool hidden, bool pass = false);
	bool IsHairCostumeHidden() const { return m_bHideHairCostume; };
#ifdef ENABLE_FREE_PASS_RAZOR93

	bool HasBattlePassBoost(uint8_t bBattlePassId);
	uint32_t GetBattlePassAdjustedTotal(uint32_t dwMissionID, uint32_t dwBattlePassID, uint32_t dwBaseTotal);
	void ApplyBattlePassBoostRecalc(uint8_t bBattlePassId);

	void EnsureFreeBattlePassActive();

#endif

#ifdef ENABLE_ACCE_SYSTEM
	void SetAcceCostumeHidden(bool hidden, bool pass = false);
	bool IsAcceCostumeHidden() const { return m_bHideAcceCostume; };
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	void SetWeaponCostumeHidden(bool hidden, bool pass = false);
	bool IsWeaponCostumeHidden() const { return m_bHideWeaponCostume; };
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
	struct Gaya_Shop_Values
	{
		int		value_1;
		int		value_2;
		int 	value_3;
		int 	value_4;
		int 	value_5;
		int 	value_6;

		bool operator == (const Gaya_Shop_Values& b)
		{
			return (this->value_1 == b.value_1) && (this->value_2 == b.value_2) &&
				(this->value_3 == b.value_3) && (this->value_4 == b.value_4) &&
				(this->value_5 == b.value_5) && (this->value_6 == b.value_6);
		}
	};

	struct Gaya_Load_Values
	{
		uint32_t	items;
		uint32_t	gaya;
		uint32_t	count;
		uint32_t	glimmerstone;
		uint32_t	gaya_expansion;
		uint32_t	gaya_refresh;
		uint32_t	glimmerstone_count;
		uint32_t	gaya_expansion_count;
		uint32_t	gaya_refresh_count;
		uint32_t	grade_stone;
		uint32_t	give_gaya;
		uint32_t	prob_gaya;
		uint32_t	cost_gaya_yang;
	};

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
	int  GetGayaState(const std::string& state) const;
	void SetGayaState(const std::string& state, int szValue);
	void StartCheckTimeMarket();

public:
	std::vector<Gaya_Shop_Values> info_items;
	std::vector<Gaya_Shop_Values> info_slots;
	std::vector<Gaya_Load_Values> load_gaya_items;
	Gaya_Load_Values	load_gaya_values;
	LPEVENT	GayaUpdateTime;
#endif



#ifdef ENABLE_SOUL_SYSTEM
public:
	bool 		DoRefineItemSoul(LPITEM item);
	int 		GetSoulItemDamage(LPCHARACTER pkVictim, int iDamage, uint8_t bSoulType);
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
	void ChatPacketNew(uint8_t type, uint32_t idx, const char* format, ...);
#endif
#ifdef ENABLE_NEW_FISHING_SYSTEM
public:
	LPEVENT m_pkFishingNewEvent;

	void fishing_new_start();
	void fishing_new_stop();
	void fishing_new_catch();
	void fishing_new_catch_failed();
	void fishing_catch_decision(uint32_t itemVnum);
	void SetFishCatch(int i) { m_bFishCatch = i; }
	uint8_t GetFishCatch() { return m_bFishCatch; }
	void SetLastCatchTime(uint32_t i) { m_dwLastCatch = i; }
	int GetLastCatchTime() { return m_dwLastCatch; }
	void SetFishCatchFailed(int i) { m_dwCatchFailed = i; }
	uint8_t GetFishCatchFailed() { return m_dwCatchFailed; }

private:
	uint8_t m_bFishCatch;
	uint32_t m_dwCatchFailed;
	int m_dwLastCatch;
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
	int32_t	GetLastDSREfine() const { return m_iLastDSRefine; }
	void	SetLastDSREfine() { m_iLastDSRefine = get_global_time() + 3; }

protected:
	int32_t	m_iLastUnlock, m_iLastDSRefine;
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
	DynamicCharacterPtr ch;
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





