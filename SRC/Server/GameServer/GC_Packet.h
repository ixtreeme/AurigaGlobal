#pragma once
#include <GameServer/packet.h>
#include "../CommonDefines.h"
#include "../service.h"

#pragma pack(1)
enum ECharacterEquipmentPart
{
	CHR_EQUIPPART_ARMOR,
	CHR_EQUIPPART_WEAPON,
	CHR_EQUIPPART_HEAD,
	CHR_EQUIPPART_HAIR,
#ifdef ENABLE_ACCE_SYSTEM
	CHR_EQUIPPART_ACCE,
#endif
#ifdef ENABLE_COSTUME_EFFECT
	CHR_EQUIPPART_EFFECT_BODY,
	CHR_EQUIPPART_EFFECT_WEAPON,
#endif
#ifdef ENABLE_RUNE_SYSTEM
	CHR_EQUIPPART_RUNE,
#endif
	CHR_EQUIPPART_NUM,
};

#ifdef ENABLE_RANKING
enum
{
	MAX_RANKING_LIST = 51,

};
#endif

#ifdef LEADERBOARD_RAZOR93
//enum
//{
//	HEADER_GC_LEADERBOARD_DATA = 351,
//};

typedef struct SPacketGCLeaderboardData
{
	uint8_t header;
	char data[2048];
} TPacketGCLeaderboard;

typedef struct SPacketGCLeaderboardNews
{
	uint8_t header;
	char data[2048];
} TPacketGCLeaderboardNews;
#endif

typedef struct packet_quest_confirm
{
	uint8_t header;
	char msg[64 + 1];
	int32_t timeout;
	uint32_t requestPID;
} TPacketGCQuestConfirm;

typedef struct packet_handshake
{
	uint8_t	bHeader;
	uint32_t	dwHandshake;
	uint32_t	dwTime;
	int32_t	lDelta;
} TPacketGCHandshake;

typedef struct packet_phase
{
	uint8_t	header;
	uint8_t	phase;
} TPacketGCPhase;

typedef struct packet_bindudp
{
	uint8_t	header;
	uint32_t	addr;
	uint16_t	port;
} TPacketGCBindUDP;

typedef struct packet_login_success
{
	uint8_t		bHeader;
	TSimplePlayer	players[PLAYER_PER_ACCOUNT];
	uint32_t		guild_id[PLAYER_PER_ACCOUNT];
	char		guild_name[PLAYER_PER_ACCOUNT][GUILD_NAME_MAX_LEN + 1];

	uint32_t		handle;
	uint32_t		random_key;
} TPacketGCLoginSuccess;

typedef struct packet_auth_success
{
	uint8_t	bHeader;
	uint32_t	dwLoginKey;
	uint8_t	bResult;
} TPacketGCAuthSuccess;

typedef struct packet_auth_success_openid
{
	uint8_t	bHeader;
	uint32_t	dwLoginKey;
	uint8_t	bResult;
	char	login[LOGIN_MAX_LEN + 1];
} TPacketGCAuthSuccessOpenID;

typedef struct packet_login_failure
{
	uint8_t	header;
	char	szStatus[ACCOUNT_STATUS_MAX_LEN + 1];
} TPacketGCLoginFailure;

typedef struct packet_create_failure
{
	uint8_t	header;
	uint8_t	bType;
} TPacketGCCreateFailure;

typedef struct command_player_create_success
{
	uint8_t			header;
	uint8_t			bAccountCharacterIndex;
	TSimplePlayer	player;
} TPacketGCPlayerCreateSuccess;


typedef struct packet_add_char
{
	uint8_t	header;
	uint32_t	dwVID;
	float	angle;
	int32_t	x;
	int32_t	y;
	int32_t	z;
	uint8_t	bType;
	uint16_t	wRaceNum;
	uint16_t	bMovingSpeed;
	uint16_t	bAttackSpeed;
	uint8_t	bStateFlag;
	uint32_t	dwAffectFlag[2];
#ifdef ENABLE_MULTI_NAMES
	bool transname;
#endif
} TPacketGCCharacterAdd;

