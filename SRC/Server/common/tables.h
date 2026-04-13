#ifndef __INC_TABLES_H__
#define __INC_TABLES_H__

#include "length.h"
#include "item_length.h"
#include "CommonDefines.h"
#include "service.h"

typedef	uint32_t IDENT;

/**
 * @version 05/06/10	Bang2ni - Myshop Pricelist 관련 패킷 HEADER_XX_MYSHOP_PRICELIST_XXX 추가
 */
enum
{
	HEADER_GD_LOGIN				= 1,
	HEADER_GD_LOGOUT			= 2,

	HEADER_GD_PLAYER_LOAD		= 3,
	HEADER_GD_PLAYER_SAVE		= 4,
	HEADER_GD_PLAYER_CREATE		= 5,
	HEADER_GD_PLAYER_DELETE		= 6,

	HEADER_GD_LOGIN_KEY			= 7,
	// 8 empty
	HEADER_GD_BOOT				= 9,
	HEADER_GD_PLAYER_COUNT		= 10,
	HEADER_GD_QUEST_SAVE		= 11,
	HEADER_GD_SAFEBOX_LOAD		= 12,
	HEADER_GD_SAFEBOX_SAVE		= 13,
	HEADER_GD_SAFEBOX_CHANGE_SIZE	= 14,
	HEADER_GD_EMPIRE_SELECT		= 15,

	HEADER_GD_SAFEBOX_CHANGE_PASSWORD		= 16,
	HEADER_GD_SAFEBOX_CHANGE_PASSWORD_SECOND	= 17, // Not really a packet, used internal
	HEADER_GD_DIRECT_ENTER		= 18,

	HEADER_GD_GUILD_SKILL_UPDATE	= 19,
	HEADER_GD_GUILD_EXP_UPDATE		= 20,
	HEADER_GD_GUILD_ADD_MEMBER		= 21,
	HEADER_GD_GUILD_REMOVE_MEMBER	= 22,
	HEADER_GD_GUILD_CHANGE_GRADE	= 23,
	HEADER_GD_GUILD_CHANGE_MEMBER_DATA	= 24,
	HEADER_GD_GUILD_DISBAND		= 25,
	HEADER_GD_GUILD_WAR			= 26,
	HEADER_GD_GUILD_WAR_SCORE		= 27,
	HEADER_GD_GUILD_CREATE		= 28,

#ifdef ADVANCED_GUILD_INFO
	HEADER_GD_GUILD_RESET 		= 29, //Reset all status for all channels
#endif

	HEADER_GD_ITEM_SAVE			= 30,
	HEADER_GD_ITEM_DESTROY		= 31,

	HEADER_GD_ADD_AFFECT		= 32,
	HEADER_GD_REMOVE_AFFECT		= 33,

	HEADER_GD_HIGHSCORE_REGISTER	= 34,
	HEADER_GD_ITEM_FLUSH		= 35,

	HEADER_GD_PARTY_CREATE		= 36,
	HEADER_GD_PARTY_DELETE		= 37,
	HEADER_GD_PARTY_ADD			= 38,
	HEADER_GD_PARTY_REMOVE		= 39,
	HEADER_GD_PARTY_STATE_CHANGE	= 40,
	HEADER_GD_PARTY_HEAL_USE		= 41,

	HEADER_GD_FLUSH_CACHE		= 42,
	HEADER_GD_RELOAD_PROTO		= 43,

	HEADER_GD_CHANGE_NAME		= 44,
	HEADER_GD_SMS				= 45,

	HEADER_GD_GUILD_CHANGE_LADDER_POINT	= 46,
	HEADER_GD_GUILD_USE_SKILL		= 47,

	HEADER_GD_REQUEST_EMPIRE_PRIV	= 48,
	HEADER_GD_REQUEST_GUILD_PRIV	= 49,

	HEADER_GD_MONEY_LOG				= 50,

	HEADER_GD_GUILD_DEPOSIT_MONEY				= 51,
	HEADER_GD_GUILD_WITHDRAW_MONEY				= 52,
	HEADER_GD_GUILD_WITHDRAW_MONEY_GIVE_REPLY	= 53,

	HEADER_GD_REQUEST_CHARACTER_PRIV	= 54,

	HEADER_GD_SET_EVENT_FLAG			= 55,

	HEADER_GD_PARTY_SET_MEMBER_LEVEL	= 56,

	HEADER_GD_GUILD_WAR_BET		= 57,

	HEADER_GD_CREATE_OBJECT		= 60,
	HEADER_GD_DELETE_OBJECT		= 61,
	HEADER_GD_UPDATE_LAND		= 62,

	HEADER_GD_MARRIAGE_ADD		= 70,
	HEADER_GD_MARRIAGE_UPDATE	= 71,
	HEADER_GD_MARRIAGE_REMOVE	= 72,

	HEADER_GD_WEDDING_REQUEST	= 73,
	HEADER_GD_WEDDING_READY		= 74,
	HEADER_GD_WEDDING_END		= 75,

#ifdef ENABLE_BATTLE_PASS
	HEADER_GD_SAVE_BATTLE_PASS		= 82,
	HEADER_GD_REGISTER_BP_RANKING	= 83,
	HEADER_GD_BATTLE_PASS_RANKING 	= 84,
#endif
#ifdef ENABLE_HWID
	HEADER_GD_BLOCKHWID = 85,
	HEADER_GD_UNBLOCKHWID = 86,
#endif
	HEADER_GD_AUTH_LOGIN		= 100,
	HEADER_GD_LOGIN_BY_KEY		= 101,
	HEADER_GD_BILLING_EXPIRE	= 104,
	HEADER_GD_VCARD				= 105,
	HEADER_GD_BILLING_CHECK		= 106,
	HEADER_GD_MALL_LOAD			= 107,

	HEADER_GD_MYSHOP_PRICELIST_UPDATE	= 108,		///< 가격정보 갱신 요청
	HEADER_GD_MYSHOP_PRICELIST_REQ		= 109,		///< 가격정보 리스트 요청

	HEADER_GD_BLOCK_CHAT				= 110,

	// PCBANG_IP_LIST_BY_AUTH
	HEADER_GD_PCBANG_REQUEST_IP_LIST	= 111,
	HEADER_GD_PCBANG_CLEAR_IP_LIST		= 112,
	HEADER_GD_PCBANG_INSERT_IP			= 113,
	// END_OF_PCBANG_IP_LIST_BY_AUTH

	HEADER_GD_HAMMER_OF_TOR			= 114,
	HEADER_GD_RELOAD_ADMIN			= 115,			///<운영자 정보 요청
	HEADER_GD_BREAK_MARRIAGE		= 116,			///< 결혼 파기

	HEADER_GD_REQ_CHANGE_GUILD_MASTER	= 129,

	HEADER_GD_REQ_SPARE_ITEM_ID_RANGE	= 130,

	HEADER_GD_UPDATE_HORSE_NAME		= 131,
	HEADER_GD_REQ_HORSE_NAME		= 132,

	HEADER_GD_DC					= 133,		// Login Key를 지움

	HEADER_GD_VALID_LOGOUT			= 134,
	HEADER_GD_REQUEST_CHARGE_CASH	= 137,

	HEADER_GD_DELETE_AWARDID	= 138,	// delete gift notify icon

	HEADER_GD_UPDATE_CHANNELSTATUS	= 139,
	HEADER_GD_REQUEST_CHANNELSTATUS	= 140,
#if defined(BL_OFFLINE_MESSAGE)
	HEADER_GD_REQUEST_OFFLINE_MESSAGES = 143,
	HEADER_GD_SEND_OFFLINE_MESSAGE = 144,
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
	HEADER_GD_NEW_OFFLINESHOP		= 153,
#endif

#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
	HEADER_GD_FIND_CHANNEL = 99,
#endif

#ifdef __SKILL_COLOR_SYSTEM__
	HEADER_GD_SKILL_COLOR_SAVE = 145,
#endif

	HEADER_GD_SETUP			= 0xff,

	///////////////////////////////////////////////
	HEADER_DG_NOTICE			= 1,

	HEADER_DG_LOGIN_SUCCESS			= 30,
	HEADER_DG_LOGIN_NOT_EXIST		= 31,
	HEADER_DG_LOGIN_WRONG_PASSWD	= 33,
	HEADER_DG_LOGIN_ALREADY			= 34,

	HEADER_DG_PLAYER_LOAD_SUCCESS	= 35,
	HEADER_DG_PLAYER_LOAD_FAILED	= 36,
	HEADER_DG_PLAYER_CREATE_SUCCESS	= 37,
	HEADER_DG_PLAYER_CREATE_ALREADY	= 38,
	HEADER_DG_PLAYER_CREATE_FAILED	= 39,
	HEADER_DG_PLAYER_DELETE_SUCCESS	= 40,
	HEADER_DG_PLAYER_DELETE_FAILED	= 41,

