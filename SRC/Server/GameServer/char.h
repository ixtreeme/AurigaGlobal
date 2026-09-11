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
	// Entity
	//////////////////////////////////////////////////////////////////////////////////

public:
	int m_lastBeltMountCount;

#ifdef ENABLE_VOTE4BUFF
public:
	long long	GetVoteCoin();
	void		SetVoteCoin(long long amount);
#endif

public:







	virtual void			EndStateEmpty() {}

	void				RestartAtSamePos();

protected:
	//////////////////////////////////////////////////////////////////////////////////

public:
	CHARACTER();
	~CHARACTER() override;

	void			Create(const char* c_pszName, uint32_t vid, bool isPC);
	void			Destroy();


protected:
	void			Initialize();

	//////////////////////////////////////////////////////////////////////////////////
	// Basic Points
#ifdef __SEND_TARGET_INFO__
private:
	uint32_t			dwLastTargetInfoPulse;

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
	void CHARACTER::UpdateMountCountOverhead(LPCHARACTER ch, bool force)
#endif


	void			SetPlayerProto(const TPlayerTable* table);

	void			SetProto(const CMob* c_pkMob);
	uint16_t			GetRaceNum() const;

	void			Save();		// DelayedSave

#ifdef ENABLE_MULTI_NAMES
	const char* GetName(uint8_t lang = DEFAULT_LANGUAGE) const;
#else
	const char* GetName() const;
#endif

	uint32_t		GetLegacyVID() const;
	uint32_t		GetPacketVID() const;
	// char.h (public)

	void			SetCharType(uint8_t bType) { m_bCharType = bType; }
	//void SetName(const char* name) { m_stName = (name ? name : ""); }
	void			SetName(const std::string& name) { m_stName = name; }

	void			SetRace(uint8_t race);
	bool			ChangeSex();

	uint32_t			GetAID() const;

	uint8_t			GetCharType() const;

	bool			IsPC() const { return GetDesc() ? true : false; }
	bool			IsNPC()	const { return m_bCharType != CHAR_TYPE_PC; }
	bool			IsMonster()	const { return m_bCharType == CHAR_TYPE_MONSTER; }
	bool			IsStone() const { return m_bCharType == CHAR_TYPE_STONE; }
	bool			IsDoor() const { return m_bCharType == CHAR_TYPE_DOOR; }
	bool			IsBuilding() const { return m_bCharType == CHAR_TYPE_BUILDING; }
	//		bool			IsPet() const		{ return m_bCharType == CHAR_TYPE_PET; }
#ifdef ENABLE_EVENT_MANAGER
	// DUNGEON_TICKET_LOOT_EVENT extra metin marker
#endif

	uint32_t			GetLastShoutPulse() const;
	void			SetLastShoutPulse(uint32_t pulse);

	BOOL 			IsGM() const;

#ifdef __ENABLE_BLOCK_EXP__
	bool			Block_Exp;
#endif
	uint32_t			GetNextExp() const;

	// ���� ���� ���� ����� �����Ѵ�.

	void			SetPosition(int pos);
	bool			IsPosition(int pos) const;
	int				GetPosition() const;


	int64_t				GetSP() const;






	void			SetRealPoint(uint8_t idx, int64_t val);
	int64_t			GetRealPoint(uint8_t idx) const;

	void			SetPoint(uint8_t idx, int64_t val);

	int64_t			GetPoint(uint8_t idx) const;


	const TMobTable& GetMobTable() const;
	uint32_t				GetMobDropItemVnum() const;

	// NEWAI




	// NEWAI END

	uint32_t			GetSummonVnum() const;

	uint32_t			GetPolymorphItemVnum() const;

	void			ComputePoints();

	void			PointChange(uint8_t type, int64_t amount, bool bAmount = false, bool bBroadcast = false
#ifdef __ENABLE_BLOCK_EXP__
		, bool bForceExp = false
#endif
	);

	void			CheckMaximumPoints();	// HP, SP ���� ���� ���� �ִ밪 ���� ������ �˻��ϰ� ���ٸ� �����.


	void			Sitdown(int is_ground);
	void			Standup();