typedef struct packet_char_additional_info
{
	uint8_t    header;
	uint32_t   dwVID;
	char    name[CHARACTER_NAME_MAX_LEN + 1];
	uint16_t    awPart[CHR_EQUIPPART_NUM];
	uint8_t	bEmpire;
	uint32_t   dwGuildID;
	uint32_t   dwLevel;
	uint32_t	sAlignment;
	uint8_t	bPKMode;
	uint32_t	dwMountVnum;
#ifdef __SKILL_COLOR_SYSTEM__
	uint32_t	dwSkillColor[ESkillColorLength::MAX_SKILL_COUNT + ESkillColorLength::MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
#endif

#ifdef ENABLE_MULTI_LANGUAGE
	uint8_t	bLanguage;
#endif
} TPacketGCCharacterAdditionalInfo;

typedef struct packet_update_char
{
	uint8_t	header;
	uint32_t	dwVID;

	uint16_t        awPart[CHR_EQUIPPART_NUM];
	uint16_t	bMovingSpeed;
	uint16_t	bAttackSpeed;

	uint8_t	bStateFlag;
	uint32_t	dwAffectFlag[2];

	uint32_t	dwGuildID;
	uint32_t	sAlignment;
	uint8_t	bPKMode;
	uint32_t	dwMountVnum;
#ifdef __SKILL_COLOR_SYSTEM__
	uint32_t	dwSkillColor[ESkillColorLength::MAX_SKILL_COUNT + ESkillColorLength::MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
#endif

#ifdef ENABLE_MULTI_LANGUAGE
	uint8_t	bLanguage;
#endif
} TPacketGCCharacterUpdate;

typedef struct packet_del_char
{
	uint8_t	header;
	uint32_t	id;
} TPacketGCCharacterDelete;

typedef struct packet_chat
{
	uint8_t	header;
	uint16_t	size;
	uint8_t	type;
	uint32_t	id;
	uint8_t	bEmpire;
} TPacketGCChat;

typedef struct packet_whisper
{
	uint8_t	bHeader;
	uint16_t	wSize;
	uint8_t	bType;
	char	szNameFrom[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGCWhisper;

#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
typedef struct SPacketGCItemOnTitleNameUpdate
{
	uint8_t header;
	uint32_t dwVID;
	char name[CHARACTER_NAME_MAX_LEN + 64];
} TPacketGCItemOnTitleNameUpdate;
#endif

typedef struct packet_main_character
{
	uint8_t        header;
	uint32_t	dwVID;
	uint16_t	wRaceNum;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t	lx, ly, lz;
	uint8_t	empire;
	uint8_t	skill_group;
} TPacketGCMainCharacter;

// SUPPORT_BGM
typedef struct packet_main_character3_bgm
{
	enum
	{
		MUSIC_NAME_LEN = 24,
	};

	uint8_t    header;
	uint32_t	dwVID;
	uint16_t	wRaceNum;
	char	szChrName[CHARACTER_NAME_MAX_LEN + 1];
	char	szBGMName[MUSIC_NAME_LEN + 1];
	int32_t	lx, ly, lz;
	uint8_t	empire;
	uint8_t	skill_group;
} TPacketGCMainCharacter3_BGM;

typedef struct packet_main_character4_bgm_vol
{
	enum
	{
		MUSIC_NAME_LEN = 24,
	};

	uint8_t    header;
	uint32_t	dwVID;
	uint16_t	wRaceNum;
	char	szChrName[CHARACTER_NAME_MAX_LEN + 1];
	char	szBGMName[MUSIC_NAME_LEN + 1];
	float	fBGMVol;
	int32_t	lx, ly, lz;
	uint8_t	empire;
	uint8_t	skill_group;
} TPacketGCMainCharacter4_BGM_VOL;
// END_OF_SUPPORT_BGM

typedef struct packet_points
{
	uint8_t	header;

	int64_t	points[POINT_MAX_NUM];

} TPacketGCPoints;

typedef struct packet_skill_level
{
	uint8_t		bHeader;
	TPlayerSkill	skills[SKILL_MAX_NUM];
} TPacketGCSkillLevel;

typedef struct packet_point_change
{
	int		header;
	uint32_t	dwVID;
	uint8_t	type;

	int64_t	amount;
	int64_t	value;

} TPacketGCPointChange;

typedef struct packet_stun
{
	uint8_t	header;
	uint32_t	vid;
} TPacketGCStun;

typedef struct packet_dead
{
	uint8_t	header;
	uint32_t	vid;
} TPacketGCDead;

struct TPacketGCItemDelDeprecated
{
	uint8_t	header;
	TItemPos Cell;
	uint32_t	vnum;
#ifdef ENABLE_NEW_STACK_LIMIT
	int
#else
	uint8_t
#endif
		count;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
};

typedef struct packet_item_set
{
	uint8_t	header;
	TItemPos Cell;
	uint32_t	vnum;
#ifdef ENABLE_NEW_STACK_LIMIT
	int
#else
	uint8_t
#endif
		count;
	uint32_t	flags;
	uint32_t	anti_flags;
	bool	highlight;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
} TPacketGCItemSet;

typedef struct packet_item_del
{
	uint8_t	header;
	uint16_t	pos;
} TPacketGCItemDel;

typedef struct packet_item_update
{
	uint8_t	header;
	TItemPos Cell;
#ifdef ENABLE_NEW_STACK_LIMIT
	int
#else
	uint8_t
#endif
		count;
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
} TPacketGCItemUpdate;

typedef struct packet_item_ground_add
{
	uint8_t	bHeader;
	int32_t 	x, y, z;
	uint32_t	dwVID;
	uint32_t	dwVnum;
} TPacketGCItemGroundAdd;

typedef struct packet_item_ownership
{
	uint8_t	bHeader;
	uint32_t	dwVID;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGCItemOwnership;

typedef struct packet_item_ground_del
{
	uint8_t	bHeader;
	uint32_t	dwVID;
} TPacketGCItemGroundDel;
#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
struct TPacketGCBeltNameUpdate
{
	uint8_t header; // HEADER_GC_BELT_NAME_UPDATE
	uint32_t dwVID; // karakter egyedi azonosítója
	char name[CHARACTER_NAME_MAX_LEN + 64]; // 88 bájt
};

typedef struct packet_mount_count_overhead {
	uint8_t header;
	uint32_t dwVID;
	int mountCount;
} TPacketGCMountCountOverhead;


#endif


struct packet_shop_item
{
	uint32_t vnum;
	int64_t price;

#ifdef ENABLE_NEW_STACK_LIMIT
	int count;
#else
	uint8_t count;
#endif
#ifdef ENABLE_BUY_WITH_ITEM
	TShopItemPrice	itemprice[MAX_SHOP_PRICES];
#endif
	uint8_t display_pos;
	int32_t alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
#ifdef ATTR_LOCK
	short lockedattr;
#endif
};

typedef struct packet_shop_start
{
	uint32_t   owner_vid;
	struct packet_shop_item	items[SHOP_HOST_ITEM_MAX_NUM];
} TPacketGCShopStart;

typedef struct packet_shop_start_ex
{
	typedef struct sub_packet_shop_tab
	{
		char name[SHOP_TAB_NAME_MAX];
		uint8_t coin_type;
		packet_shop_item items[SHOP_HOST_ITEM_MAX_NUM];
	} TSubPacketShopTab;
	uint32_t owner_vid;
	uint8_t shop_tab_count;
} TPacketGCShopStartEx;

typedef struct packet_shop_update_item
{
	uint8_t			pos;
	struct packet_shop_item	item;
} TPacketGCShopUpdateItem;

typedef struct packet_shop_update_price
{
	int64_t		iPrice;

} TPacketGCShopUpdatePrice;

typedef struct packet_shop
{
	uint8_t        header;
	uint16_t	size;
	uint8_t        subheader;
} TPacketGCShop;

struct packet_exchange
{
	uint8_t	header;
	uint8_t	sub_header;
	uint8_t	is_me;

	int64_t arg1;

	TItemPos	arg2;	// cell
	uint32_t	arg3;	// count
#ifdef WJ_ENABLE_TRADABLE_ICON
	TItemPos	arg4;
#endif
	int32_t	alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
};

struct packet_position
{
	uint8_t	header;
	uint32_t	vid;
	uint8_t	position;
};

typedef struct packet_ping
{
	uint8_t	header;
} TPacketGCPing;

struct packet_script
{
	uint8_t	header;
	uint16_t	size;
	uint8_t	skin;
	uint16_t	src_size;
};

typedef struct packet_change_speed
{
	uint8_t		header;
	uint32_t		vid;
	uint16_t		moving_speed;
} TPacketGCChangeSpeed;

struct packet_mount
{
	uint8_t	header;
	uint32_t	vid;
	uint32_t	mount_vid;
	uint8_t	pos;
	uint32_t	x, y;
};

typedef struct packet_move
{
	uint8_t		bHeader;
	uint8_t		bFunc;
	uint8_t		bArg;
	float		bRot;
	uint32_t		dwVID;
	int32_t		lX;
	int32_t		lY;
	uint32_t		dwTime;
	uint32_t		dwDuration;
} TPacketGCMove;

typedef struct packet_ownership
{
	uint8_t		bHeader;
	uint32_t		dwOwnerVID;
	uint32_t		dwVictimVID;
} TPacketGCOwnership;

typedef struct packet_sync_position_element
{
	uint32_t	dwVID;
	int32_t	lX;
	int32_t	lY;
} TPacketGCSyncPositionElement;

typedef struct packet_sync_position
{
	uint8_t	bHeader;
	uint16_t	wSize;
} TPacketGCSyncPosition;

typedef struct packet_fly
{
	uint8_t	bHeader;
	uint8_t	bType;
	uint32_t	dwStartVID;
	uint32_t	dwEndVID;
} TPacketGCCreateFly;

typedef struct packet_fly_targeting
{
	uint8_t		bHeader;
	uint32_t		dwShooterVID;
	uint32_t		dwTargetVID;
	int32_t		x, y;
} TPacketGCFlyTargeting;

typedef struct packet_duel_start
{
	uint8_t	header;
	uint16_t	wSize;
} TPacketGCDuelStart;

typedef struct packet_pvp
{
	uint8_t        bHeader;
	uint32_t       dwVIDSrc;
	uint32_t       dwVIDDst;
	uint8_t        bMode;	// 0 ÀÌ¸é ²û, 1ÀÌ¸é ÄÔ
} TPacketGCPVP;

typedef struct packet_target
{
	uint8_t	header;
	uint32_t	dwVID;
	uint8_t	bHPPercent;
#ifdef __VIEW_TARGET_DECIMAL_HP__
	int64_t		iMinHP;
	int64_t		iMaxHP;
#endif
#ifdef ELEMENT_TARGET
	uint8_t	bElement;
#endif
} TPacketGCTarget;
#ifdef __SEND_TARGET_INFO__

typedef struct packet_target_info
{
	uint8_t	header;
	uint32_t	dwVID;
	uint32_t	race;
	uint32_t	dwVnum;
#ifdef ENABLE_NEW_STACK_LIMIT
	uint32_t	count;
#else
	uint8_t	count;
#endif
} TPacketGCTargetInfo;

#endif
typedef struct packet_warp
{
	uint8_t	bHeader;
	int32_t	lX;
	int32_t	lY;
	uint32_t	lAddr;
	uint16_t	wPort;
} TPacketGCWarp;

typedef struct packet_messenger
{
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
} TPacketGCMessenger;

typedef struct packet_messenger_guild_list
{
	uint8_t connected;
	uint8_t length;
	//char login[LOGIN_MAX_LEN+1];
} TPacketGCMessengerGuildList;

typedef struct packet_messenger_guild_login
{
	uint8_t length;
	//char login[LOGIN_MAX_LEN+1];
} TPacketGCMessengerGuildLogin;

typedef struct packet_messenger_guild_logout
{
	uint8_t length;

	//char login[LOGIN_MAX_LEN+1];
} TPacketGCMessengerGuildLogout;

typedef struct packet_messenger_list_offline
{
	uint8_t connected; // always 0
	uint8_t length;
} TPacketGCMessengerListOffline;

typedef struct packet_messenger_list_online
{
	uint8_t connected; // always 1
	uint8_t length;
} TPacketGCMessengerListOnline;

#if defined(ENABLE_MESSENGER_TEAM) || defined(ENABLE_MESSENGER_HELPER)
typedef struct packet_messenger_team_list_offline
{
	uint8_t	connected;
#ifdef ENABLE_MULTI_LANGUAGE
	char	language[2 + 1];
#endif
	uint8_t	length;
} TPacketGCMessengerTeamListOffline;

typedef struct packet_messenger_team_list_online
{
	uint8_t	connected;
#ifdef ENABLE_MULTI_LANGUAGE
	char	language[2 + 1];
#endif
	uint8_t	length;
} TPacketGCMessengerTeamListOnline;
#endif

typedef struct packet_party_invite
{
	uint8_t	header;
	uint32_t	leader_vid;
} TPacketGCPartyInvite;

typedef struct paryt_parameter
{
	uint8_t	bHeader;
	uint8_t	bDistributeMode;
} TPacketGCPartyParameter;

typedef struct packet_party_add
{
	uint8_t	header;
	uint32_t	pid;
	char	name[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGCPartyAdd;

typedef struct packet_party_update
{
	uint8_t	header;
	uint32_t	pid;
	uint8_t	role;
	uint8_t	percent_hp;
	short	affects[7];
} TPacketGCPartyUpdate;

typedef struct packet_party_remove
{
	uint8_t header;
	uint32_t pid;
} TPacketGCPartyRemove;

typedef struct packet_party_link
{
	uint8_t header;
	uint32_t pid;
	uint32_t vid;
} TPacketGCPartyLink;

typedef struct packet_party_unlink
{
	uint8_t header;
	uint32_t pid;
	uint32_t vid;
} TPacketGCPartyUnlink;

typedef struct packet_empire
{
	uint8_t	bHeader;
	uint8_t	bEmpire;
} TPacketGCEmpire;

typedef struct packet_safebox_money_change
{
	uint8_t	bHeader;
	int32_t	lMoney;
} TPacketGCSafeboxMoneyChange;

typedef struct packet_mount_inventory
{
	uint8_t bHeader;
	uint8_t bWidth;
	uint8_t bHeight;
	uint16_t wCount;
} TPacketGCMountInventory;

typedef struct packet_mount_inventory_item
{
	uint16_t wSlot;
	uint32_t dwVnum;
	uint32_t dwCount;
	int32_t alSockets[ITEM_SOCKET_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_MAX_NUM];
} TMountInventoryItemData;

typedef struct packet_guild
{
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
} TPacketGCGuild;

typedef struct packet_guild_name_t
{
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
	uint32_t	guildID;
#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93
	uint8_t	guildLevel;
#endif
	char	guildName[GUILD_NAME_MAX_LEN];
} TPacketGCGuildName;

typedef struct packet_guild_war
{
	uint32_t	dwGuildSelf;
	uint32_t	dwGuildOpp;
	uint8_t	bType;
	uint8_t 	bWarState;
} TPacketGCGuildWar;

typedef struct packet_mark_idxlist
{
	uint8_t    header;
	uint32_t	bufSize;
	uint16_t	count;

} TPacketGCMarkIDXList;

typedef struct packet_mark_block
{
	uint8_t	header;
	uint32_t	bufSize;
	uint8_t	imgIdx;
	uint32_t	count;

} TPacketGCMarkBlock;

typedef struct packet_symbol_data
{
	uint8_t header;
	uint16_t size;
	uint32_t guild_id;
} TPacketGCGuildSymbolData;

typedef struct packet_fishing
{
	uint8_t header;
	uint8_t subheader;
	uint32_t info;
	uint8_t dir;
} TPacketGCFishing;

typedef struct packet_dungeon
{
	uint8_t bHeader;
	uint16_t size;
	uint8_t subheader;
} TPacketGCDungeon;

typedef struct packet_dungeon_dest_position
{
	int32_t x;
	int32_t y;
} TPacketGCDungeonDestPosition;

typedef struct SPacketGCShopSign
{
	uint8_t	bHeader;
	uint32_t	dwVID;
#ifdef KASMIR_PAKET_SYSTEM
	uint8_t	bShopKasmirTitle;
#endif
	char	szSign[SHOP_SIGN_MAX_LEN + 1];
} TPacketGCShopSign;

#ifdef ENABLE_FAKE_SHOP_HEADER

typedef struct SPacketGCFakeShopSign
{
	uint8_t bHeader;
	uint32_t dwVID;
	int iMountCount;
	int	iBeltCount;
} TPacketGCFakeShopSign;
#endif

typedef struct SPacketGCTime
{
	uint8_t	bHeader;
	uint64_t	time;
} TPacketGCTime;

typedef struct SPacketGCWalkMode
{
	uint8_t	header;
	uint32_t	vid;
	uint8_t	mode;
} TPacketGCWalkMode;

typedef struct SPacketGCChangeSkillGroup
{
	uint8_t        header;
	uint8_t        skill_group;
} TPacketGCChangeSkillGroup;

typedef struct SPacketGCRefineInformaion
{
	uint8_t	header;
	uint8_t	type;
	uint8_t	pos;
	uint32_t	src_vnum;
	uint32_t	result_vnum;
	uint8_t	material_count;
	int		cost;
	int		prob;
	TRefineMaterial materials[REFINE_MATERIAL_MAX_NUM];
} TPacketGCRefineInformation;

typedef struct SPacketGCNPCPosition
{
	uint8_t header;
	uint16_t size;
	uint16_t count;

} TPacketGCNPCPosition;

typedef struct SPacketGCSpecialEffect
{
	uint8_t header;
	uint8_t type;
	uint32_t vid;

} TPacketGCSpecialEffect;

typedef struct SPacketGCChangeName
{
	uint8_t header;
	uint32_t pid;
	char name[CHARACTER_NAME_MAX_LEN + 1];

} TPacketGCChangeName;

typedef struct packet_channel
{
	uint8_t header;
	uint8_t channel;
} TPacketGCChannel;

typedef struct packet_land_list
{
	uint8_t	header;
	uint16_t	size;
} TPacketGCLandList;

typedef struct
{
	uint8_t	bHeader;
	int32_t	lID;
	char	szName[32 + 1];
	uint32_t	dwVID;
	uint8_t	bType;
} TPacketGCTargetCreate;

typedef struct
{
	uint8_t	bHeader;
	int32_t	lID;
	int32_t	lX, lY;
} TPacketGCTargetUpdate;

typedef struct
{
	uint8_t	bHeader;
	int32_t	lID;
} TPacketGCTargetDelete;

typedef struct
{
	uint8_t		bHeader;
	TPacketAffectElement elem;
} TPacketGCAffectAdd;

typedef struct
{
	uint8_t	bHeader;
	uint32_t	dwType;
	uint8_t	bApplyOn;
} TPacketGCAffectRemove;

typedef struct packet_lover_info
{
	uint8_t header;
	char name[CHARACTER_NAME_MAX_LEN + 1];
	uint8_t love_point;
} TPacketGCLoverInfo;

typedef struct packet_love_point_update
{
	uint8_t header;
	uint8_t love_point;
} TPacketGCLovePointUpdate;

typedef struct packet_dig_motion
{
	uint8_t header;
	uint32_t vid;
	uint32_t target_vid;
	uint8_t count;
} TPacketGCDigMotion;

typedef struct packet_damage_info
{
	uint8_t header;
	uint32_t dwVID;
	uint8_t flag;
	int damage;
} TPacketGCDamageInfo;

#define MAX_EFFECT_FILE_NAME 128
typedef struct SPacketGCSpecificEffect
{
	uint8_t header;
	uint32_t vid;
	char effect_file[MAX_EFFECT_FILE_NAME];
} TPacketGCSpecificEffect;

typedef struct SPacketGCDragonSoulRefine
{
	SPacketGCDragonSoulRefine() : header(HEADER_GC_DRAGON_SOUL_REFINE), bSubType(0)
	{
	}

	uint8_t header;
	uint8_t bSubType;
	TItemPos Pos;
} TPacketGCDragonSoulRefine;

typedef struct SPacketGCStateCheck
{
	uint8_t header;
	unsigned long key;
	unsigned long index;
	unsigned char state;
} TPacketGCStateCheck;

typedef struct SPacketGCList
{
	int		iPosition, iRealPosition, iLevel, iPoints;
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGCList;

typedef struct SPacketGCRankingTable {
	SPacketGCRankingTable() : bHeader(HEADER_GC_RANKING_SEND), list{}
	{
	}

	uint8_t 			bHeader;
	TPacketGCList	list[MAX_RANKING_LIST];
} TPacketGCRankingTable;

typedef struct
{
	uint8_t bHeader;
#ifdef __ENABLE_LARGE_DYNAMIC_PACKET__
	int wSize;
#else
	uint16_t wSize;
#endif
	uint8_t bSubHeader;
} TPacketGCNewOfflineshop;

typedef struct {
	uint32_t	dwShopCount;
} TSubPacketGCShopList;


typedef struct {
	offlineshop::TShopInfo	shop;

} TSubPacketGCShopOpen;


typedef struct {
	offlineshop::TShopInfo	shop;
	uint32_t		dwSoldCount;
	uint32_t		dwOfferCount;

} TSubPacketGCShopOpenOwner;



typedef struct {
	uint32_t dwOwnerID;
	uint32_t dwItemID;
}TSubPacketGCShopBuyItemFromSearch;


typedef struct {
	uint32_t dwCount;
} TSubPacketGCShopFilterResult;


typedef struct {
	uint32_t dwOfferCount;

} TSubPacketGCShopOfferList;

typedef struct {
	offlineshop::TAuctionInfo auction;
	uint32_t dwOfferCount;


}TSubPacketGCAuctionOpen;

typedef struct {
	offlineshop::TValutesInfo	valute;
	uint32_t			dwItemCount;

}TSubPacketGCShopSafeboxRefresh;

//AUCTION
typedef struct {
	uint32_t dwCount;
	bool bOwner;
}TSubPacketGCAuctionList;



#ifdef ENABLE_NEW_SHOP_IN_CITIES
typedef struct {
	uint32_t	dwVID;
	char	szName[OFFLINE_SHOP_NAME_MAX_LEN];
	int		iType;

	int32_t 	x, y, z;
#ifdef KASMIR_PAKET_SYSTEM
	uint32_t	dwKasmirNpc;
#endif
} TSubPacketGCInsertShopEntity;


typedef struct {
	uint32_t dwVID;
} TSubPacketGCRemoveShopEntity;
#endif

struct TPacketGCSwitchbot
{
	uint8_t header;
	int size;
	uint8_t subheader;
	uint8_t slot;
};

typedef struct dates_cube_renewal
{
	uint32_t npc_vnum;
	uint32_t index;

	uint32_t vnum_reward;
	int count_reward;

	bool item_reward_stackable;

	uint32_t vnum_material_1;
	int count_material_1;

	uint32_t vnum_material_2;
	int count_material_2;

	uint32_t vnum_material_3;
	int count_material_3;

	uint32_t vnum_material_4;
	int count_material_4;

	uint32_t vnum_material_5;
	int count_material_5;

	int64_t gold;
	int percent;

#ifdef ENABLE_GAYA_SYSTEM
	int gaya;
#endif

#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
	uint32_t allowCopy;
#endif

	char category[100];
}TInfoDateCubeRenewal;

typedef struct packet_receive_cube_renewal
{
	packet_receive_cube_renewal() : header(HEADER_GC_CUBE_RENEWAL), subheader(0), date_cube_renewal()
	{
	}

	uint8_t header;
	uint8_t subheader;
	TInfoDateCubeRenewal date_cube_renewal;
}TPacketGCCubeRenewalReceive;

#ifdef TEXTS_IMPROVEMENT
typedef struct SPacketGCChatNew {
	uint8_t header;
	uint8_t type;
	uint32_t idx;
	uint16_t size;
} TPacketGCChatNew;

#endif

#ifdef ENABLE_BATTLE_PASS

typedef struct SPacketGCBattlePass
{
	uint8_t	bHeader;
	uint16_t	wSize;
	uint16_t	wRewardSize;
} TPacketGCBattlePass;

typedef struct SPacketGCBattlePassUpdate
{
	uint8_t	bHeader;
	uint8_t	bMissionType;
	uint32_t	dwNewProgress;
} TPacketGCBattlePassUpdate;



typedef struct SPacketGCBattlePassRanking
{
	uint8_t	bHeader;
	uint16_t	wSize;
	uint8_t	bIsGlobal;
} TPacketGCBattlePassRanking;
#endif

#ifdef ENABLE_ATLAS_BOSS
typedef struct SPacketGCBossPosition
{
	uint8_t	bHeader;
	uint16_t	wSize;
	uint16_t	wCount;
} TPacketGCBossPosition;

struct TBossPosition
{
	uint8_t	bType;
#ifdef ENABLE_MULTI_NAMES
	uint32_t	szName;
#else
	char	szName[CHARACTER_NAME_MAX_LEN + 1];
#endif
	int32_t	lX;
	int32_t	lY;
	int32_t	lTime;
};
#endif

#ifdef ENABLE_EVENT_MANAGER
typedef struct SPacketGCEventManager
{
	uint8_t	header;
	uint32_t	size;
} TPacketGCEventManager;
#endif
#ifdef ENABLE_ITEMSHOP
typedef struct SPacketGCItemShop
{
	uint8_t	header;
	uint32_t	size;
} TPacketGCItemShop;
#endif




#pragma pack()