	HEADER_DG_ITEM_LOAD			= 42,

	HEADER_DG_BOOT				= 43,
	HEADER_DG_QUEST_LOAD		= 44,

	HEADER_DG_SAFEBOX_LOAD					= 45,
	HEADER_DG_SAFEBOX_CHANGE_SIZE			= 46,
	HEADER_DG_SAFEBOX_WRONG_PASSWORD		= 47,
	HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER = 48,

	HEADER_DG_EMPIRE_SELECT		= 49,

	HEADER_DG_AFFECT_LOAD		= 50,
	HEADER_DG_MALL_LOAD			= 51,

	HEADER_DG_DIRECT_ENTER		= 55,

	HEADER_DG_GUILD_SKILL_UPDATE	= 56,
	HEADER_DG_GUILD_SKILL_RECHARGE	= 57,
	HEADER_DG_GUILD_EXP_UPDATE		= 58,

	HEADER_DG_PARTY_CREATE		= 59,
	HEADER_DG_PARTY_DELETE		= 60,
	HEADER_DG_PARTY_ADD			= 61,
	HEADER_DG_PARTY_REMOVE		= 62,
	HEADER_DG_PARTY_STATE_CHANGE	= 63,
	HEADER_DG_PARTY_HEAL_USE		= 64,
	HEADER_DG_PARTY_SET_MEMBER_LEVEL	= 65,

#ifdef ENABLE_ITEMSHOP
	HEADER_DG_ITEMSHOP					= 76,
	HEADER_GD_ITEMSHOP					= 76,
#endif

#ifdef ENABLE_BATTLE_PASS
	HEADER_DG_BATTLE_PASS_LOAD = 78,
	HEADER_DG_BATTLE_PASS_LOAD_RANKING = 79,
#endif
#ifdef ENABLE_HWID
	HEADER_DG_BLOCKHWID = 80,
	HEADER_DG_UNBLOCKHWID = 81,
#endif

	HEADER_DG_TIME			= 90,
	HEADER_DG_ITEM_ID_RANGE		= 91,

	HEADER_DG_GUILD_ADD_MEMBER		= 92,
	HEADER_DG_GUILD_REMOVE_MEMBER	= 93,
	HEADER_DG_GUILD_CHANGE_GRADE	= 94,
	HEADER_DG_GUILD_CHANGE_MEMBER_DATA	= 95,
	HEADER_DG_GUILD_DISBAND		= 96,
	HEADER_DG_GUILD_WAR			= 97,
	HEADER_DG_GUILD_WAR_SCORE		= 98,
	HEADER_DG_GUILD_TIME_UPDATE		= 99,
	HEADER_DG_GUILD_LOAD		= 100,
	HEADER_DG_GUILD_LADDER		= 101,
	HEADER_DG_GUILD_SKILL_USABLE_CHANGE	= 102,
	HEADER_DG_GUILD_MONEY_CHANGE	= 103,
	HEADER_DG_GUILD_WITHDRAW_MONEY_GIVE	= 104,

	HEADER_DG_SET_EVENT_FLAG		= 105,

	HEADER_DG_GUILD_WAR_RESERVE_ADD	= 106,
	HEADER_DG_GUILD_WAR_RESERVE_DEL	= 107,
	HEADER_DG_GUILD_WAR_BET		= 108,

#ifdef ADVANCED_GUILD_INFO
	HEADER_DG_GUILD_WAR_RESET		= 109,
#endif

	HEADER_DG_RELOAD_PROTO		= 120,
	HEADER_DG_CHANGE_NAME		= 121,

	HEADER_DG_AUTH_LOGIN		= 122,

	HEADER_DG_CHANGE_EMPIRE_PRIV	= 124,
	HEADER_DG_CHANGE_GUILD_PRIV		= 125,

	HEADER_DG_MONEY_LOG			= 126,

	HEADER_DG_CHANGE_CHARACTER_PRIV	= 127,

	HEADER_DG_BILLING_REPAIR		= 128,
	HEADER_DG_BILLING_EXPIRE		= 129,
	HEADER_DG_BILLING_LOGIN		= 130,
	HEADER_DG_VCARD			= 131,
	HEADER_DG_BILLING_CHECK		= 132,

	HEADER_DG_CREATE_OBJECT		= 140,
	HEADER_DG_DELETE_OBJECT		= 141,
	HEADER_DG_UPDATE_LAND		= 142,

	HEADER_DG_MARRIAGE_ADD		= 150,
	HEADER_DG_MARRIAGE_UPDATE		= 151,
	HEADER_DG_MARRIAGE_REMOVE		= 152,

	HEADER_DG_WEDDING_REQUEST		= 153,
	HEADER_DG_WEDDING_READY		= 154,
	HEADER_DG_WEDDING_START		= 155,
	HEADER_DG_WEDDING_END		= 156,

	HEADER_DG_MYSHOP_PRICELIST_RES	= 157,		///< 가격정보 리스트 응답
	HEADER_DG_RELOAD_ADMIN = 158, 				///< 운영자 정보 리로드
	HEADER_DG_BREAK_MARRIAGE = 159,				///< 결혼 파기

	HEADER_DG_BLOCK_COUNTRY_IP		= 171,		// 광대역 IP-Block
	HEADER_DG_BLOCK_EXCEPTION		= 172,		// 광대역 IP-Block 예외 account

	HEADER_DG_ACK_CHANGE_GUILD_MASTER = 173,

	HEADER_DG_ACK_SPARE_ITEM_ID_RANGE = 174,

	HEADER_DG_UPDATE_HORSE_NAME 	= 175,
	HEADER_DG_ACK_HORSE_NAME		= 176,

	HEADER_DG_NEED_LOGIN_LOG		= 177,
	HEADER_DG_RESULT_CHARGE_CASH	= 179,
	HEADER_DG_ITEMAWARD_INFORMER	= 180,	//gift notify
	HEADER_DG_RESPOND_CHANNELSTATUS		= 181,
#ifdef __ENABLE_NEW_OFFLINESHOP__
	HEADER_DG_NEW_OFFLINESHOP		= 190,
#endif

#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
	HEADER_DG_CHANNEL_RESULT = 184,
#endif
#ifdef ENABLE_ITEM_EXTRA_PROTO
	HEADER_DG_ITEM_EXTRA_PROTO_LOAD = 185,
#endif

#ifdef __SKILL_COLOR_SYSTEM__
	HEADER_DG_SKILL_COLOR_LOAD = 186,
#endif
#if defined(BL_OFFLINE_MESSAGE)
	HEADER_DG_RESPOND_OFFLINE_MESSAGES = 187,
#endif

#ifdef ENABLE_EVENT_MANAGER
	HEADER_DG_EVENT_MANAGER						= 212,
	HEADER_GD_EVENT_MANAGER						= 212,
#endif

	HEADER_DG_MAP_LOCATIONS		= 0xfe,
	HEADER_DG_P2P			= 0xff,
};

/* game Server -> DB Server */
#pragma pack(1)
enum ERequestChargeType
{
	ERequestCharge_Cash = 0,
	ERequestCharge_Mileage,
};

typedef struct SRequestChargeCash
{
	int32_t aid;
	int32_t amount;
} TRequestChargeCash;

typedef struct SSimplePlayer
{
	uint32_t		dwID;
	char		szName[CHARACTER_NAME_MAX_LEN + 1];
	uint8_t		byJob;
	uint8_t		byLevel;
	uint32_t		dwPlayMinutes;
	uint8_t		byST, byHT, byDX, byIQ;
	uint16_t		wMainPart;
	uint8_t		bChangeName;
	uint16_t		wHairPart;
#ifdef ENABLE_ACCE_SYSTEM
	uint16_t		wAccePart;
#endif
	uint8_t		bDummy[4];
	int32_t		x, y;
	uint32_t		lAddr;
	uint16_t		wPort;
	uint8_t		skill_group;
#if defined(ENABLE_ORDER_BY_LASTPLAY)
	int32_t lastplay;
#endif
} TSimplePlayer;

typedef struct SAccountTable
{
#ifdef ENABLE_HWID
	char hwid[HWID_LENGTH + 1];
#endif
	uint32_t id;
	char login[LOGIN_MAX_LEN + 1];
	char passwd[PASSWD_MAX_LEN + 1];
	char social_id[SOCIAL_ID_MAX_LEN + 1];
#ifdef ENABLE_MULTILANGUAGE_SYSTEM
	char language[LANGUAGE_MAX_LEN + 1];
#endif
	char status[ACCOUNT_STATUS_MAX_LEN + 1];
	uint8_t bEmpire;
#ifdef ENABLE_MULTI_LANGUAGE
	uint8_t bLanguage;
#endif
#ifdef ENABLE_GENERAL_CH
	uint8_t bChannel;
#endif
	TSimplePlayer players[PLAYER_PER_ACCOUNT];
} TAccountTable;