#ifdef ENABLE_ANCIENT_PYRAMID
	void			SetRotation(float fRot, bool bForce = false);
#else
	void			SetRotation(float fRot);
#endif
	float			GetRotation() const;


	void			SendGreetMessage();


	void			SetBlockMode(uint8_t bFlag);
	uint8_t			GetBlockMode() const;
	bool			IsBlockMode(uint8_t bFlag) const;


	// FISING
	void			fishing();
	void			fishing_take();
	// END_OF_FISHING

	// MINING
	// END_OF_MINING

	void			ResetPlayTime(uint32_t dwTimeRemain = 0);



	void			ResetMountCounter();
	uint8_t			IncreaseMountCounter();
	uint8_t			GetMountCounter() const;

protected:
	uint32_t			m_dwPolymorphRace;
	bool			m_bPolyMaintainStat;
	uint32_t			m_dwLoginPlayTime;
	uint32_t			m_dwPlayerID;
	std::string		m_stName;
	uint8_t			m_bCharType;
#ifdef ENABLE_EVENT_MANAGER
#endif


	int				m_iMoveCount;
	uint32_t			m_dwPlayStartTime;
	uint8_t			m_bAddChrState;

	//////////////////////////////////////////////////////////////////////////////////
	// Move & Synchronize Positions
	//////////////////////////////////////////////////////////////////////////////////
public:
	 
	void SetFakePlayer(bool b) { m_bFakePlayer = b; }
	bool IsFakePlayer() const { return m_bFakePlayer; }

	 
	//void			SetCharType(uint8_t bType) { m_bCharType = bType; }
private:
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
	void			SetNowWalking(bool bWalkFlag);



	bool			Sync(int32_t x, int32_t y);	// 
	bool			Move(int32_t x, int32_t y);	// 
	void			OnMove(bool bIsAttack = false);
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
	uint32_t			GetLastMoveTime() const { return m_dwLastMoveTime; }
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





#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
	bool			SwitchChannel(int32_t newAddr, uint16_t newPort);
	bool			StartChannelSwitch(int32_t newAddr, uint16_t newPort);
#endif



	void			StopStaminaConsume();
	bool			IsStaminaHalfConsume() const;

	void			ResetStopTime();
	uint32_t			GetStopTime() const;

protected:




	uint32_t			m_dwMoveStartTime;
	uint32_t			m_dwMoveDuration;

	uint32_t			m_dwLastMoveTime;

	uint32_t			m_dwStopTime;

	bool			m_bNowWalking;
	bool			m_bStaminaConsume;
	// End

	// Quickslot 
public:
	void			SyncQuickslot(uint8_t bType, uint8_t bOldPos, uint8_t bNewPos);
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

	////////////////////////////////////////////////////////////////////////////////////////
	// Affect
public:

	bool			UpdateAffect();	// called from EVENT

	void			LoadAffect(uint32_t dwCount, TPacketAffectElement* pElements);





	CAffect* FindAffect(uint32_t dwType, uint8_t bApply = APPLY_NONE) const;
	TAffectFlag GetAffectFlags() const;

#ifdef ENABLE_SKILLS_BUFF_ALTERNATIVE
public:
	void						SaveAffectSkills(uint32_t dwType, uint8_t bApplyOn, int32_t lApplyValue, uint32_t dwFlag, int32_t lDuration, int32_t lSPCost);
	void						LoadAffectSkills();

#endif

public:
	// PARTY_JOIN_BUG_FIX
	void			SetParty(LPPARTY pkParty);
	LPPARTY			GetParty() const { return m_pkParty; }

	bool			RequestToParty(entt::entity leader);
	void			DenyToParty(entt::entity member);
	void			AcceptToParty(entt::entity member);
	void			PartyInvite(entt::entity invitee);
	void			PartyInviteAccept(entt::entity invitee);
	void			PartyInviteDeny(uint32_t dwPID);
	void			SetPartyRequestEvent(LPEVENT pkEvent) { m_pkPartyRequestEvent = pkEvent; }

protected:

	
	void			PartyJoin(entt::entity leader);


	enum PartyJoinErrCode {
		PERR_NONE = 0,
		PERR_SERVER,	
		PERR_DUNGEON,	
		PERR_OBSERVER,	
		PERR_LVBOUNDARY,
		PERR_LOWLEVEL,	
		PERR_HILEVEL,	
		PERR_ALREADYJOIN,
		PERR_PARTYISFULL,
		PERR_SEPARATOR,	
		PERR_DIFFEMPIRE,
		PERR_MAX		
	};

	
	static PartyJoinErrCode	IsPartyJoinableCondition(entt::entity leader, entt::entity guest);
	static PartyJoinErrCode	IsPartyJoinableMutableCondition(entt::entity leader, entt::entity guest);

	LPPARTY			m_pkParty;
	LPEVENT			m_pkPartyRequestEvent;
	typedef std::map< uint32_t, LPEVENT >	EventMap;
	EventMap		m_PartyInviteEventMap;

	// END_OF_PARTY_JOIN_BUG_FIX

	////////////////////////////////////////////////////////////////////////////////////////
	// Dungeon
public:
protected:
	int			m_iEventAttr;

	////////////////////////////////////////////////////////////////////////////////////////
	// Guild
public:
	void			SetGuild(CGuild* pGuild);
	CGuild* GetGuild() const { return m_pGuild; }


protected:
	CGuild* m_pGuild;

	////////////////////////////////////////////////////////////////////////////////////////
	// Item related
public:
	bool			CanHandleItem(bool bSkipRefineCheck = false, bool bSkipObserver = false); 

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
	void			SetNextSortExtraInventoryPulse(int pulse) { m_sortExtraInventoryPulse = pulse; }
	int				GetSortExtraInventoryPulse() { return m_sortExtraInventoryPulse; }
	int				m_sortExtraInventoryPulse;
#endif
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	int		ExtraInventoryMaxSlots(int iArg1, bool bAuto = false) const;
	void	UnlockExtraInventory(uint8_t category);