typedef struct SPacketDGCreateSuccess
{
	uint8_t		bAccountCharacterIndex;
	TSimplePlayer	player;
} TPacketDGCreateSuccess;

typedef struct TPlayerItemAttribute
{
	uint8_t	bType;
	short	sValue;
} TPlayerItemAttribute;

typedef struct SPlayerItem
{
	uint32_t	id;
	uint8_t	window;
	uint16_t	pos;
	uint32_t	count;

	uint32_t	vnum;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];	// 소켓번호

	TPlayerItemAttribute    aAttr[ITEM_ATTRIBUTE_MAX_NUM];

	uint32_t	owner;
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
} TPlayerItem;

typedef struct SQuickslot
{
	uint8_t	type;
	uint16_t	pos;
} TQuickslot;

typedef struct SPlayerSkill
{
	uint8_t	bMasterType;
	uint8_t	bLevel;
	uint64_t	tNextRead;
} TPlayerSkill;

struct	THorseInfo
{
	uint8_t	bLevel;
	uint8_t	bRiding;
	short	sStamina;
	short	sHealth;
	uint32_t	dwHorseHealthDropTime;
};

typedef struct SPlayerTable
{
	uint32_t	id;

	char	name[CHARACTER_NAME_MAX_LEN + 1];
	char	ip[IP_ADDRESS_LENGTH + 1];

	uint8_t	job;
	uint8_t	voice;

	uint8_t	level;
	uint8_t	level_step;
	uint8_t	st, ht, dx, iq;

	uint32_t	exp;

	int64_t	gold;

#ifdef ENABLE_GAYA_SYSTEM
	int		gaya;
#endif

	uint8_t	dir;
	int32_t		x, y, z;
	int32_t		lMapIndex;

	int32_t	lExitX, lExitY;
	int32_t	lExitMapIndex;

	// @fixme301
	int		hp;
	int		sp;

	short	sRandomHP;
	short	sRandomSP;

	int         playtime;

	short	stat_point;
	short	skill_point;
	short	sub_skill_point;
	short	horse_skill_point;

	TPlayerSkill skills[SKILL_MAX_NUM];

	TQuickslot  quickslot[QUICKSLOT_MAX_NUM];

	uint8_t	part_base;
	uint16_t	parts[PART_MAX_NUM];

	short	stamina;

	uint8_t	skill_group;
	uint32_t	lAlignment;
	char	szMobile[MOBILE_MAX_LEN + 1];

	short	stat_reset_count;

	THorseInfo	horse;

	uint32_t	logoff_interval;

	int		aiPremiumTimes[PREMIUM_MAX_NUM];
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	int 	envanter;
#endif

#ifdef ENABLE_BATTLE_PASS
	uint32_t	dwBattlePassEndTime;
#endif
#ifdef ENABLE_RANKING
	long long	lRankPoints[RANKING_MAX_CATEGORIES];
#endif
} TPlayerTable;

typedef struct SMobSkillLevel
{
	uint32_t	dwVnum;
	uint8_t	bLevel;
} TMobSkillLevel;

typedef struct SEntityTable
{
	uint32_t dwVnum;
} TEntityTable;

typedef struct SMobTable : public SEntityTable
{
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
#ifdef ENABLE_MULTI_NAMES
	char	szLocaleName[LANGUAGE_MAX_NUM][CHARACTER_NAME_MAX_LEN + 1];
#else
	char	szLocaleName[CHARACTER_NAME_MAX_LEN + 1];
#endif

	uint8_t	bType;			// Monster, NPC
	uint8_t	bRank;			// PAWN, KNIGHT, KING
	uint8_t	bBattleType;		// MELEE, etc..
	uint8_t	bLevel;			// Level
	uint8_t	bSize;

	uint32_t	dwGoldMin;
	uint32_t	dwGoldMax;
	uint32_t	dwExp;
	uint64_t	dwMaxHP;
	uint8_t	bRegenCycle;
	uint8_t	bRegenPercent;
	uint16_t	wDef;

	uint32_t	dwAIFlag;
	uint32_t	dwRaceFlag;
	uint32_t	dwImmuneFlag;

	uint8_t	bStr, bDex, bCon, bInt;
	uint32_t	dwDamageRange[2];

	short	sAttackSpeed;
	short	sMovingSpeed;
	uint8_t	bAggresiveHPPct;
	uint16_t	wAggressiveSight;
	uint16_t	wAttackRange;

	char	cEnchants[MOB_ENCHANTS_MAX_NUM];
	char	cResists[MOB_RESISTS_MAX_NUM];

	uint32_t	dwResurrectionVnum;
	uint32_t	dwDropItemVnum;

	uint8_t	bMountCapacity;
	uint8_t	bOnClickType;

	uint8_t	bEmpire;
	char	szFolder[64 + 1];

	float	fDamMultiply;

	uint32_t	dwSummonVnum;
	uint32_t	dwDrainSP;
	uint32_t	dwMobColor;
	uint32_t	dwPolymorphItemVnum;

	TMobSkillLevel Skills[MOB_SKILL_MAX_NUM];

	uint8_t	bBerserkPoint;
	uint8_t	bStoneSkinPoint;
	uint8_t	bGodSpeedPoint;
	uint8_t	bDeathBlowPoint;
	uint8_t	bRevivePoint;
} TMobTable;

typedef struct SSkillTable
{
	uint32_t	dwVnum;
	char	szName[32 + 1];
	uint8_t	bType;
	uint8_t	bMaxLevel;
	uint32_t	dwSplashRange;

	char	szPointOn[64];
	char	szPointPoly[100 + 1];
	char	szSPCostPoly[100 + 1];
	char	szDurationPoly[100 + 1];
	char	szDurationSPCostPoly[100 + 1];
	char	szCooldownPoly[100 + 1];
	char	szMasterBonusPoly[100 + 1];
	//char	szAttackGradePoly[100 + 1];
	char	szGrandMasterAddSPCostPoly[100 + 1];
	uint32_t	dwFlag;
	uint32_t	dwAffectFlag;

	// Data for secondary skill
	char 	szPointOn2[64];
	char 	szPointPoly2[100 + 1];
	char 	szDurationPoly2[100 + 1];
	uint32_t 	dwAffectFlag2;

	// Data for grand master point
	char 	szPointOn3[64];
	char 	szPointPoly3[100 + 1];
	char 	szDurationPoly3[100 + 1];

	uint8_t	bLevelStep;
	uint8_t	bLevelLimit;
	uint32_t	preSkillVnum;
	uint8_t	preSkillLevel;

	int32_t	lMaxHit;
	char	szSplashAroundDamageAdjustPoly[100 + 1];

	uint8_t	bSkillAttrType;

	uint32_t	dwTargetRange;
} TSkillTable;

#ifdef ENABLE_BUY_WITH_ITEM
typedef struct SShopItemPrice
{
	uint32_t		vnum;
	uint32_t		count;
} TShopItemPrice;
#endif

typedef struct SShopItemTable
{
	uint32_t		vnum;
#ifdef ENABLE_NEW_STACK_LIMIT
	int			count;
#else
	uint8_t		count;
#endif
#ifdef ENABLE_BUY_WITH_ITEM
	TShopItemPrice	itemprice[MAX_SHOP_PRICES];
#endif

	int64_t	price;
	TItemPos	pos;
	uint8_t		display_pos;
} TShopItemTable;

typedef struct SShopTable
{
	uint32_t		dwVnum;
	uint32_t		dwNPCVnum;
	uint8_t		byItemCount;
	TShopItemTable	items[SHOP_HOST_ITEM_MAX_NUM];
} TShopTable;

#define QUEST_NAME_MAX_LEN	32
#define QUEST_STATE_MAX_LEN	64

typedef struct SQuestTable
{
	uint32_t		dwPID;
	char		szName[QUEST_NAME_MAX_LEN + 1];
	char		szState[QUEST_STATE_MAX_LEN + 1];
	int32_t		lValue;
} TQuestTable;

typedef struct SItemLimit
{
	uint8_t	bType;
	int32_t	lValue;
} TItemLimit;

typedef struct SItemApply
{
	uint8_t	bType;
	int32_t	lValue;
} TItemApply;

typedef struct SItemTable : public SEntityTable
{
	uint32_t		dwVnumRange;
	char        szName[ITEM_NAME_MAX_LEN + 1];
#ifdef ENABLE_MULTI_NAMES
	char	szLocaleName[LANGUAGE_MAX_NUM][ITEM_NAME_MAX_LEN + 1];
#else
	char	szLocaleName[ITEM_NAME_MAX_LEN + 1];
#endif
	uint8_t	bType;
	uint8_t	bSubType;

	uint8_t        bWeight;
	uint8_t	bSize;

	uint32_t	dwAntiFlags;
	uint32_t	dwFlags;
	uint32_t	dwWearFlags;
	uint32_t	dwImmuneFlag;

	int64_t	dwGold;
	int64_t	dwShopBuyPrice;


	TItemLimit	aLimits[ITEM_LIMIT_MAX_NUM];
	TItemApply	aApplies[ITEM_APPLY_MAX_NUM];
	int32_t        alValues[ITEM_VALUES_MAX_NUM];
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	uint32_t	dwRefinedVnum;
	uint16_t	wRefineSet;
	uint8_t	bAlterToMagicItemPct;
	uint8_t	bSpecular;
	uint8_t	bGainSocketPct;

	short int	sAddonType;

	char		cLimitRealTimeFirstUseIndex;
	char		cLimitTimerBasedOnWearIndex;

} TItemTable;

struct TItemAttrTable
{
	TItemAttrTable() :
		dwApplyIndex(0),
		dwProb(0)
	{
		szApply[0] = 0;
		memset(&lValues, 0, sizeof(lValues));
		memset(&bMaxLevelBySet, 0, sizeof(bMaxLevelBySet));
	}

	char	szApply[APPLY_NAME_MAX_LEN + 1];
	uint32_t	dwApplyIndex;
	uint32_t	dwProb;
#ifdef ENABLE_ATTR_COSTUMES
	int32_t	lValues[ITEM_ATTRIBUTE_MAX_LEVEL + COSTUME_ATTRIBUTE_MAX_LEVEL];
#else
	int32_t	lValues[ITEM_ATTRIBUTE_MAX_LEVEL];
#endif
	uint8_t	bMaxLevelBySet[ATTRIBUTE_SET_MAX_NUM];
};

typedef struct SConnectTable
{
	char	login[LOGIN_MAX_LEN + 1];
	IDENT	ident;
} TConnectTable;

typedef struct SLoginPacket
{
	char	login[LOGIN_MAX_LEN + 1];
	char	passwd[PASSWD_MAX_LEN + 1];
} TLoginPacket;

typedef struct SPlayerLoadPacket
{
	uint32_t	account_id;
	uint32_t	player_id;
	uint8_t	account_index;	/* account 에서의 위치 */
} TPlayerLoadPacket;

typedef struct SPlayerCreatePacket
{
	char		login[LOGIN_MAX_LEN + 1];
	char		passwd[PASSWD_MAX_LEN + 1];
	uint32_t		account_id;
	uint8_t		account_index;
	TPlayerTable	player_table;
} TPlayerCreatePacket;

typedef struct SPlayerDeletePacket
{
	char	login[LOGIN_MAX_LEN + 1];
	uint32_t	player_id;
	uint8_t	account_index;
	//char	name[CHARACTER_NAME_MAX_LEN + 1];
	char	private_code[8];
} TPlayerDeletePacket;

typedef struct SLogoutPacket
{
	char	login[LOGIN_MAX_LEN + 1];
	char	passwd[PASSWD_MAX_LEN + 1];
} TLogoutPacket;

typedef struct SPlayerCountPacket
{
	uint32_t	dwCount;
} TPlayerCountPacket;

#define SAFEBOX_MAX_NUM			432
#define SAFEBOX_PASSWORD_MAX_LEN	6





typedef struct SSafeboxTable
{
	uint32_t	dwID;
	uint8_t	bSize;
	uint32_t	dwGold;
	uint16_t	wItemCount;
} TSafeboxTable;

typedef struct SSafeboxChangeSizePacket
{
	uint32_t	dwID;
	uint8_t	bSize;
} TSafeboxChangeSizePacket;

typedef struct SSafeboxLoadPacket
{
	uint32_t	dwID;
	char	szLogin[LOGIN_MAX_LEN + 1];
	char	szPassword[SAFEBOX_PASSWORD_MAX_LEN + 1];
} TSafeboxLoadPacket;

typedef struct SSafeboxChangePasswordPacket
{
	uint32_t	dwID;
	char	szOldPassword[SAFEBOX_PASSWORD_MAX_LEN + 1];
	char	szNewPassword[SAFEBOX_PASSWORD_MAX_LEN + 1];
} TSafeboxChangePasswordPacket;

typedef struct SSafeboxChangePasswordPacketAnswer
{
	uint8_t	flag;
} TSafeboxChangePasswordPacketAnswer;

typedef struct SEmpireSelectPacket
{
	uint32_t	dwAccountID;
	uint8_t	bEmpire;
} TEmpireSelectPacket;


typedef struct SMountInventoryItemTable
{
	uint32_t	id;
	uint16_t	slot;
	uint32_t	vnum;
	uint32_t	count;
	int32_t alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute	aAttr[ITEM_ATTRIBUTE_MAX_NUM];
} TMountInventoryItemTable;


typedef struct SPacketGDSetup
{
	char	szPublicIP[16];	// Public IP which listen to users
	uint8_t	bChannel;	// 채널
	uint16_t	wListenPort;	// 클라이언트가 접속하는 포트 번호
	uint16_t	wP2PPort;	// 서버끼리 연결 시키는 P2P 포트 번호
	int32_t	alMaps[MAP_ALLOW_LIMIT];
	uint32_t	dwLoginCount;
	uint8_t	bAuthServer;
} TPacketGDSetup;

typedef struct SPacketDGMapLocations
{
	uint8_t	bCount;
} TPacketDGMapLocations;

typedef struct SMapLocation
{
	int32_t alMaps[MAP_ALLOW_LIMIT];
	char szHost[MAX_HOST_LENGTH + 1];
	uint16_t wPort;
#ifdef ENABLE_GENERAL_CH
	uint8_t bChannel;
#endif
} TMapLocation;

typedef struct SPacketDGP2P
{
	char	szHost[MAX_HOST_LENGTH + 1];
	uint16_t	wPort;
	uint8_t	bChannel;
} TPacketDGP2P;

typedef struct SPacketGDDirectEnter
{
	char	login[LOGIN_MAX_LEN + 1];
	char	passwd[PASSWD_MAX_LEN + 1];
	uint8_t	index;
} TPacketGDDirectEnter;

typedef struct SPacketDGDirectEnter
{
	TAccountTable accountTable;
	TPlayerTable playerTable;
} TPacketDGDirectEnter;

typedef struct SPacketGuildSkillUpdate
{
	uint32_t guild_id;
	int amount;
	uint8_t skill_levels[12];
	uint8_t skill_point;
	uint8_t save;
} TPacketGuildSkillUpdate;

typedef struct SPacketGuildExpUpdate
{
	uint32_t guild_id;
	int amount;
} TPacketGuildExpUpdate;

typedef struct SPacketGuildChangeMemberData
{
	uint32_t guild_id;
	uint32_t pid;
	uint32_t offer;
	uint8_t level;
	uint8_t grade;
} TPacketGuildChangeMemberData;


typedef struct SPacketDGLoginAlready
{
	char	szLogin[LOGIN_MAX_LEN + 1];
} TPacketDGLoginAlready;

typedef struct TPacketAffectElement
{
	uint32_t	dwType;
	uint8_t	bApplyOn;
	int32_t	lApplyValue;
	uint32_t	dwFlag;
	int32_t	lDuration;
	int32_t	lSPCost;
} TPacketAffectElement;

typedef struct SPacketGDAddAffect
{
	uint32_t			dwPID;
	TPacketAffectElement	elem;
} TPacketGDAddAffect;

typedef struct SPacketGDRemoveAffect
{
	uint32_t	dwPID;
	uint32_t	dwType;
	uint8_t	bApplyOn;
} TPacketGDRemoveAffect;

typedef struct SPacketGDHighscore
{
	uint32_t	dwPID;
	int32_t	lValue;
	char	cDir;
	char	szBoard[21];
} TPacketGDHighscore;

typedef struct SPacketPartyCreate
{
	uint32_t	dwLeaderPID;
} TPacketPartyCreate;

typedef struct SPacketPartyDelete
{
	uint32_t	dwLeaderPID;
} TPacketPartyDelete;

typedef struct SPacketPartyAdd
{
	uint32_t	dwLeaderPID;
	uint32_t	dwPID;
	uint8_t	bState;
} TPacketPartyAdd;

typedef struct SPacketPartyRemove
{
	uint32_t	dwLeaderPID;
	uint32_t	dwPID;
} TPacketPartyRemove;

typedef struct SPacketPartyStateChange
{
	uint32_t	dwLeaderPID;
	uint32_t	dwPID;
	uint8_t	bRole;
	uint8_t	bFlag;
} TPacketPartyStateChange;