#endif
	bool			IsEmptyItemGrid(TItemPos Cell, uint8_t size, int iExceptionCell = -1) const;


	// MYSHOP_PRICE_LIST

	void			UseSilkBotaryReal(const TPacketMyshopPricelistHeader* p);
	// END_OF_MYSHOP_PRICE_LIST


	// ADD_REFINE_BUILDING
	void			SetRefineNPC(entt::entity character);
	// END_OF_ADD_REFINE_BUILDING

	bool			DropItem(TItemPos Cell,
#ifdef ENABLE_NEW_STACK_LIMIT
		int
#else
		uint8_t
#endif
		bCount = 0);
	bool			DestroyItem(TItemPos Cell);

	//	void			PotionPacket(int iPotionType);

	// ADD_MONSTER_REFINE
	// END_OF_ADD_MONSTER_REFINE


	void			SetRefineMode(int iAdditionalCell = -1);
	void			ClearRefineMode();

	bool			GiveItem(entt::entity victim, TItemPos Cell);
	bool			CanReceiveItem(entt::entity from, LPITEM item) const;
	void			ReceiveItem(entt::entity from, LPITEM item);
	bool			GiveItemFromSpecialItemGroup(uint32_t dwGroupNum, std::vector <uint32_t>& dwItemVnums,
		std::vector <uint32_t>& dwItemCounts, std::vector<entt::entity>& item_gets, int& count);

	bool			PickupItem(uint32_t vid);

	entt::entity		AutoGiveItem(uint32_t dwItemVnum,
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
	bool			CanTakeInventoryItem(entt::entity item, TItemPos* pos);

#ifdef ENABLE_EXTRA_INVENTORY
	int				GetEmptyExtraInventory(LPITEM pItem) const;
	int				GetEmptyExtraInventory(uint8_t size, uint8_t category) const; // needed for offline shop
#endif

	int				GetEmptyDragonSoulInventory(LPITEM pItem) const;


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


	bool			m_bItemLoaded;

public:
	////////////////////////////////////////////////////////////////////////////////////////
	// Money related

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	int				Inven_Point() const;
	int				Inventory_Size() const { return 90 + (5 * Inven_Point()); }
	void			Set_Inventory_Point(int value);
	bool			Update_Inven();
#endif
	bool			DropGold(int64_t gold);


#ifdef ENABLE_PVP_ADVANCED
#endif
#ifdef ENABLE_GAYA_SYSTEM
#endif

	// End of Money

	////////////////////////////////////////////////////////////////////////////////////////
	// Shop related
public:


#ifdef ENABLE_PVP_ADVANCED
	void			DestroyPvP();
#endif

protected:

	// End of shop


	////////////////////////////////////////////////////////////////////////////////////////
	// Exchange related
public:
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
	public:
		void SetWheelDestiny(std::shared_ptr<CWheelDestiny> pt);
		std::shared_ptr<CWheelDestiny> GetWheelDestiny() const { return pWheelDestiny; }

	private:
		std::shared_ptr<CWheelDestiny> pWheelDestiny = nullptr;
#endif
protected:
	// End of Exchange

#ifdef __DUNGEON_INFO_SYSTEM__
public:
	void				SetQuestDamage(int race, int dmg);

private:
	std::map<int, int>	dungeonDamage;
#endif

	////////////////////////////////////////////////////////////////////////////////////////
	// Battle
public:
	typedef struct SAttackLog
	{
		uint32_t	dwVID;
		uint32_t	dwTime;
	} AttackLog;


#ifdef __ENABLE_BERAN_ADDONS__
#endif






#ifdef __NEWPET_SYSTEM__
	//int GetBeltCount() const;//#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
#endif


#ifdef ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93
#endif

#ifdef ENABLE_RANKING

	//void SendLeaderboardData();
	//void SendLeaderboardNews();
	//static void LeaderboardLoop();
#endif
#ifdef LEADERBOARD_RAZOR93

#endif
	void				AttackedByFire(entt::entity attacker, int amount, int count);



	//int GetBeltCount() const;
#ifdef ENABLE_FAKE_SHOP_HEADER
	//void UpdateMountCountOverhead(LPCHARACTER ch);
#ifdef DISABLE_CORE_PULSE_RAZOR93


#endif
#endif
	//����ġ ���
	//void				ShowAlignment(bool bShow);





	//
	// HACK
	//
public:



protected:
	uint8_t m_bComboSequence;
	uint32_t m_dwLastComboTime;
	int m_iValidComboInterval;
	uint8_t m_bComboIndex;
	int m_iComboHackCount;

protected:

public:
	// Read-only view for entity-native callers; the map is keyed by entity
	// already, so nothing has to resolve a character to walk it.

private:
	//		AttackLog			m_kAttackLog;


	// Aggro

	// End of Battle

	// Stone
public:
#ifdef ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93
#else
#endif
#ifdef ENABLE_ITEMSHOP
#endif


	uint32_t				GetDropMetinStofaVnum() const { return m_dwDropMetinStofa; }
	uint8_t				GetDropMetinStofaPct() const { return m_bDropMetinStofaPct; }
	uint32_t				GetDropMetinSaccaVnum() const { return m_dwDropMetinSacca; }
	uint8_t				GetDropMetinSaccaPct() const { return m_bDropMetinSaccaPct; }

protected:
	uint32_t				m_dwDropMetinStofa;
	uint8_t				m_bDropMetinStofaPct;
	uint32_t				m_dwDropMetinSacca;
	uint8_t				m_bDropMetinSaccaPct;

#ifdef ENABLE_RANKING
protected:
	long long	m_lRankPoints[RANKING_MAX_CATEGORIES];
public:
#ifdef LEADERBOARD_RAZOR93
#endif
	long long	GetRankPoints(int iArg);
	void		SetRankPoints(int iArg, long long lPoint);
	void		RankingSubcategory(int iArg);
#endif
#ifdef ENABLE_NEW_PET_EDITS
public:

protected:
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
	// ADD_GRANDMASTER_SKILL
	bool				UseSkill(uint32_t dwVnum, entt::entity victim, bool bUseGrandMaster = true);
	void				ResetSkill();
	void				SetSkillLevel(uint32_t dwVnum, uint8_t bLev);

	bool				IsLearnableSkill(uint32_t dwSkillVnum) const;
	// END_OF_ADD_GRANDMASTER_SKILL

	bool				CanUseSkill(uint32_t dwSkillVnum) const;
	bool				IsUsableSkillMotion(uint32_t dwMotionIndex) const;
	int					GetSkillLevel(uint32_t dwVnum) const;
	int					GetSkillMasterType(uint32_t dwVnum) const;
	int					GetSkillPower(uint32_t dwVnum, uint8_t bLevel = 0) const;

	time_t				GetSkillNextReadTime(uint32_t dwVnum) const;
	void				SetSkillNextReadTime(uint32_t dwVnum, time_t time);

#ifdef ENABLE_NEW_GYEONGGONG_SKILL
	int					ComputeGyeongGongSkill(uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel = 0);
#endif
	int					ComputeSkill(uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel = 0);
#ifdef GROUP_BUFF
	int					ComputeSkillParty(uint32_t dwVnum, entt::entity victim, uint8_t bSkillLevel = 0);
#endif
	int					ComputeSkillAtPosition(uint32_t dwVnum, const PIXEL_POSITION& posTarget, uint8_t bSkillLevel = 0);

	void				SetSkillGroup(uint8_t bSkillGroup);
	uint8_t				GetSkillGroup() const;


	void				DisableCooltime();
	bool				LearnSkillByBook(uint32_t dwSkillVnum, uint8_t bProb = 0);
	bool				LearnGrandMasterSkill(uint32_t dwSkillVnum);

private:
	bool				m_bDisableCooltime;
	// End of Skill
#ifdef DISABLE_CORE_PULSE_RAZOR93

#endif
	// MOB_SKILL
public:
	bool				CanUseMobSkill(unsigned int idx) const;
	void				ResetMobSkillCooltime();
protected:
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

	////////////////////////////////////////////////////////////////////////////////////////
	// AI related
public:
	void			AssignTriggers(const TMobTable* table);

protected:

public:

	bool			OnIdle();

	void			OnClick(entt::entity causer);
	CTrigger&		GetTriggerOnClick() { return m_triggerOnClick; }
	const CTrigger&	GetTriggerOnClick() const { return m_triggerOnClick; }

	uint32_t		 m_dwLegacyVID { 0 };


protected:
	CTrigger		m_triggerOnClick;
	// End of AI

	////////////////////////////////////////////////////////////////////////////////////////
	// Target
protected:

public:

	////////////////////////////////////////////////////////////////////////////////////////
	// Safebox
public:


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

	/// â�� ���� ��û�� ���
	/**
	 * ReqSafeboxLoad �� ȣ���ϰ� CloseSafebox ���� �ʾ��� �� �� �Լ��� ȣ���ϸ� â���� �� �� �ִ�.
	 * â�������� ��û�� DB �������� ���������� �޾��� ��� �� �Լ��� ����ؼ� ��û�� �� �� �ְ� ���ش�.
	 */

	void				SetMallLoadTime(int t) { m_iMallLoadTime = t; }
	int					GetMallLoadTime() const { return m_iMallLoadTime; }



protected:

	bool				 m_bMountInventoryLoaded;

	int					m_iMallLoadTime;


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

#endif
#ifdef ENABLE_COSTUME_MOUNT
public:
	void	UpdateMountSkin();
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

protected:

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

protected:
	// End of Resists & Proofs

	////////////////////////////////////////////////////////////////////////////////////////
	// QUEST
	//
public:

	void				SetQuestItemPtr(entt::entity item);
	void				ClearQuestItemPtr();
	entt::entity		GetQuestItemEntity() const;
	LPITEM				GetQuestItemPtr() const;




private:

	// Events
public:
	bool				StartStateMachine(int iPulse = 1);
	void				StopStateMachine();
	void				UpdateStateMachine(uint32_t dwPulse);
	void				SetNextStatePulse(int iPulseNext);

	// ĳ���� �ν��Ͻ� ������Ʈ �Լ�. ������ �̻��� ��ӱ����� CFSM::Update �Լ��� ȣ���ϰų� UpdateStateMachine �Լ��� ����ߴµ�, ������ ������Ʈ �Լ� �߰���.

protected:

	// Marriage
public:
	LPCHARACTER			GetMarryPartner() const;
	void				SetMarryPartner(entt::entity character);
	int					GetMarriageBonus(uint32_t dwItemVnum, bool bSum = true);


private:
	LPCHARACTER			m_pkChrMarried;

	// Warp Character
public:

public:
	void				StartSaveEvent();
	void				StartDestroyWhenIdleEvent();


	//DELAYED_WARP
	//END_DELAYED_WARP

	// MINING
	LPEVENT				m_pkMiningEvent;
	// END_OF_MINING
	LPEVENT				m_pkDestroyWhenIdleEvent;
	LPEVENT				m_pkPetSystemUpdateEvent;
#ifdef __NEWPET_SYSTEM__
	LPEVENT				m_pkNewPetSystemUpdateEvent;
	LPEVENT				m_pkNewPetSystemExpireEvent;
#endif


	const CMob* m_pkMobData;
	const CMob* GetMobData() const { return m_pkMobData; }


	friend struct FuncSplashDamage;
	friend struct FuncSplashAffect;
	friend class CFuncShoot;

public:

private:
	int				m_aiPremiumTimes[PREMIUM_MAX_NUM];

	// CHANGE_ITEM_ATTRIBUTES
	// static const uint32_t		msc_dwDefaultChangeItemAttrCycle;	///< ����Ʈ ������ �Ӽ����� ���� �ֱ�
	public:
	static const char		msc_szLastChangeItemAttrFlag[];		///< �ֱ� ������ �Ӽ��� ������ �ð��� Quest Flag �̸�
	// static const char		msc_szChangeItemAttrCycleFlag[];		///< ������ �Ӽ����� ���� �ֱ��� Quest Flag �̸�
	// END_OF_CHANGE_ITEM_ATTRIBUTES

	// PC_BANG_ITEM_ADD
private:

public:
	bool SetPCBang(bool flag);
	// END_PC_BANG_ITEM_ADD

	// NEW_HAIR_STYLE_ADD
public:
	// END_NEW_HAIR_STYLE_ADD

public:
	void ClearSkill();

	// RESET_ONE_SKILL
	// END_RESET_ONE_SKILL


	// ARENA
private:

public:


	// END_ARENA

		//PREVENT_TRADE_WINDOW
public:
	bool	IsOpenSafebox() const;
	void 	SetOpenSafebox(bool b);

	//END_PREVENT_TRADE_WINDOW
private:

public:
	int		GetSkillPowerByLevel(int level, bool bMob = false) const;

	//PREVENT_REFINE_HACK
	//END_PREVENT_REFINE_HACK

	//RESTRICT_USE_SEED_OR_MOONBOTTLE
	//END_RESTRICT_USE_SEED_OR_MOONBOTTLE

	//PREVENT_PORTAL_AFTER_EXCHANGE
	//END_PREVENT_PORTAL_AFTER_EXCHANGE


	// Hack ������ ���� üũ.


public:

	// by mhh
	bool IsCubeOpen() const;
	void SetCubeNpc(entt::entity npc);
	bool CanDoCube() const;


private:

public:

private:
	void	__OpenPrivateShop(
#ifdef KASMIR_PAKET_SYSTEM
		bool bKasmir = false
#endif
	);

public:


private:
	std::string m_strNewName;

public:

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


private:
	uint32_t m_dwLastGoldDropTime;
#ifdef ENABLE_NEWSTUFF
public:
#endif
public:
#ifdef ENABLE_RECALL
	void AutoRecallProcess();
#endif

public:
	void BuffOnAttr_AddBuffsFromItem(LPITEM pItem);
	void BuffOnAttr_RemoveBuffsFromItem(LPITEM pItem);

private:
public:
	// Driven by the point-change flow, which lives in ecs::PointSystem now.
	uint32_t GetPlayStartTime() const { return m_dwPlayStartTime; }
private:

	// ���� : ��Ȱ�� �׽�Ʈ�� ���Ͽ�.
public:
private:
#ifdef __PET_SYSTEM__
private:
public:
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
private:

public:
#endif

#ifdef __NEWPET_SYSTEM__
private:
	int m_eggvid;
public:

#endif

	//���� ������ ����.
private:
public:

private:

public:
	//��ȥ��

	// ĳ������ affect, quest�� load �Ǳ� ���� DragonSoul_Initialize�� ȣ���ϸ� �ȵȴ�.
	// affect�� ���� �������� �ε�Ǿ� LoadAffect���� ȣ����.


	// �ݵ�� ClearItem ���� �ҷ��� �Ѵ�.
	// �ֳ��ϸ�....
	// ��ȥ�� �ϳ� �ϳ��� deactivate�� ������ ���� active�� ��ȥ���� �ִ��� Ȯ���ϰ�,
	// active�� ��ȥ���� �ϳ��� ���ٸ�, ĳ������ ��ȥ�� affect��, Ȱ�� ���¸� �����Ѵ�.
	//
	// ������ ClearItem ��, ĳ���Ͱ� �����ϰ� �ִ� ��� �������� unequip�ϴ� �ٶ���,
	// ��ȥ�� Affect�� ���ŵǰ�, �ᱹ �α��� ��, ��ȥ���� Ȱ��ȭ���� �ʴ´�.
	// (Unequip�� ������ �α׾ƿ� ��������, �ƴ��� �� �� ����.)
	// ��ȥ���� deactivate��Ű�� ĳ������ ��ȥ�� �� Ȱ�� ���´� �ǵ帮�� �ʴ´�.
	// ��ȥ�� ��ȭâ
public:
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
	//bool		 itemAward_flag;
public:
	//bool		 GetItemAward_flag() { return itemAward_flag; }
	//void		 SetItemAward_flag(bool flag) { itemAward_flag = flag; }
#ifdef ENABLE_ANTI_CMD_FLOOD
private:
public:
#endif
private:
	// SyncPosition�� �ǿ��Ͽ� Ÿ������ �̻��� ������ ������ �� ����ϱ� ���Ͽ�,
	// SyncPosition�� �Ͼ ���� ���.
	int			m_iSyncHackCount;
public:
	void			SetSyncHackCount(int iCount) { m_iSyncHackCount = iCount; }
	int				GetSyncHackCount() { return m_iSyncHackCount; }



#ifdef __HIDE_COSTUME_SYSTEM__
public:

#ifdef ENABLE_FREE_PASS_RAZOR93


	void EnsureFreeBattlePassActive();

#endif

#ifdef ENABLE_ACCE_SYSTEM
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
#endif

private:
#ifdef ENABLE_ACCE_SYSTEM
#endif
#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
	//uint32_t m_dwBattlePassStayOnlineNextTick;


#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
#endif
#endif


#ifdef ENABLE_GAYA_SYSTEM
public:
#endif


#ifdef ENABLE_SOUL_SYSTEM
public:
	int 		GetSoulItemDamage(entt::entity victim, int iDamage, uint8_t bSoulType);
#endif

#ifdef ENABLE_BATTLE_PASS
public:
	void LoadBattlePass(uint32_t dwCount, TPlayerBattlePassMission* data);

private:

public:
protected:

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
#endif
#ifdef TEXTS_IMPROVEMENT
public:
#endif
#ifdef ENABLE_NEW_FISHING_SYSTEM
public:
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

protected:
#endif

#ifdef ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
public:

protected:
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
//#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
//	std::shared_ptr<CWheelDestiny> pWheelDestiny = nullptr;
//#endif
public:
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