typedef struct SPacketPartySetMemberLevel
{
	uint32_t	dwLeaderPID;
	uint32_t	dwPID;
	uint8_t	bLevel;
} TPacketPartySetMemberLevel;

typedef struct SPacketGDBoot
{
    uint32_t	dwItemIDRange[2];
	char	szIP[16];
} TPacketGDBoot;

typedef struct SPacketGuild
{
	uint32_t	dwGuild;
	uint32_t	dwInfo;
} TPacketGuild;

typedef struct SPacketGDGuildAddMember
{
	uint32_t	dwPID;
	uint32_t	dwGuild;
	uint8_t	bGrade;
} TPacketGDGuildAddMember;

typedef struct SPacketDGGuildMember
{
	uint32_t	dwPID;
	uint32_t	dwGuild;
	uint8_t	bGrade;
	uint8_t	isGeneral;
	uint8_t	bJob;
	uint8_t	bLevel;
	uint32_t	dwOffer;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketDGGuildMember;

typedef struct SPacketGuildWar
{
	uint8_t	bType;
	uint8_t	bWar;
	uint32_t	dwGuildFrom;
	uint32_t	dwGuildTo;
	int32_t	lWarPrice;
	int32_t	lInitialScore;
} TPacketGuildWar;

// Game -> DB : 상대적 변화값
// DB -> Game : 토탈된 최종값
typedef struct SPacketGuildWarScore
{
	uint32_t dwGuildGainPoint;
	uint32_t dwGuildOpponent;
	int32_t lScore;
	int32_t lBetScore;
} TPacketGuildWarScore;

#ifdef ADVANCED_GUILD_INFO
typedef struct SPacketGuildReset
{
	uint32_t stat;
} TPacketGuildReset;
#endif

typedef struct SRefineMaterial
{
	uint32_t vnum;
	int count;
} TRefineMaterial;

typedef struct SRefineTable
{
	//uint32_t src_vnum;
	//uint32_t result_vnum;
	uint32_t id;
	uint8_t material_count;
	int64_t cost; // 소요 비용
	int prob; // 확률
	TRefineMaterial materials[REFINE_MATERIAL_MAX_NUM];
} TRefineTable;

typedef struct SBanwordTable
{
	char szWord[BANWORD_MAX_LEN + 1];
} TBanwordTable;

typedef struct SPacketGDChangeName
{
	uint32_t pid;
	char name[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGDChangeName;

typedef struct SPacketDGChangeName
{
	uint32_t pid;
	char name[CHARACTER_NAME_MAX_LEN + 1];
} TPacketDGChangeName;

typedef struct SPacketGuildLadder
{
	uint32_t dwGuild;
	int32_t lLadderPoint;
	int32_t lWin;
	int32_t lDraw;
	int32_t lLoss;
} TPacketGuildLadder;

typedef struct SPacketGuildLadderPoint
{
	uint32_t dwGuild;
	int32_t lChange;
} TPacketGuildLadderPoint;

typedef struct SPacketGDSMS
{
	char szFrom[CHARACTER_NAME_MAX_LEN + 1];
	char szTo[CHARACTER_NAME_MAX_LEN + 1];
	char szMobile[MOBILE_MAX_LEN + 1];
	char szMsg[SMS_MAX_LEN + 1];
} TPacketGDSMS;

typedef struct SPacketGuildUseSkill
{
	uint32_t dwGuild;
	uint32_t dwSkillVnum;
	uint32_t dwCooltime;
} TPacketGuildUseSkill;

typedef struct SPacketGuildSkillUsableChange
{
	uint32_t dwGuild;
	uint32_t dwSkillVnum;
	uint8_t bUsable;
} TPacketGuildSkillUsableChange;

typedef struct SPacketGDLoginKey
{
	uint32_t dwAccountID;
	uint32_t dwLoginKey;
} TPacketGDLoginKey;

typedef struct SPacketGDAuthLogin
{
	uint32_t	dwID;
#ifdef ENABLE_HWID
	char hwid[HWID_LENGTH + 1];
#endif
	uint32_t	dwLoginKey;
	char	szLogin[LOGIN_MAX_LEN + 1];
	char	szSocialID[SOCIAL_ID_MAX_LEN + 1];
#ifdef ENABLE_MULTILANGUAGE_SYSTEM
	char	szLanguage[LANGUAGE_MAX_LEN + 1];
#endif
	uint32_t	adwClientKey[4];
	uint8_t	bBillType;
	uint32_t	dwBillID;
#ifdef ENABLE_MULTI_LANGUAGE
	uint8_t	bLanguage;
#endif	
	int		iPremiumTimes[PREMIUM_MAX_NUM];
} TPacketGDAuthLogin;

typedef struct SPacketGDLoginByKey
{
	char	szLogin[LOGIN_MAX_LEN + 1];
	uint32_t	dwLoginKey;
	uint32_t	adwClientKey[4];
	char	szIP[MAX_HOST_LENGTH + 1];
} TPacketGDLoginByKey;

/**
 * @version 05/06/08	Bang2ni - 지속시간 추가
 */
typedef struct SPacketGiveGuildPriv
{
	uint8_t type;
	int value;
	uint32_t guild_id;
	time_t duration_sec;	///< 지속시간
} TPacketGiveGuildPriv;
typedef struct SPacketGiveEmpirePriv
{
	uint8_t type;
	int value;
	uint8_t empire;
	time_t duration_sec;
} TPacketGiveEmpirePriv;
typedef struct SPacketGiveCharacterPriv
{
	uint8_t type;
	int value;
	uint32_t pid;
} TPacketGiveCharacterPriv;
typedef struct SPacketRemoveGuildPriv
{
	uint8_t type;
	uint32_t guild_id;
} TPacketRemoveGuildPriv;
typedef struct SPacketRemoveEmpirePriv
{
	uint8_t type;
	uint8_t empire;
} TPacketRemoveEmpirePriv;

typedef struct SPacketDGChangeCharacterPriv
{
	uint8_t type;
	int value;
	uint32_t pid;
	uint8_t bLog;
} TPacketDGChangeCharacterPriv;

/**
 * @version 05/06/08	Bang2ni - 지속시간 추가
 */
typedef struct SPacketDGChangeGuildPriv
{
	uint8_t type;
	int value;
	uint32_t guild_id;
	uint8_t bLog;
	time_t end_time_sec;	///< 지속시간
} TPacketDGChangeGuildPriv;

typedef struct SPacketDGChangeEmpirePriv
{
	uint8_t type;
	int value;
	uint8_t empire;
	uint8_t bLog;
	time_t end_time_sec;
} TPacketDGChangeEmpirePriv;

typedef struct SPacketMoneyLog
{
	uint8_t type;
	uint32_t vnum;
	int64_t gold;

} TPacketMoneyLog;

typedef struct SPacketGDGuildMoney
{
	uint32_t dwGuild;
	int64_t iGold;
} TPacketGDGuildMoney;

typedef struct SPacketDGGuildMoneyChange
{
	uint32_t dwGuild;
	int64_t iTotalGold;
} TPacketDGGuildMoneyChange;

typedef struct SPacketDGGuildMoneyWithdraw
{
	uint32_t dwGuild;
	int64_t iChangeGold;
} TPacketDGGuildMoneyWithdraw;

typedef struct SPacketGDGuildMoneyWithdrawGiveReply
{
	uint32_t dwGuild;
	int64_t iChangeGold;
	uint8_t bGiveSuccess;
} TPacketGDGuildMoneyWithdrawGiveReply;

typedef struct SPacketSetEventFlag
{
	char	szFlagName[EVENT_FLAG_NAME_MAX_LEN + 1];
	int32_t	lValue;
} TPacketSetEventFlag;

typedef struct SPacketBillingLogin
{
	uint32_t	dwLoginKey;
	uint8_t	bLogin;
} TPacketBillingLogin;

typedef struct SPacketBillingRepair
{
	uint32_t	dwLoginKey;
	char	szLogin[LOGIN_MAX_LEN + 1];
	char	szHost[MAX_HOST_LENGTH + 1];
} TPacketBillingRepair;

typedef struct SPacketBillingExpire
{
	char	szLogin[LOGIN_MAX_LEN + 1];
	uint8_t	bBillType;
	uint32_t	dwRemainSeconds;
} TPacketBillingExpire;

typedef struct SPacketLoginOnSetup
{
	uint32_t   dwID;
#ifdef ENABLE_HWID
	char hwid[HWID_LENGTH + 1];
#endif
	char    szLogin[LOGIN_MAX_LEN + 1];
	char    szSocialID[SOCIAL_ID_MAX_LEN + 1];
#ifdef ENABLE_MULTILANGUAGE_SYSTEM
	char	szLanguage[LANGUAGE_MAX_LEN + 1];
#endif
	char    szHost[MAX_HOST_LENGTH + 1];
	uint32_t   dwLoginKey;
	uint32_t   adwClientKey[4];
#ifdef ENABLE_MULTI_LANGUAGE
	uint8_t	bLanguage;
#endif
} TPacketLoginOnSetup;

typedef struct SPacketGDCreateObject
{
	uint32_t	dwVnum;
	uint32_t	dwLandID;
	int32_t		lMapIndex;
	int	 	x, y;
	float	xRot;
	float	yRot;
	float	zRot;
} TPacketGDCreateObject;

typedef struct SPacketGDHammerOfTor
{
	uint32_t 	key;
	uint32_t	delay;
} TPacketGDHammerOfTor;

typedef struct SPacketGDVCard
{
	uint32_t	dwID;
	char	szSellCharacter[CHARACTER_NAME_MAX_LEN + 1];
	char	szSellAccount[LOGIN_MAX_LEN + 1];
	char	szBuyCharacter[CHARACTER_NAME_MAX_LEN + 1];
	char	szBuyAccount[LOGIN_MAX_LEN + 1];
} TPacketGDVCard;

typedef struct SGuildReserve
{
	uint32_t       dwID;
	uint32_t       dwGuildFrom;
	uint32_t       dwGuildTo;
	uint32_t       dwTime;
	uint8_t        bType;
	int32_t        lWarPrice;
	int32_t        lInitialScore;
	bool        bStarted;
	uint32_t	dwBetFrom;
	uint32_t	dwBetTo;
	int32_t	lPowerFrom;
	int32_t	lPowerTo;
	int32_t	lHandicap;
} TGuildWarReserve;

typedef struct
{
	uint32_t	dwWarID;
	char	szLogin[LOGIN_MAX_LEN + 1];
	uint32_t	dwGold;
	uint32_t	dwGuild;
} TPacketGDGuildWarBet;

// Marriage

typedef struct
{
	uint32_t dwPID1;
	uint32_t dwPID2;
	time_t tMarryTime;
	char szName1[CHARACTER_NAME_MAX_LEN + 1];
	char szName2[CHARACTER_NAME_MAX_LEN + 1];
} TPacketMarriageAdd;

typedef struct
{
	uint32_t dwPID1;
	uint32_t dwPID2;
	int  iLovePoint;
	uint8_t  byMarried;
} TPacketMarriageUpdate;

typedef struct
{
	uint32_t dwPID1;
	uint32_t dwPID2;
} TPacketMarriageRemove;

typedef struct
{
	uint32_t dwPID1;
	uint32_t dwPID2;
} TPacketWeddingRequest;

typedef struct
{
	uint32_t dwPID1;
	uint32_t dwPID2;
	uint32_t dwMapIndex;
} TPacketWeddingReady;

typedef struct
{
	uint32_t dwPID1;
	uint32_t dwPID2;
} TPacketWeddingStart;

typedef struct
{
	uint32_t dwPID1;
	uint32_t dwPID2;
} TPacketWeddingEnd;

/// 개인상점 가격정보의 헤더. 가변 패킷으로 이 뒤에 byCount 만큼의 TItemPriceInfo 가 온다.
typedef struct SPacketMyshopPricelistHeader
{
	uint32_t	dwOwnerID;	///< 가격정보를 가진 플레이어 ID
	uint8_t	byCount;	///< 가격정보 갯수
} TPacketMyshopPricelistHeader;

/// 개인상점의 단일 아이템에 대한 가격정보
typedef struct SItemPriceInfo
{
	uint32_t	dwVnum;		///< 아이템 vnum
	uint32_t	dwPrice;	///< 가격
} TItemPriceInfo;

/// 개인상점 아이템 가격정보 리스트 테이블
typedef struct SItemPriceListTable
{
	uint32_t	dwOwnerID;	///< 가격정보를 가진 플레이어 ID
	uint8_t	byCount;	///< 가격정보 리스트의 갯수

	TItemPriceInfo	aPriceInfo[SHOP_PRICELIST_MAX_NUM];	///< 가격정보 리스트
} TItemPriceListTable;

typedef struct
{
	char szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t lDuration;
} TPacketBlockChat;

// PCBANG_IP_LIST
typedef struct SPacketPCBangIP
{
	uint32_t id;
	uint32_t ip;
} TPacketPCBangIP;
// END_OF_PCBANG_IP_LIST


//ADMIN_MANAGER
typedef struct TAdminInfo
{
	int m_ID;				//고유ID
	char m_szAccount[32];	//계정
	char m_szName[32];		//캐릭터이름
	char m_szContactIP[16];	//접근아이피
	char m_szServerIP[16];  //서버아이피
	int m_Authority;		//권한
} tAdminInfo;
//END_ADMIN_MANAGER

//BOOT_LOCALIZATION
struct tLocale
{
	char szValue[32];
	char szKey[32];
};
//BOOT_LOCALIZATION

//RELOAD_ADMIN
typedef struct SPacketReloadAdmin
{
	char szIP[16];
} TPacketReloadAdmin;
//END_RELOAD_ADMIN

typedef struct tChangeGuildMaster
{
	uint32_t dwGuildID;
	uint32_t idFrom;
	uint32_t idTo;
} TPacketChangeGuildMaster;

typedef struct tItemIDRange
{
	uint32_t dwMin;
	uint32_t dwMax;
	uint32_t dwUsableItemIDMin;
} TItemIDRangeTable;

typedef struct tUpdateHorseName
{
	uint32_t dwPlayerID;
	char szHorseName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketUpdateHorseName;

typedef struct tDC
{
	char	login[LOGIN_MAX_LEN + 1];
} TPacketDC;

typedef struct tNeedLoginLogInfo
{
	uint32_t dwPlayerID;
} TPacketNeedLoginLogInfo;

typedef struct tItemAwardInformer
{
	char	login[LOGIN_MAX_LEN + 1];
	char	command[20];
	uint32_t vnum;
} TPacketItemAwardInfromer;

typedef struct tDeleteAwardID
{
	uint32_t dwID;
} TPacketDeleteAwardID;

typedef struct SChannelStatus
{
	short nPort;
	uint8_t bStatus;
} TChannelStatus;
#ifdef __ENABLE_NEW_OFFLINESHOP__
//common
typedef struct {
	uint8_t bSubHeader;
} TPacketGDNewOfflineShop;


typedef struct {
	uint8_t bSubHeader;
} TPacketDGNewOfflineShop;


namespace offlineshop
{
	//patch 08-03-2020
	enum class ExpirationType {
		EXPIRE_NONE,
		EXPIRE_REAL_TIME,
		EXPIRE_REAL_TIME_FIRST_USE,
	};

	typedef struct SPriceInfo
	{
		int64_t	illYang;
#ifdef __ENABLE_CHEQUE_SYSTEM__
		int iCheque;
#endif

		SPriceInfo() : illYang(0)
#ifdef __ENABLE_CHEQUE_SYSTEM__
			,iCheque(0)
#endif
		{}

		bool operator < (const SPriceInfo& rItem) const
		{
			return GetTotalYangAmount() < rItem.GetTotalYangAmount();
		}

		int64_t GetTotalYangAmount() const{
			int64_t total = illYang;
#ifdef __ENABLE_CHEQUE_SYSTEM__
			total += (long long) YANG_PER_CHEQUE * (long long) iCheque;
#endif
			return total;
		}

	} TPriceInfo;

	typedef struct
	{
		uint32_t	dwVnum;
		uint32_t	dwCount;
		int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
		TPlayerItemAttribute    aAttr[ITEM_ATTRIBUTE_MAX_NUM];

#ifdef __ENABLE_CHANGELOOK_SYSTEM__
		uint32_t	dwTransmutation;
#endif
#ifdef ATTR_LOCK
		int     iLockedAttr;
#endif
		//patch 08-03-2020
		ExpirationType	expiration;

	} TItemInfoEx;

	typedef struct
	{
		uint32_t		dwOwnerID, dwItemID;
		TPriceInfo	price;
		TItemInfoEx	item;
	} TItemInfo;

	typedef struct {
		uint32_t		dwOfferID, dwOwnerID, dwItemID, dwOffererID;
		TPriceInfo	price;
		bool		bNoticed, bAccepted;

		//offlineshop-updated 03/08/19
		char		szBuyerName[CHARACTER_NAME_MAX_LEN+1];

	} TOfferInfo;

	//AUCTION
	typedef struct {
		uint32_t dwOwnerID;
		char  szOwnerName[CHARACTER_NAME_MAX_LEN + 1];
		uint32_t dwDuration;

		TPriceInfo	init_price;
		TItemInfoEx item;
	} TAuctionInfo;


	typedef struct {
		TPriceInfo	price;
		uint32_t		dwOwnerID;
		uint32_t		dwBuyerID;

		char		szBuyerName[CHARACTER_NAME_MAX_LEN + 1];
	} TAuctionOfferInfo;

	typedef struct SValutesInfoa
	{
		int64_t	illYang;
#ifdef __ENABLE_CHEQUE_SYSTEM__
		int 		iCheque;
#endif

		void operator +=(const SValutesInfoa& r)
		{
			illYang += r.illYang;
#ifdef __ENABLE_CHEQUE_SYSTEM__
			iCheque += r.iCheque;
#endif
		}

		void operator -=(const SValutesInfoa& r)
		{
			illYang -= r.illYang;
#ifdef __ENABLE_CHEQUE_SYSTEM__
			iCheque -= r.iCheque;
#endif
		}

		SValutesInfoa() : illYang(0)
#ifdef __ENABLE_CHEQUE_SYSTEM__
			, iCheque(0)
#endif
		{}

	} TValutesInfo;


	typedef struct {
		uint32_t	dwOwnerID;
		uint32_t	dwDuration;
		char	szName[OFFLINE_SHOP_NAME_MAX_LEN];
#ifdef KASMIR_PAKET_SYSTEM
		uint32_t	dwKasmirNpc;
#endif
		uint32_t	dwCount;
	} TShopInfo;



	// ### GAME TO DB ###

	enum eNewOfflineshopSubHeaderGD
	{
		SUBHEADER_GD_BUY_ITEM = 0,
		SUBHEADER_GD_BUY_LOCK_ITEM,
		SUBHEADER_GD_CANNOT_BUY_LOCK_ITEM, //topatch
		SUBHEADER_GD_EDIT_ITEM,
		SUBHEADER_GD_REMOVE_ITEM,
		SUBHEADER_GD_ADD_ITEM,

		SUBHEADER_GD_SHOP_FORCE_CLOSE,
		SUBHEADER_GD_SHOP_CREATE_NEW,
		SUBHEADER_GD_SHOP_CHANGE_NAME,


		SUBHEADER_GD_OFFER_CREATE,
		SUBHEADER_GD_OFFER_NOTIFIED,
		SUBHEADER_GD_OFFER_ACCEPT,
		SUBHEADER_GD_OFFER_CANCEL,

		SUBHEADER_GD_SAFEBOX_GET_ITEM,
		SUBHEADER_GD_SAFEBOX_GET_VALUTES,
		SUBHEADER_GD_SAFEBOX_ADD_ITEM,

		//AUCTION
		SUBHEADER_GD_AUCTION_CREATE,
		SUBHEADER_GD_AUCTION_ADD_OFFER,
		SUBHEADER_GD_AUCTION_CLOSE,
	};



	typedef struct {
		uint32_t dwOwnerID, dwItemID, dwGuestID;
	} TSubPacketGDBuyItem;


	typedef struct {
		uint32_t dwOwnerID, dwItemID, dwGuestID;
		int64_t TotalPriceSeen;
	} TSubPacketGDLockBuyItem;

	typedef struct SSubPacketGDCannotBuyLockItem //topatch
	{
		uint32_t dwOwnerID, dwItemID;
	} TSubPacketGDCannotBuyLockItem;

	typedef struct {
		uint32_t		dwOwnerID, dwItemID;
		TPriceInfo	priceInfo;
	} TSubPacketGDEditItem;


	typedef struct {
		uint32_t dwOwnerID;
		uint32_t dwItemID;
	} TSubPacketGDRemoveItem;


	typedef struct {
		uint32_t		dwOwnerID;
		TItemInfo	itemInfo;
	} TSubPacketGDAddItem;


	typedef struct {
		uint32_t dwOwnerID;
	} TSubPacketGDShopForceClose;


	typedef struct {
		TShopInfo shop;
	} TSubPacketGDShopCreateNew;


	typedef struct {
		uint32_t	dwOwnerID;
		char	szName[OFFLINE_SHOP_NAME_MAX_LEN];
	} TSubPacketGDShopChangeName;


	typedef struct {
		uint32_t dwOwnerID, dwItemID;
		TOfferInfo offer;
	} TSubPacketGDOfferCreate;


	typedef struct {
		uint32_t dwOfferID;
		uint32_t dwOwnerID;
	}TSubPacketGDOfferCancel;


	typedef struct {
		uint32_t dwOwnerID, dwOfferID;
	} TSubPacketGDOfferNotified;


	typedef struct {
		uint32_t dwOwnerID, dwOfferID;
	} TSubPacketGDOfferAccept;


	typedef struct {
		uint32_t			dwOwnerID;
		uint32_t			dwItemID;
	} TSubPacketGDSafeboxGetItem;


	typedef struct {
		uint32_t			dwOwnerID;
		TItemInfoEx		item;
	} TSubPacketGDSafeboxAddItem;



	typedef struct {
		uint32_t			dwOwnerID;
		TValutesInfo	valute;
	} TSubPacketGDSafeboxGetValutes;


	//AUCTION
	typedef struct 
	{
		TAuctionInfo auction;
	}TSubPacketGDAuctionCreate;

	typedef struct 
	{
		TAuctionOfferInfo offer;
	}TSubPacketGDAuctionAddOffer;

	typedef struct  {
		uint32_t dwOwnerID;
	}TSubPacketGDAuctionClose;



	// ### DB TO GAME

	enum eSubHeaderDGNewOfflineshop
	{
		SUBHEADER_DG_BUY_ITEM,
		SUBHEADER_DG_LOCKED_BUY_ITEM,
		SUBHEADER_DG_EDIT_ITEM,
		SUBHEADER_DG_REMOVE_ITEM,
		SUBHEADER_DG_ADD_ITEM,

		SUBHEADER_DG_SHOP_FORCE_CLOSE,
		SUBHEADER_DG_SHOP_CREATE_NEW,
		SUBHEADER_DG_SHOP_CHANGE_NAME,
		SUBHEADER_DG_SHOP_EXPIRED,


		SUBHEADER_DG_OFFER_CREATE,
		SUBHEADER_DG_OFFER_NOTIFIED,
		SUBHEADER_DG_OFFER_ACCEPT,
		SUBHEADER_DG_OFFER_CANCEL,

		SUBHEADER_DG_LOAD_TABLES,

		SUBHEADER_DG_SAFEBOX_ADD_ITEM,
		SUBHEADER_DG_SAFEBOX_ADD_VALUTES,
		SUBHEADER_DG_SAFEBOX_LOAD,
		//patch 08-03-2020
		SUBHEADER_DG_SAFEBOX_EXPIRED_ITEM,

		//AUCTION
		SUBHEADER_DG_AUCTION_CREATE,
		SUBHEADER_DG_AUCTION_ADD_OFFER,
		SUBHEADER_DG_AUCTION_EXPIRED,
	};


	typedef struct {
		uint32_t dwOwnerID, dwItemID, dwBuyerID;
	} TSubPacketDGBuyItem;

	typedef struct {
		uint32_t dwOwnerID, dwItemID, dwBuyerID;
	} TSubPacketDGLockedBuyItem;


	typedef struct {
		uint32_t		dwOwnerID, dwItemID;
		TPriceInfo	price;
	} TSubPacketDGEditItem;


	typedef struct {
		uint32_t dwOwnerID, dwItemID;
	} TSubPacketDGRemoveItem;


	typedef struct {
		uint32_t		dwOwnerID, dwItemID;
		TItemInfo	item;
	} TSubPacketDGAddItem;


	typedef struct {
		uint32_t dwOwnerID;
	} TSubPacketDGShopForceClose;


	typedef struct {
		TShopInfo shop;
	} TSubPacketDGShopCreateNew;



	typedef struct {
		uint32_t dwOwnerID;
		char  szName[OFFLINE_SHOP_NAME_MAX_LEN];
	} TSubPacketDGShopChangeName;


	typedef struct {
		uint32_t		dwOwnerID, dwItemID;
		TOfferInfo	offer;
	} TSubPacketDGOfferCreate;


	typedef struct {
		uint32_t dwOfferID;
		uint32_t dwOwnerID;

		//offlineshop-updated 05/08/19
		bool  IsRemovingItem;

	}TSubPacketDGOfferCancel;



	typedef struct {
		uint32_t dwOwnerID, dwOfferID;
	} TSubPacketDGOfferNotified;

	typedef struct {
		uint32_t dwOwnerID, dwOfferID;
	} TSubPacketDGOfferAccept;

	typedef struct {
		uint32_t	dwShopCount;
		uint32_t	dwOfferCount;
		uint32_t	dwAuctionCount;
		uint32_t	dwAuctionOfferCount;

	} TSubPacketDGLoadTables;


	typedef struct {
		uint32_t dwOwnerID;
	} TSubPacketDGShopExpired;


	typedef struct {
		uint32_t dwOwnerID, dwItemID;
		TItemInfoEx item;
	} TSubPacketDGSafeboxAddItem;


	typedef struct {
		uint32_t			dwOwnerID;
		TValutesInfo	valute;
	} TSubPacketDGSafeboxAddValutes;

	typedef struct {
		uint32_t			dwOwnerID;
		TValutesInfo	valute;

		uint32_t			dwItemCount;
	} TSubPacketDGSafeboxLoad;

	//patch 08-03-2020
	typedef struct {
		uint32_t dwOwnerID;
		uint32_t dwItemID;
	} TSubPacketDGSafeboxExpiredItem;


	//AUCTION
	typedef struct 
	{
		TAuctionInfo auction;
	}TSubPacketDGAuctionCreate;

	typedef struct 
	{
		TAuctionOfferInfo offer;
	}TSubPacketDGAuctionAddOffer;

	typedef struct
	{
		uint32_t dwOwnerID;
	}TSubPacketDGAuctionExpired;

}

#endif

#ifdef ENABLE_SWITCHBOT
struct TSwitchbotAttributeAlternativeTable
{
	TPlayerItemAttribute attributes[MAX_NORM_ATTR_NUM];

	bool IsConfigured() const
	{
		for (const auto& it : attributes)
		{
			if (it.bType && it.sValue)
			{
				return true;
			}
		}

		return false;
	}
};

struct TSwitchbotTable
{
	uint32_t player_id;
	bool active[SWITCHBOT_SLOT_COUNT];
	bool finished[SWITCHBOT_SLOT_COUNT];
	uint32_t items[SWITCHBOT_SLOT_COUNT];
	TSwitchbotAttributeAlternativeTable alternatives[SWITCHBOT_SLOT_COUNT][SWITCHBOT_ALTERNATIVE_COUNT];

	TSwitchbotTable() : player_id(0)
	{
		memset(&items, 0, sizeof(items));
		memset(&alternatives, 0, sizeof(alternatives));
		memset(&active, false, sizeof(active));
		memset(&finished, false, sizeof(finished));
	}
};

struct TSwitchbottAttributeTable
{
	uint8_t attribute_set;
	int apply_num;
	int32_t max_value;
};
#endif

#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
typedef struct
{
	int32_t lMapIndex;
	int channel;
} TPacketChangeChannel;

typedef struct
{
	int32_t lAddr;
	uint16_t port;
} TPacketReturnChannel;
#endif

#ifdef ENABLE_ITEM_EXTRA_PROTO
typedef struct {
	uint32_t dwVnum;

#ifdef ENABLE_RARITY_SYSTEM
	int iRarity;
#endif
#ifdef ENABLE_NEW_EXTRA_BONUS
	TItemApply ExtraBonus[NEW_EXTRA_BONUS_COUNT];
#endif
} TItemExtraProto;


typedef struct {
	uint32_t dwCount;
	uint32_t dwTableSize;
} TPacketDGLoadItemExtraProto;

#endif

#ifdef __SKILL_COLOR_SYSTEM__
typedef struct
{
	uint32_t	player_id;
	uint32_t	dwSkillColor[ESkillColorLength::MAX_SKILL_COUNT + ESkillColorLength::MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
} TSkillColor;
#endif

#ifdef ENABLE_BATTLE_PASS
typedef struct SPlayerBattlePassMission
{
	uint32_t dwPlayerId;
	uint32_t dwMissionId;
	uint32_t dwBattlePassId;
	uint32_t dwExtraInfo;
	uint8_t bCompleted;
	uint8_t bIsUpdated;
} TPlayerBattlePassMission;

typedef struct SBattlePassRewardItem
{
	uint32_t	dwVnum;
	int	bCount;
} TBattlePassRewardItem;

typedef struct SBattlePassMissionInfo
{
	uint8_t	bMissionType;
	uint32_t	dwMissionInfo[3];
	TBattlePassRewardItem aRewardList[MISSION_REWARD_COUNT];
} TBattlePassMissionInfo;

typedef struct SBattlePassRanking
{
	uint8_t	bPos;
	char	playerName[CHARACTER_NAME_MAX_LEN + 1];
	uint32_t	dwFinishTime;
} TBattlePassRanking;

typedef struct SBattlePassRegisterRanking
{
	uint8_t	bBattlePassId;
	char	playerName[CHARACTER_NAME_MAX_LEN + 1];
} TBattlePassRegisterRanking;
#endif

#ifdef ENABLE_BATTLE_PASS_SECURITY_KILL
typedef struct SBattlePassKillVictim {
	uint32_t	dwVictimPid;
	uint32_t	dwLastKillTime;
} TBattlePassKillVictim;
#endif

#if defined(BL_OFFLINE_MESSAGE)
typedef struct
{
	char 	szName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGDReadOfflineMessage;

typedef struct
{
	char	szFrom[CHARACTER_NAME_MAX_LEN + 1];
	char	szMessage[CHAT_MAX_LEN + 1];
} TPacketDGReadOfflineMessage;

typedef struct
{
	char	szFrom[CHARACTER_NAME_MAX_LEN + 1];
	char	szTo[CHARACTER_NAME_MAX_LEN + 1];
	char	szMessage[CHAT_MAX_LEN + 1];
} TPacketGDSendOfflineMessage;
#endif

#ifdef ENABLE_HWID
typedef struct {
	char whoname[CHARACTER_NAME_MAX_LEN + 1];
	char targetname[CHARACTER_NAME_MAX_LEN + 1];
} THwidRequest;
#endif

#ifdef ENABLE_EVENT_MANAGER
typedef struct event_struct_
{
	uint16_t	eventID;
	uint8_t	eventIndex;
	int		startTime;
	int		endTime;
	uint8_t	empireFlag;
	uint8_t	channelFlag;
	uint32_t	value[4];
	bool	eventStatus;
	bool	eventTypeOnlyStart;
	char	startTimeText[25];
	char	endTimeText[25];
}TEventManagerData;
enum
{
	EVENT_MANAGER_LOAD,
	EVENT_MANAGER_EVENT_STATUS,
	EVENT_MANAGER_REMOVE_EVENT,
	EVENT_MANAGER_UPDATE,

	BONUS_EVENT = 1,
	DOUBLE_BOSS_LOOT_EVENT = 2,
	DOUBLE_METIN_LOOT_EVENT = 3,
	DOUBLE_MISSION_BOOK_EVENT = 4,
	DUNGEON_COOLDOWN_EVENT = 5,
	DUNGEON_TICKET_LOOT_EVENT = 6,
	EMPIRE_WAR_EVENT = 7,
	MOONLIGHT_EVENT = 8,
	TOURNAMENT_EVENT = 9,
	WHELL_OF_FORTUNE_EVENT = 10,
	HALLOWEEN_EVENT = 11,
	NPC_SEARCH_EVENT = 12,
	EXP_EVENT = 13,
	ITEM_DROP_EVENT = 14,
	YANG_DROP_EVENT = 15,
	HATSZOG_LADA_EVENT = 16,
	BUPLA_RUN_BOSS_LOOT_EVENT = 17,
	MIKI_EVENT = 18,
	KARI_EVENT = 19,
	DUPLA_SZILI_EVENT = 20,
	COIN_EVENT = 21,
	DUPLA_BOSS_PONT_EVENT = 22,
	DUPLA_RUN_PONT_EVENT = 23,
	VALENTIN_EVENT = 24,
	TANAKA_EVENT = 25,
	GOLDEN_FROG_EVENT = 26,
	EASTER_EVENT = 27,
	LELEKGOMB_EVENT = 28,





};
#endif


#ifdef ENABLE_ITEMSHOP
enum
{
	ITEMSHOP_LOAD,
	ITEMSHOP_LOG,
	ITEMSHOP_BUY,
	ITEMSHOP_DRAGONCOIN,
	ITEMSHOP_RELOAD,
};
typedef struct SIShopData
{
	uint32_t	id;
	uint32_t	itemVnum;
	uint32_t	itemPrice;
	int		topSellingIndex;
	uint8_t	discount;
	int64_t		offerTime;
	int64_t		addedTime;
	uint32_t	sellCount;
	int	week_limit;
	int	month_limit;
	int32_t alSocket[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
}TIShopData;
typedef struct SIShopLogData
{
	uint32_t	accountID;
	char	playerName[CHARACTER_NAME_MAX_LEN + 1];
	char	buyDate[21];
	int		buyTime;
	char	ipAdress[16];
	uint32_t	itemVnum;
	int		itemCount;
	uint32_t	itemPrice;
}TIShopLogData;
#endif

#pragma pack()
#endif
