#pragma once

#include "Locale.h"
#include "../Game/RaceData.h"
#include "../Game/ItemData.h"

typedef uint8_t TPacketHeader;

enum
{
	HEADER_CG_HANDSHAKE = 0xf0,
	HEADER_CG_PONG = 0xef,
	HEADER_CG_TIME_SYNC = 0xed,
	HEADER_CG_KEY_AGREEMENT = 0xec,

	HEADER_CG_LOGIN = 0x20,
	HEADER_CG_ATTACK = 49,
	HEADER_CG_CHAT = 3,
	HEADER_CG_PLAYER_CREATE = 0x19,
	HEADER_CG_PLAYER_DESTROY					= 5,
	HEADER_CG_PLAYER_SELECT						= 6,
	HEADER_CG_CHARACTER_MOVE					= 7,
	HEADER_CG_SYNC_POSITION  					= 8,
	
	HEADER_CG_ENTERGAME							= 10,
	HEADER_CG_ITEM_USE							= 11,
	HEADER_CG_ITEM_DROP							= 12,
	HEADER_CG_ITEM_MOVE							= 13,
	HEADER_CG_ITEM_PICKUP						= 15,
	HEADER_CG_QUICKSLOT_ADD                     = 16,
	HEADER_CG_QUICKSLOT_DEL                     = 17,
	HEADER_CG_QUICKSLOT_SWAP                    = 18,
	HEADER_CG_WHISPER							= 19,
	HEADER_CG_ITEM_DROP2                        = 20,
	HEADER_CG_ITEM_DESTROY = 21,
	HEADER_CG_ITEM_DIVISION = 22,
#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	ENVANTER_BLACK = 23,
#endif
	HEADER_CG_ON_CLICK							= 26,
	HEADER_CG_EXCHANGE							= 27,
    HEADER_CG_CHARACTER_POSITION                = 28,
    HEADER_CG_SCRIPT_ANSWER						= 29,
	HEADER_CG_QUEST_INPUT_STRING				= 30,
    HEADER_CG_QUEST_CONFIRM                     = 31,
#ifdef ENABLE_BATTLE_PASS
	HEADER_CG_BATTLE_PASS = 40,
#endif
	HEADER_CG_LOGIN3 = 44,
	HEADER_CG_USE_SKILL = 42,
	HEADER_CG_SHOP								= 50,
	HEADER_CG_FLY_TARGETING						= 51,
	HEADER_CG_ADD_FLY_TARGETING                 = 53,
	HEADER_CG_SHOOT								= 54,
	HEADER_CG_MYSHOP                            = 55,

#ifdef ENABLE_SKILL_COLOR_SYSTEM
	HEADER_CG_SKILL_COLOR						= 56,
#endif
#ifdef ENABLE_OPENSHOP_PACKET
	HEADER_CG_OPENSHOP = 57,
#endif
#ifdef ENABLE_SEND_TARGET_INFO
	HEADER_CG_TARGET_INFO_LOAD = 59,
#endif


	//HEADER_BLANK56								= 56,
	//HEADER_BLANK58								= 58,
	//HEADER_BLANK59								= 59,
	HEADER_CG_ITEM_USE_TO_ITEM					= 60,
    HEADER_CG_TARGET                            = 61,
	//HEADER_BLANK62								= 62,
	//HEADER_BLANK63								= 63,
	//HEADER_BLANK64								= 64,
	HEADER_CG_WARP								= 65,
    HEADER_CG_SCRIPT_BUTTON						= 66,
    HEADER_CG_MESSENGER                         = 67,
	//HEADER_BLANK68								= 68,
    HEADER_CG_MALL_CHECKOUT                     = 69,
    HEADER_CG_SAFEBOX_CHECKIN                   = 70,   // 아이템을 창고에 넣는다.
    HEADER_CG_SAFEBOX_CHECKOUT                  = 71,   // 아이템을 창고로 부터 빼온다.
    HEADER_CG_PARTY_INVITE                      = 72,
    HEADER_CG_PARTY_INVITE_ANSWER               = 73,
    HEADER_CG_PARTY_REMOVE                      = 74,
    HEADER_CG_PARTY_SET_STATE                   = 75,
    HEADER_CG_PARTY_USE_SKILL                   = 76,
    HEADER_CG_SAFEBOX_ITEM_MOVE                 = 77,
	HEADER_CG_PARTY_PARAMETER                   = 78,
	//HEADER_BLANK68								= 79,
	HEADER_CG_GUILD								= 80,
	HEADER_CG_ANSWER_MAKE_GUILD					= 81,
	HEADER_CG_FISHING                           = 82,
	HEADER_CG_GIVE_ITEM                         = 83,

	//HEADER_BLANK84								= 84,
	//HEADER_BLANK85								= 85,
	//HEADER_BLANK86								= 86,
	//HEADER_BLANK87								= 87,
	//HEADER_BLANK88								= 88,
	//HEADER_BLANK89								= 89,
    HEADER_CG_EMPIRE                            = 90,
	//HEADER_BLANK91								= 91,
	//HEADER_BLANK92								= 92,
	//HEADER_BLANK93								= 93,
	//HEADER_BLANK94								= 94,
	//HEADER_BLANK95								= 95,
    HEADER_CG_REFINE                            = 96,
	//HEADER_BLANK97								= 97,
	//HEADER_BLANK98								= 98,
	//HEADER_BLANK99								= 99,

	HEADER_CG_MARK_LOGIN						= 100,
	HEADER_CG_MARK_CRCLIST						= 101,
	HEADER_CG_MARK_UPLOAD						= 102,
	HEADER_CG_MARK_IDXLIST						= 104,

	//HEADER_CG_CRC_REPORT						= 103,

	HEADER_CG_HACK								= 105,
    HEADER_CG_CHANGE_NAME                       = 106,

    HEADER_CG_LOGIN2                            = 109,
	HEADER_CG_DUNGEON							= 110,
	HEADER_CG_GUILD_SYMBOL_UPLOAD				= 112,
	HEADER_CG_GUILD_SYMBOL_CRC					= 113,
	HEADER_CG_SCRIPT_SELECT_ITEM				= 114,
	//HEADER_CG_LOGIN4							= 115,

#ifdef ENABLE_MAP_TELEPORTER
	HEADER_CG_MAP_TELEPORTER = 117,
#endif
#ifdef __ENABLE_NEW_OFFLINESHOP__
	HEADER_CG_NEW_OFFLINESHOP					= 119,
#endif

#ifdef ENABLE_MULTI_LANGUAGE
	HEADER_CG_CHANGE_LANGUAGE = 120,
	HEADER_CG_REQUEST_LANGUAGE = 121,
#endif

#ifdef NEW_PET_SYSTEM
	HEADER_CG_PetSetName = 146,
#endif	
#ifdef ENABLE_SWITCHBOT
	HEADER_CG_SWITCHBOT = 171,
#endif

	HEADER_CG_DRAGON_SOUL_REFINE			= 205,
	HEADER_CG_STATE_CHECKER					= 206,
#ifdef ENABLE_DS_REFINE_ALL
	HEADER_CG_DRAGON_SOUL_REFINE_ALL = 207,
#endif
#ifdef ENABLE_ACCE_SYSTEM
	HEADER_CG_ACCE = 211,
#endif

#ifdef ENABLE_NEW_FISHING_SYSTEM
	HEADER_CG_FISHING_NEW = 216,
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
	HEADER_CG_WHEEL_DESTINY = 219,
#endif
#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
	HEADER_CG_WHISPER_ADMIN			= 220,
#endif

#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
	HEADER_CG_CUBE_RENEWAL 						= 215,
#endif

	
	HEADER_CG_MOUNT_INVENTORY_CHECKIN = 125,
	HEADER_CG_MOUNT_INVENTORY_CHECKOUT = 126,
	HEADER_CG_MOUNT_INVENTORY_ITEM_MOVE = 127,

	HEADER_CG_CLIENT_VERSION = 0xfd,
	HEADER_CG_CLIENT_VERSION2 = 0xf1,
	
	

	/////////////////////////////////////////////////
	// From Server

	HEADER_GC_CHARACTER_ADD						= 1,
	HEADER_GC_CHARACTER_DEL						= 2,
	HEADER_GC_CHARACTER_MOVE					= 3,
	HEADER_GC_CHAT								= 4,
	HEADER_GC_SYNC_POSITION 					= 5,
	HEADER_GC_LOGIN_SUCCESS3					= 6,
	HEADER_GC_LOGIN_FAILURE						= 7,
	HEADER_GC_PLAYER_CREATE_SUCCESS				= 8,
	HEADER_GC_PLAYER_CREATE_FAILURE				= 9,
	HEADER_GC_PLAYER_DELETE_SUCCESS				= 10,
	HEADER_GC_PLAYER_DELETE_WRONG_SOCIAL_ID		= 11,
	// 12
	HEADER_GC_STUN								= 13,
	HEADER_GC_DEAD								= 14,

	HEADER_GC_PLAYER_POINTS						= 16,
	HEADER_GC_PLAYER_POINT_CHANGE				= 17,
	HEADER_GC_CHANGE_SPEED						= 18,
	HEADER_GC_CHARACTER_UPDATE                  = 19,

	HEADER_GC_ITEM_SET							= 20, // 아이템 창에 추가
	HEADER_GC_ITEM_SET2							= 21, // 아이템 창에 추가

	HEADER_GC_ITEM_USE							= 22, // 아이템 사용 (주위 사람들에게 보여주기 위해)
	HEADER_GC_ITEM_DROP							= 23, // 아이템 버리기
	HEADER_GC_ITEM_UPDATE						= 25, // 아이템 수치 업데이트
	HEADER_GC_ITEM_GROUND_ADD					= 26, // 바닥에 아이템 추가
	HEADER_GC_ITEM_GROUND_DEL					= 27, // 바닥에서 아이템 삭제
	HEADER_GC_QUICKSLOT_ADD                     = 28,
	HEADER_GC_QUICKSLOT_DEL                     = 29,
	HEADER_GC_QUICKSLOT_SWAP                    = 30,
	HEADER_GC_ITEM_OWNERSHIP					= 31,
	HEADER_GC_LOGIN_SUCCESS4					= 32,
	HEADER_GC_ITEM_UNBIND_TIME					= 33,
	HEADER_GC_WHISPER							= 34,
#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
	HEADER_GC_BELT_NAME_UPDATE					= 245,
	HEADER_GC_MOUNT_COUNT_OVERHEAD				= 248,
#endif
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
	HEADER_GC_ITEM_ON_TITLE_NAME_UPDATE = 246,
#endif
	HEADER_GC_ALERT								= 35,

	HEADER_GC_MOTION							= 36,



	HEADER_GC_SHOP							    = 38,
	HEADER_GC_SHOP_SIGN							= 39,
#ifdef ENABLE_FAKE_SHOP_HEADER
	HEADER_GC_FAKE_SHOP_SIGN					 = 249,
#endif
#ifdef LEADERBOARD_RAZOR93
	
		HEADER_GC_LEADERBOARD_DATA				= 250,
		HEADER_GC_LEADERBOARD_NEWS				= 251,
		HEADER_GC_LEADERBOARD_GUILD				= 252,
#endif
	// 39 ~ 41 Balnk
	HEADER_GC_DUEL_START						= 40,
	HEADER_GC_PVP								= 41,
	HEADER_GC_EXCHANGE							= 42,
    HEADER_GC_CHARACTER_POSITION                = 43,

	HEADER_GC_PING								= 44,

	HEADER_GC_SCRIPT							= 45,
    HEADER_GC_QUEST_CONFIRM                     = 46,
#ifdef ENABLE_EVENT_MANAGER
	HEADER_GC_EVENT_MANAGER						= 147,
#endif

#ifdef ENABLE_SEND_TARGET_INFO
	HEADER_GC_TARGET_INFO						= 58,
#endif

	HEADER_GC_OWNERSHIP                         = 62,
    HEADER_GC_TARGET                            = 63,
	HEADER_GC_WARP								= 65,
	HEADER_GC_ADD_FLY_TARGETING                 = 69,

	HEADER_GC_CREATE_FLY						= 70,
	HEADER_GC_FLY_TARGETING						= 71,
	HEADER_GC_SKILL_LEVEL						= 72,
	HEADER_GC_SKILL_COOLTIME_END				= 73,
    HEADER_GC_MESSENGER                         = 74,
	HEADER_GC_GUILD								= 75,
	HEADER_GC_SKILL_LEVEL_NEW					= 76,

    HEADER_GC_PARTY_INVITE                      = 77,
    HEADER_GC_PARTY_ADD                         = 78,
    HEADER_GC_PARTY_UPDATE                      = 79,
    HEADER_GC_PARTY_REMOVE                      = 80,

    HEADER_GC_QUEST_INFO                        = 81,
    HEADER_GC_REQUEST_MAKE_GUILD                = 82,
	HEADER_GC_PARTY_PARAMETER                   = 83,

    HEADER_GC_SAFEBOX_MONEY_CHANGE              = 84,
    HEADER_GC_SAFEBOX_SET                       = 85,
    HEADER_GC_SAFEBOX_DEL                       = 86,
    HEADER_GC_SAFEBOX_WRONG_PASSWORD            = 87,
    HEADER_GC_SAFEBOX_SIZE                      = 88,

    HEADER_GC_FISHING                           = 89,

    HEADER_GC_EMPIRE                            = 90,

    HEADER_GC_PARTY_LINK                        = 91,
    HEADER_GC_PARTY_UNLINK                      = 92,
#ifdef ENABLE_ITEMSHOP
		HEADER_GC_ITEMSHOP = 93,
#endif
#ifdef ENABLE_NEW_FISHING_SYSTEM
	HEADER_GC_FISHING_NEW = 223, 
#endif

	HEADER_GC_MOUNT_INVENTORY					= 224,
    HEADER_GC_REFINE_INFORMATION                = 95,

	HEADER_GC_OBSERVER_ADD						= 96,
	HEADER_GC_OBSERVER_REMOVE					= 97,
	HEADER_GC_OBSERVER_MOVE						= 98,
	HEADER_GC_VIEW_EQUIP                        = 99,

	HEADER_GC_MARK_BLOCK						= 100,
	HEADER_GC_MARK_DIFF_DATA                    = 101,
	HEADER_GC_MARK_IDXLIST						= 102,

	//HEADER_GC_SLOW_TIMER						= 105,
    HEADER_GC_TIME                              = 106,
    HEADER_GC_CHANGE_NAME                       = 107,

	HEADER_GC_DUNGEON							= 110,
	HEADER_GC_WALK_MODE							= 111,
	HEADER_GC_CHANGE_SKILL_GROUP				= 112,
	HEADER_GC_MAIN_CHARACTER					= 113,
	HEADER_GC_MAIN_CHARACTER3_BGM				= 137,
	HEADER_GC_MAIN_CHARACTER4_BGM_VOL			= 138,
    HEADER_GC_SEPCIAL_EFFECT                    = 114,
	HEADER_GC_NPC_POSITION						= 115,

 //   HEADER_GC_CHARACTER_UPDATE2                 = 117,
    HEADER_GC_LOGIN_KEY                         = 118,
    HEADER_GC_REFINE_INFORMATION_NEW            = 119,
    HEADER_GC_CHARACTER_ADD2                    = 120,
    HEADER_GC_CHANNEL                           = 121,

    HEADER_GC_MALL_OPEN                         = 122,
	HEADER_GC_TARGET_UPDATE                     = 123,
	HEADER_GC_TARGET_DELETE                     = 124,
	HEADER_GC_TARGET_CREATE_NEW                 = 125,

	HEADER_GC_AFFECT_ADD                        = 126,
	HEADER_GC_AFFECT_REMOVE                     = 127,

    HEADER_GC_MALL_SET                          = 128,
    HEADER_GC_MALL_DEL                          = 129,
	HEADER_GC_LAND_LIST                         = 130,
	HEADER_GC_LOVER_INFO						= 131,
	HEADER_GC_LOVE_POINT_UPDATE					= 132,
	HEADER_GC_GUILD_SYMBOL_DATA					= 133,
    HEADER_GC_DIG_MOTION                        = 134,

	HEADER_GC_DAMAGE_INFO						= 135,
	HEADER_GC_CHAR_ADDITIONAL_INFO				= 136,
    HEADER_GC_AUTH_SUCCESS                      = 150,
#ifdef ENABLE_SWITCHBOT
		HEADER_GC_SWITCHBOT						= 171,
#endif

#ifdef TEXTS_IMPROVEMENT
	HEADER_GC_CHAT_NEW = 155,
#endif



	HEADER_GC_SPECIFIC_EFFECT					= 208,
	HEADER_GC_DRAGON_SOUL_REFINE						= 209,
	HEADER_GC_RESPOND_CHANNELSTATUS				= 210,

	// @fixme007
	HEADER_GC_UNK_213							= 213,
#ifdef __ENABLE_NEW_OFFLINESHOP__
	HEADER_GC_NEW_OFFLINESHOP					= 214,
#endif


#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
	HEADER_GC_CUBE_RENEWAL 						= 217,
#endif
#ifdef ENABLE_ATLAS_BOSS
	HEADER_GC_BOSS_POSITION = 222,
#endif

	HEADER_GC_KEY_AGREEMENT_COMPLETED = 0xeb,
	HEADER_GC_KEY_AGREEMENT = 0xec,
	HEADER_GC_HANDSHAKE_OK = 0xed,
	HEADER_GC_PHASE = 0xee,
//#ifdef HEXA_V1TOOL_PROTECTION	
//	HEADER_GC_BINDUDP = 0xc7,
//		HEADER_GC_HANDSHAKE = 0xc6,
//#endif
	HEADER_GC_BINDUDP = 0xef,
	HEADER_GC_HANDSHAKE = 0xf0,
};

enum
{
	ID_MAX_NUM = 30,
	PASS_MAX_NUM = 16,
	CHAT_MAX_NUM = 128,
	PATH_NODE_MAX_NUM = 64,
	SHOP_SIGN_MAX_LEN = 32,

	PLAYER_PER_ACCOUNT3 = 3,
#ifndef ENABLE_PLAYER_PER_ACCOUNT5
	PLAYER_PER_ACCOUNT4 = 4,
#else
	PLAYER_PER_ACCOUNT4 = 5,
	PLAYER_PER_ACCOUNT5 = 5,
#endif

	PLAYER_ITEM_SLOT_MAX_NUM = 20,		// 플래이어의 슬롯당 들어가는 갯수.

	QUICKSLOT_MAX_LINE = 4,
	QUICKSLOT_MAX_COUNT_PER_LINE = 8, // 클라이언트 임의 결정값
	QUICKSLOT_MAX_COUNT = QUICKSLOT_MAX_LINE * QUICKSLOT_MAX_COUNT_PER_LINE,

	QUICKSLOT_MAX_NUM = 36, // 서버와 맞춰져 있는 값
#ifdef ENABLE_120_SHOP_SLOT_RAZOR93
	SHOP_HOST_ITEM_MAX_NUM = 120,
#else
	SHOP_HOST_ITEM_MAX_NUM = 40,
#endif
	METIN_SOCKET_COUNT = 6,

	PARTY_AFFECT_SLOT_MAX_NUM = 7,

	GUILD_GRADE_NAME_MAX_LEN = 8,
	GUILD_NAME_MAX_LEN = 12,
	GUILD_GRADE_COUNT = 15,
	GULID_COMMENT_MAX_LEN = 50,

	MARK_CRC_NUM = 8*8,
	MARK_DATA_SIZE = 16*12,
	SYMBOL_DATA_SIZE = 128*256,
	QUEST_INPUT_STRING_MAX_NUM = 64,

	PRIVATE_CODE_LENGTH = 8,

#if defined(ENABLE_PLAYER_PIN_SYSTEM)
	PIN_CODE_LENGTH = 4,
#endif

	REFINE_MATERIAL_MAX_NUM = 5,

	WEAR_MAX_NUM = CItemData::WEAR_MAX_NUM,


	SHOP_TAB_NAME_MAX = 32,
	SHOP_TAB_COUNT_MAX = 3,
#ifdef ENABLE_HWID
	HWID_LENGTH = 64,
#endif
};

#pragma pack(push)
#pragma pack(1) 

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Mark
typedef struct command_mark_login
{
    uint8_t    header;
    uint32_t   handle;
    uint32_t   random_key;
} TPacketCGMarkLogin;
#ifdef ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
typedef struct SPacketGCBeltNameUpdate
{
	uint8_t header;          // 
	char name[CHARACTER_NAME_MAX_LEN + 64]; // 
} TPacketGCBeltNameUpdate;
#endif
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
typedef struct SPacketGCItemOnTitleNameUpdate
{
	uint8_t header;
	uint32_t dwVID;
	char name[CHARACTER_NAME_MAX_LEN + 64];
} TPacketGCItemOnTitleNameUpdate;
#endif
typedef struct packet_mount_count_overhead {
	uint8_t header;
	uint32_t dwVID;
	int mountCount;
} TPacketGCMountCountOverhead;

typedef struct command_mark_upload
{
    uint8_t    header;
    uint32_t   gid;
    uint8_t    image[16*12*4];
} TPacketCGMarkUpload;

typedef struct command_mark_idxlist
{
	uint8_t    header;
} TPacketCGMarkIDXList;

typedef struct command_mark_crclist
{
	uint8_t    header;
	uint8_t    imgIdx;
    uint32_t   crclist[80];
} TPacketCGMarkCRCList;

typedef struct packet_mark_idxlist
{
	uint8_t    header;
	uint32_t	bufSize;
	uint16_t    count;
    //뒤에 size * (uint16_t + uint16_t)만큼 데이터 붙음
} TPacketGCMarkIDXList;

typedef struct packet_mark_block
{
	uint8_t    header;
    uint32_t   bufSize;
	uint8_t	imgIdx;
    uint32_t   count;
    // 뒤에 64 x 48 x 픽셀크기(4바이트) = 12288만큼 데이터 붙음
} TPacketGCMarkBlock;

typedef struct command_symbol_upload
{
	uint8_t	header;
	uint16_t	size;
	uint32_t	handle;
} TPacketCGSymbolUpload;

typedef struct command_symbol_crc
{
	uint8_t	header;
	uint32_t	dwGuildID;
	uint32_t	dwCRC;
	uint32_t	dwSize;
} TPacketCGSymbolCRC;

typedef struct packet_symbol_data
{
	uint8_t header;
	uint16_t size;
    uint32_t guild_id;
} TPacketGCGuildSymbolData;

//
//
//
typedef struct packet_observer_add
{
	uint8_t	header;
	uint32_t	vid;
	uint16_t	x;
	uint16_t	y;
} TPacketGCObserverAdd;

typedef struct packet_observer_move
{
	uint8_t	header;
	uint32_t	vid;
	uint16_t	x;
	uint16_t	y;
} TPacketGCObserverMove;


typedef struct packet_observer_remove
{
	uint8_t	header;
	uint32_t	vid;
} TPacketGCObserverRemove;


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// To Server

typedef struct command_checkin
{
	uint8_t header;
	char name[ID_MAX_NUM+1];
	char pwd[PASS_MAX_NUM+1];
} TPacketCGCheckin;

typedef struct command_login
{
    uint8_t header;
    char name[ID_MAX_NUM + 1];
    char pwd[PASS_MAX_NUM + 1];
} TPacketCGLogin;

// start - 권한 서버 접속을 위한 패킷들
typedef struct command_login2
{
	uint8_t	header;
	char	name[ID_MAX_NUM + 1];
	uint32_t	login_key;
    uint32_t	adwClientKey[4];
} TPacketCGLogin2;

typedef struct command_login3 {
	uint8_t header;
	char name[ID_MAX_NUM + 1];
	char pwd[PASS_MAX_NUM + 1];
	uint32_t adwClientKey[4];
#ifdef ENABLE_HWID
	char hwid[HWID_LENGTH + 1];
#endif
} TPacketCGLogin3;

#ifdef NEW_PET_SYSTEM
typedef struct packet_RequestPetName
{
	uint8_t byHeader;
	char petname[13];
} TPacketCGRequestPetName;
#endif

typedef struct command_direct_enter
{
	uint8_t        bHeader;
    char        login[ID_MAX_NUM + 1];
    char        passwd[PASS_MAX_NUM + 1];
	uint8_t        index;
} TPacketCGDirectEnter;

typedef struct command_player_select
{
	uint8_t	header;
	uint8_t	player_index;
} TPacketCGSelectCharacter;

typedef struct command_attack
{
	uint8_t	header;
	uint8_t	bType;			// 공격 유형
	uint32_t	dwVictimVID;	// 적 VID
	uint8_t	bCRCMagicCubeProcPiece;
	uint8_t	bCRCMagicCubeFilePiece;
} TPacketCGAttack;

typedef struct command_chat
{
	uint8_t	header;
	uint16_t	length;
	uint8_t	type;
} TPacketCGChat;

typedef struct command_whisper
{
	uint8_t        bHeader;
	uint16_t        wSize;
    char        szNameTo[CHARACTER_NAME_MAX_LEN + 1];
} TPacketCGWhisper;



enum EBattleMode
{
	BATTLEMODE_ATTACK = 0,
	BATTLEMODE_DEFENSE = 1,
};

typedef struct command_EnterFrontGame
{
	uint8_t header;
} TPacketCGEnterFrontGame;

typedef struct command_item_use
{
	uint8_t header;
	TItemPos pos;

	command_item_use() : header(0) {}
} TPacketCGItemUse;

typedef struct command_item_use_to_item
{
	uint8_t header;
	TItemPos source_pos;
	TItemPos target_pos;

	command_item_use_to_item() : header(0) {}
} TPacketCGItemUseToItem;

typedef struct command_item_drop
{
	uint8_t  header;
	TItemPos pos;
	int64_t elk;

	command_item_drop() : header(0), elk(0) {}
} TPacketCGItemDrop;

typedef struct command_item_drop2
{
	uint8_t		header;
	TItemPos	pos;
	int64_t		gold;
	int			count;

	command_item_drop2() : header(0), gold(0), count(0) {}
} TPacketCGItemDrop2;

typedef struct command_item_destroy
{
	uint8_t		header;
	TItemPos	pos;

	command_item_destroy() : header(0) {}
} TPacketCGItemDestroy;

typedef struct command_item_division
{
	uint8_t		header;
	TItemPos	pos;

	command_item_division() : header(0) {}
} TPacketCGItemDivision;

typedef struct command_item_move
{
	uint8_t		header;
	TItemPos	pos;
	TItemPos	change_pos;
	int			count;

	command_item_move() : header(0), count(0) {}
} TPacketCGItemMove;

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
typedef struct envanter_paketi
{
	uint8_t		header;
} TPacketCGEnvanter;
#endif


typedef struct command_item_pickup
{
	uint8_t header;
	uint32_t vid;
} TPacketCGItemPickUp;

typedef struct command_quickslot_add
{
	uint8_t        header;
	uint8_t        pos;
	TQuickSlot	slot;
}TPacketCGQuickSlotAdd;

typedef struct command_quickslot_del
{
	uint8_t        header;
	uint8_t        pos;
}TPacketCGQuickSlotDel;

typedef struct command_quickslot_swap
{
	uint8_t        header;
	uint8_t        pos;
	uint8_t        change_pos;
}TPacketCGQuickSlotSwap;

typedef struct command_on_click
{
	uint8_t		header;
	uint32_t		vid;
} TPacketCGOnClick;


enum
{
	SHOP_SUBHEADER_CG_END,
	SHOP_SUBHEADER_CG_BUY,
	SHOP_SUBHEADER_CG_SELL,
	SHOP_SUBHEADER_CG_SELL2
#ifdef ENABLE_BUY_STACK_FROM_SHOP
	,SHOP_SUBHEADER_CG_BUY2
#endif
};

typedef struct command_shop
{
	uint8_t	header;
	uint8_t	subheader;
} TPacketCGShop;

enum
{
	EXCHANGE_SUBHEADER_CG_START,			// arg1 == vid of target character
	EXCHANGE_SUBHEADER_CG_ITEM_ADD,		// arg1 == position of item
	EXCHANGE_SUBHEADER_CG_ITEM_DEL,		// arg1 == position of item
	EXCHANGE_SUBHEADER_CG_ELK_ADD,			// arg1 == amount of elk
	EXCHANGE_SUBHEADER_CG_ACCEPT,			// arg1 == not used
	EXCHANGE_SUBHEADER_CG_CANCEL,			// arg1 == not used
};

typedef struct command_exchange
{
	uint8_t		header;
	uint8_t		subheader;

	int64_t		arg1;

	uint8_t		arg2;
	TItemPos	Pos;

	command_exchange() : header(0), subheader(0), arg1(0), arg2(0) {}
} TPacketCGExchange;

typedef struct command_position
{
	uint8_t        header;
	uint8_t        position;
} TPacketCGPosition;

typedef struct command_script_answer
{
	uint8_t        header;
	uint8_t		answer;
} TPacketCGScriptAnswer;

typedef struct command_script_button
{
	uint8_t        header;
	uint32_t			idx;
} TPacketCGScriptButton;

typedef struct command_target
{
	uint8_t        header;
    uint32_t       dwVID;
} TPacketCGTarget;

typedef struct command_move
{
	uint8_t		bHeader;
	uint8_t		bFunc;
	uint8_t		bArg;
	float		bRot;
	int32_t		lX;
	int32_t		lY;
	uint32_t		dwTime;
} TPacketCGMove;

typedef struct command_sync_position_element
{
    uint32_t       dwVID;
	int32_t        lX;
	int32_t        lY;
} TPacketCGSyncPositionElement;

typedef struct command_sync_position
{
    uint8_t        bHeader;
	uint16_t		wSize;
} TPacketCGSyncPosition;

typedef struct command_fly_targeting
{
	uint8_t		bHeader;
	uint32_t		dwTargetVID;
	int32_t		lX;
	int32_t		lY;
} TPacketCGFlyTargeting;

typedef struct packet_fly_targeting
{
	uint8_t        bHeader;
	uint32_t		dwShooterVID;
	uint32_t		dwTargetVID;
	int32_t		lX;
	int32_t		lY;
} TPacketGCFlyTargeting;

typedef struct packet_shoot
{
	uint8_t		bHeader;
	uint8_t		bType;
} TPacketCGShoot;

typedef struct command_warp
{
	uint8_t			bHeader;
} TPacketCGWarp;

enum
{
#ifdef ENABLE_MESSENGER_TEAM
	MESSENGER_SUBHEADER_GC_TEAM_LIST,
	MESSENGER_SUBHEADER_GC_TEAM_LOGIN,
	MESSENGER_SUBHEADER_GC_TEAM_LOGOUT,
#endif
	MESSENGER_SUBHEADER_GC_LIST,
	MESSENGER_SUBHEADER_GC_LOGIN,
	MESSENGER_SUBHEADER_GC_LOGOUT,
	MESSENGER_SUBHEADER_GC_INVITE
#ifdef ENABLE_MESSENGER_HELPER
	, MESSENGER_SUBHEADER_GC_HELPER_LIST = 8 ,
	MESSENGER_SUBHEADER_GC_HELPER_LOGIN = 9,
	MESSENGER_SUBHEADER_GC_HELPER_LOGOUT = 10
#endif
};

typedef struct packet_messenger
{
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
} TPacketGCMessenger;

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
enum
{
	MESSENGER_CONNECTED_STATE_OFFLINE,
	MESSENGER_CONNECTED_STATE_ONLINE,
};

typedef struct packet_messenger_list_offline
{
	uint8_t connected; // always 0
	uint8_t length;
} TPacketGCMessengerListOffline;

typedef struct packet_messenger_list_online
{
	uint8_t connected;
	uint8_t length;
	//uint8_t length_char_name;
} TPacketGCMessengerListOnline;

typedef struct packet_messenger_login
{
	//uint8_t length_login;
	//uint8_t length_char_name;
	uint8_t length;
} TPacketGCMessengerLogin;

typedef struct packet_messenger_logout
{
	uint8_t length;
} TPacketGCMessengerLogout;

enum
{
    MESSENGER_SUBHEADER_CG_ADD_BY_VID,
    MESSENGER_SUBHEADER_CG_ADD_BY_NAME,
    MESSENGER_SUBHEADER_CG_REMOVE,
};

typedef struct command_messenger
{
	uint8_t header;
	uint8_t subheader;
} TPacketCGMessenger;

typedef struct command_messenger_remove
{
	uint8_t length;
} TPacketCGMessengerRemove;

enum
{
	SAFEBOX_MONEY_STATE_SAVE,
	SAFEBOX_MONEY_STATE_WITHDRAW,
};

typedef struct command_safebox_money
{
	uint8_t        bHeader;
	uint8_t        bState;
    uint32_t       dwMoney;

	command_safebox_money() : bHeader(0), bState(0), dwMoney(0){}
} TPacketCGSafeboxMoney;

typedef struct command_safebox_checkout
{
	uint8_t        bHeader;
	uint32_t        bSafePos;
    TItemPos	ItemPos;

	command_safebox_checkout() : bHeader(0), bSafePos(0) {}
} TPacketCGSafeboxCheckout;

typedef struct command_safebox_checkin
{
	uint8_t        bHeader;
	uint32_t        bSafePos;
    TItemPos	ItemPos;

	command_safebox_checkin() : bHeader(0), bSafePos(0) {}
} TPacketCGSafeboxCheckin;

typedef struct command_mall_checkout
{
	uint8_t        bHeader;
	uint8_t        bMallPos;
    TItemPos	ItemPos;

	command_mall_checkout() : bHeader(0), bMallPos(0) {}
} TPacketCGMallCheckout;

///////////////////////////////////////////////////////////////////////////////////
// Party

typedef struct command_use_skill
{
	uint8_t                bHeader;
    uint32_t               dwVnum;
	uint32_t				dwTargetVID;
} TPacketCGUseSkill;

typedef struct command_party_invite
{
	uint8_t header;
    uint32_t vid;
} TPacketCGPartyInvite;

typedef struct command_party_invite_answer
{
	uint8_t header;
    uint32_t leader_pid;
	uint8_t accept;
} TPacketCGPartyInviteAnswer;

typedef struct command_party_remove
{
	uint8_t header;
    uint32_t pid;
} TPacketCGPartyRemove;

typedef struct command_party_set_state
{
	uint8_t byHeader;
    uint32_t dwVID;
	uint8_t byState;
	uint8_t byFlag;
} TPacketCGPartySetState;

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

typedef struct command_party_use_skill
{
	uint8_t byHeader;
	uint8_t bySkillIndex;
    uint32_t dwTargetVID;
} TPacketCGPartyUseSkill;

enum
{
	GUILD_SUBHEADER_CG_ADD_MEMBER,
	GUILD_SUBHEADER_CG_REMOVE_MEMBER,
	GUILD_SUBHEADER_CG_CHANGE_GRADE_NAME,
	GUILD_SUBHEADER_CG_CHANGE_GRADE_AUTHORITY,
	GUILD_SUBHEADER_CG_OFFER,
	GUILD_SUBHEADER_CG_POST_COMMENT,
	GUILD_SUBHEADER_CG_DELETE_COMMENT,
	GUILD_SUBHEADER_CG_REFRESH_COMMENT,
	GUILD_SUBHEADER_CG_CHANGE_MEMBER_GRADE,
	GUILD_SUBHEADER_CG_USE_SKILL,
	GUILD_SUBHEADER_CG_CHANGE_MEMBER_GENERAL,
	GUILD_SUBHEADER_CG_GUILD_INVITE_ANSWER,
	GUILD_SUBHEADER_CG_CHARGE_GSP,
	GUILD_SUBHEADER_CG_DEPOSIT_MONEY,
	GUILD_SUBHEADER_CG_WITHDRAW_MONEY,
};

typedef struct command_guild
{
	uint8_t byHeader;
	uint8_t bySubHeader;
} TPacketCGGuild;

typedef struct command_guild_answer_make_guild
{
	uint8_t header;
	char guild_name[GUILD_NAME_MAX_LEN+1];
} TPacketCGAnswerMakeGuild;

typedef struct command_give_item
{
	uint8_t		byHeader;
	uint32_t		dwTargetVID;
	TItemPos	ItemPos;

	int			byItemCount;

	command_give_item() : byHeader(0), dwTargetVID(0), byItemCount(0) {}

} TPacketCGGiveItem;

typedef struct SPacketCGHack
{
	uint8_t        bHeader;
    char        szBuf[255 + 1];
} TPacketCGHack;

typedef struct command_dungeon
{
	uint8_t		bHeader;
	uint16_t		size;
} TPacketCGDungeon;

// Private Shop
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
	int			count;
#ifdef ENABLE_BUY_WITH_ITEM
	TShopItemPrice	itemprice[MAX_SHOP_PRICES];
#endif
	TItemPos	pos;

	int64_t	price;

	uint8_t		display_pos;
} TShopItemTable;

typedef struct SPacketCGMyShop
{
	uint8_t	bHeader;
	char	szSign[SHOP_SIGN_MAX_LEN + 1];
	uint8_t	bCount;
#ifdef KASMIR_PAKET_SYSTEM
	uint32_t	dwKasmirNpc;
	uint8_t	bKasmirBaslik;
#endif
} TPacketCGMyShop;

typedef struct SPacketCGRefine
{
	uint8_t		header;
	uint8_t		pos;
	uint8_t		type;
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	uint8_t		lLow;
	uint8_t		lMedium;
	uint8_t		lExtra;
	uint8_t		lTotal;
#endif
} TPacketCGRefine;

typedef struct SPacketCGChangeName
{
	uint8_t header;
	uint8_t index;
    char name[CHARACTER_NAME_MAX_LEN+1];
} TPacketCGChangeName;

typedef struct command_client_version
{
	uint8_t header;
	char filename[32+1];
	char timestamp[32+1];
} TPacketCGClientVersion;

typedef struct command_client_version2
{
	uint8_t header;
	char filename[32+1];
	char timestamp[32+1];
} TPacketCGClientVersion2;

typedef struct command_crc_report
{
	uint8_t header;
	uint8_t byPackMode;
	uint32_t dwBinaryCRC32;
	uint32_t dwProcessCRC32;
	uint32_t dwRootPackCRC32;
} TPacketCGCRCReport;

enum EPartyExpDistributionType
{
    PARTY_EXP_DISTRIBUTION_NON_PARITY,
    PARTY_EXP_DISTRIBUTION_PARITY,
};

typedef struct command_party_parameter
{
	uint8_t        bHeader;
	uint8_t        bDistributeMode;
} TPacketCGPartyParameter;

typedef struct command_quest_input_string
{
	uint8_t        bHeader;
    char		szString[QUEST_INPUT_STRING_MAX_NUM+1];
} TPacketCGQuestInputString;

typedef struct command_quest_confirm
{
	uint8_t header;
	uint8_t answer;
    uint32_t requestPID;
} TPacketCGQuestConfirm;

typedef struct command_script_select_item
{
	uint8_t header;
    uint32_t selection;
} TPacketCGScriptSelectItem;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// From Server
enum EPhase
{
    PHASE_CLOSE,				// 끊기는 상태 (또는 끊기 전 상태)
    PHASE_HANDSHAKE,			// 악수..;;
    PHASE_LOGIN,				// 로그인 중
    PHASE_SELECT,				// 캐릭터 선택 화면
    PHASE_LOADING,				// 선택 후 로딩 화면
    PHASE_GAME,					// 게임 화면
    PHASE_DEAD,					// 죽었을 때.. (게임 안에 있는 것일 수도..)

	PHASE_DBCLIENT_CONNECTING,	// 서버용
    PHASE_DBCLIENT,				// 서버용
    PHASE_P2P,					// 서버용
    PHASE_AUTH,					// 로그인 인증 용
};

typedef struct packet_phase
{
	uint8_t        header;
	uint8_t        phase;
} TPacketGCPhase;

typedef struct packet_blank		// 공백패킷.
{
	uint8_t		header;
} TPacketGCBlank;

typedef struct packet_blank_dynamic
{
	uint8_t		header;
	uint16_t		size;
} TPacketGCBlankDynamic;

typedef struct packet_header_handshake
{
	uint8_t		header;
	uint32_t		dwHandshake;
	uint32_t		dwTime;
	int32_t		lDelta;
} TPacketGCHandshake;

typedef struct packet_header_bindudp
{
	uint8_t		header;
	uint32_t		addr;
	uint16_t		port;
} TPacketGCBindUDP;

typedef struct packet_header_dynamic_size
{
	uint8_t		header;
	uint16_t		size;
} TDynamicSizePacketHeader;

#ifdef __ENABLE_LARGE_DYNAMIC_PACKET__
typedef struct packet_header_large_dynamic_size
{
	uint8_t		header;
	int			size;
} TLargeDynamicSizePacketHeader;
#endif

typedef struct SSimplePlayerInformation
{
	uint32_t				dwID;
	char				szName[CHARACTER_NAME_MAX_LEN + 1];
	uint8_t				byJob;
	uint8_t				byLevel;
	uint32_t				dwPlayMinutes;
	uint8_t				byST, byHT, byDX, byIQ;
	// uint16_t				wParts[CRaceData::PART_MAX_NUM];
	uint16_t				wMainPart;
	uint8_t				bChangeName;
	uint16_t				wHairPart;
#ifdef ENABLE_ACCE_SYSTEM
	uint16_t				wAccePart;
#endif
	uint8_t				bDummy[4];
	int32_t				x, y;
	int32_t				lAddr;
	uint16_t				wPort;
	uint8_t				bySkillGroup;
#if defined(ENABLE_PLAYER_PIN_SYSTEM)
	char				pin[PIN_CODE_LENGTH + 1];
#endif
} TSimplePlayerInformation;

typedef struct packet_login_success3
{
	uint8_t						header;
	TSimplePlayerInformation	akSimplePlayerInformation[PLAYER_PER_ACCOUNT3];
    uint32_t						guild_id[PLAYER_PER_ACCOUNT3];
    char						guild_name[PLAYER_PER_ACCOUNT3][GUILD_NAME_MAX_LEN+1];
	uint32_t handle;
	uint32_t random_key;
} TPacketGCLoginSuccess3;

typedef struct packet_login_success4
{
	uint8_t						header;
	TSimplePlayerInformation	akSimplePlayerInformation[PLAYER_PER_ACCOUNT4];
    uint32_t						guild_id[PLAYER_PER_ACCOUNT4];
    char						guild_name[PLAYER_PER_ACCOUNT4][GUILD_NAME_MAX_LEN+1];
	uint32_t handle;
	uint32_t random_key;
} TPacketGCLoginSuccess4;
#ifdef ENABLE_PLAYER_PER_ACCOUNT5
typedef struct packet_login_success5
{
	uint8_t						header;
	TSimplePlayerInformation	akSimplePlayerInformation[PLAYER_PER_ACCOUNT5];
    uint32_t						guild_id[PLAYER_PER_ACCOUNT5];
    char						guild_name[PLAYER_PER_ACCOUNT5][GUILD_NAME_MAX_LEN+1];
	uint32_t handle;
	uint32_t random_key;
} TPacketGCLoginSuccess5;
#endif

enum { LOGIN_STATUS_MAX_LEN = 8 };
typedef struct packet_login_failure
{
	uint8_t	header;
	char	szStatus[LOGIN_STATUS_MAX_LEN + 1];
} TPacketGCLoginFailure;

typedef struct command_player_create
{
	uint8_t		header;
	uint8_t		index;
	char		name[CHARACTER_NAME_MAX_LEN + 1];
	uint16_t		job;
	uint8_t		shape;
	uint8_t		CON;
	uint8_t		INT;
	uint8_t		STR;
	uint8_t		DEX;
#if defined(ENABLE_PLAYER_PIN_SYSTEM)
	char		pin[PIN_CODE_LENGTH + 1];
#endif
} TPacketCGCreateCharacter;

typedef struct command_player_create_success
{
	uint8_t						header;
	uint8_t						bAccountCharacterSlot;
    TSimplePlayerInformation	kSimplePlayerInfomation;
} TPacketGCPlayerCreateSuccess;

typedef struct command_create_failure
{
	uint8_t	header;
	uint8_t	bType;
} TPacketGCCreateFailure;

typedef struct command_player_delete
{
	uint8_t        header;
	uint8_t        index;
	char		szPrivateCode[PRIVATE_CODE_LENGTH];
} TPacketCGDestroyCharacter;

typedef struct packet_player_delete_success
{
	uint8_t        header;
	uint8_t        account_index;
} TPacketGCDestroyCharacterSuccess;

enum
{
	ADD_CHARACTER_STATE_DEAD   = (1 << 0),
	ADD_CHARACTER_STATE_SPAWN  = (1 << 1),
	ADD_CHARACTER_STATE_GUNGON = (1 << 2),
	ADD_CHARACTER_STATE_KILLER = (1 << 3),
	ADD_CHARACTER_STATE_PARTY  = (1 << 4),
};

enum EPKModes
{
	PK_MODE_PEACE,
	PK_MODE_REVENGE,
	PK_MODE_FREE,
	PK_MODE_PROTECT,
	PK_MODE_GUILD,
	PK_MODE_MAX_NUM,
};

// 2004.11.20.myevan.CRaceData::PART_MAX_NUM 사용안하게 수정 - 서버에서 사용하는것과 일치하지 않음
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

typedef struct packet_char_additional_info
{
	uint8_t    header;
	uint32_t   dwVID;
	char    name[CHARACTER_NAME_MAX_LEN + 1];
	uint16_t    awPart[CHR_EQUIPPART_NUM];
	uint8_t	bEmpire;
	uint32_t   dwGuildID;
	uint32_t   dwLevel;
	uint32_t   sAlignment; //선악치
	uint8_t    bPKMode;
	uint32_t   dwMountVnum;
#ifdef ENABLE_SKILL_COLOR_SYSTEM
	uint32_t	dwSkillColor[ESkillColorLength::MAX_SKILL_COUNT + MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
#endif

#ifdef ENABLE_MULTI_LANGUAGE
	uint8_t	bLanguage;
#endif
} TPacketGCCharacterAdditionalInfo;

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

typedef struct packet_add_char2
{
	uint8_t        header;

    uint32_t       dwVID;

    char        name[CHARACTER_NAME_MAX_LEN + 1];

    float       angle;
	int32_t        x;
	int32_t        y;
	int32_t        z;

	uint8_t		bType;
	uint16_t        wRaceNum;
	uint16_t        awPart[CHR_EQUIPPART_NUM];
	uint16_t        bMovingSpeed;
	uint16_t        bAttackSpeed;

	uint8_t        bStateFlag;
    uint32_t       dwAffectFlag[2];        // ??
	uint8_t        bEmpire;

    uint32_t       dwGuild;
    uint32_t       sAlignment;
	uint8_t		bPKMode;
	uint32_t		dwMountVnum;
} TPacketGCCharacterAdd2;

typedef struct packet_update_char
{
	uint8_t        header;
    uint32_t       dwVID;

	uint16_t        awPart[CHR_EQUIPPART_NUM];
	uint16_t        bMovingSpeed;
	uint16_t		bAttackSpeed;

	uint8_t        bStateFlag;
    uint32_t       dwAffectFlag[2];

	uint32_t		dwGuildID;
    uint32_t       sAlignment;
	uint8_t		bPKMode;
	uint32_t		dwMountVnum;
#ifdef ENABLE_SKILL_COLOR_SYSTEM
	uint32_t		dwSkillColor[ESkillColorLength::MAX_SKILL_COUNT + MAX_BUFF_COUNT][ESkillColorLength::MAX_EFFECT_COUNT];
#endif

#ifdef ENABLE_MULTI_LANGUAGE
	uint8_t		bLanguage;
#endif	

} TPacketGCCharacterUpdate;

//typedef struct packet_update_char2
//{
//	uint8_t        header;
//    uint32_t       dwVID;
//
//	uint16_t        awPart[CHR_EQUIPPART_NUM];
//	uint16_t        bMovingSpeed;
//	uint16_t		bAttackSpeed;
//
//	uint8_t        bStateFlag;
//    uint32_t       dwAffectFlag[2];
//
//	uint32_t		dwGuildID;
//    uint32_t       sAlignment;
//	uint8_t		bPKMode;
//	uint32_t		dwMountVnum;
//#ifdef ENABLE_MULTI_LANGUAGE
//	uint8_t		bLanguage;
//#endif
//} TPacketGCCharacterUpdate2;

typedef struct packet_del_char
{
	uint8_t	header;
    uint32_t	dwVID;
} TPacketGCCharacterDelete;

typedef struct packet_GlobalTime
{
	uint8_t	header;
	float	GlobalTime;
} TPacketGCGlobalTime;

enum EChatType
{
	CHAT_TYPE_TALKING,  /* 그냥 채팅 */
	CHAT_TYPE_INFO,     /* 정보 (아이템을 집었다, 경험치를 얻었다. 등) */
	CHAT_TYPE_NOTICE,   /* 공지사항 */
	CHAT_TYPE_PARTY,    /* 파티말 */
	CHAT_TYPE_GUILD,    /* 길드말 */
	CHAT_TYPE_COMMAND,	/* 명령 */
	CHAT_TYPE_SHOUT,	/* 외치기 */
	CHAT_TYPE_WHISPER,	// 서버와는 연동되지 않는 Only Client Enum
	CHAT_TYPE_BIG_NOTICE,
#ifdef ENABLE_DICE_SYSTEM
	CHAT_TYPE_DICE_INFO, //11
#endif
#ifdef ENABLE_NEW_CHAT
	CHAT_TYPE_INFO_EXP,
	CHAT_TYPE_INFO_ITEM,
	CHAT_TYPE_INFO_VALUE,
#endif
	CHAT_TYPE_DIALOG,
	CHAT_TYPE_MAX_NUM,
};

typedef struct packet_chatting
{
	uint8_t	header;
	uint16_t	size;
	uint8_t	type;
	uint32_t	dwVID;
	uint8_t	bEmpire;
} TPacketGCChat;

typedef struct packet_whisper   // 가변 패킷
{
	uint8_t	bHeader;
	uint16_t	wSize;
	uint8_t	bType;
	char	szNameFrom[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGCWhisper;

typedef struct packet_stun
{
	uint8_t		header;
	uint32_t		vid;
} TPacketGCStun;

typedef struct packet_dead
{
	uint8_t		header;
	uint32_t		vid;
} TPacketGCDead;

typedef struct packet_main_character
{
	uint8_t        header;
    uint32_t       dwVID;
	uint16_t		wRaceNum;
    char        szName[CHARACTER_NAME_MAX_LEN + 1];
	int32_t        lX, lY, lZ;
	uint8_t	empire;
	uint8_t		bySkillGroup;
} TPacketGCMainCharacter;

typedef struct packet_main_character3_bgm
{
	enum
	{
		MUSIC_NAME_MAX_LEN = 24,
	};
	uint8_t        header;
    uint32_t       dwVID;
	uint16_t		wRaceNum;
    char        szUserName[CHARACTER_NAME_MAX_LEN + 1];
	char        szBGMName[MUSIC_NAME_MAX_LEN + 1];
    int32_t        lX, lY, lZ;
	uint8_t		byEmpire;
	uint8_t		bySkillGroup;
} TPacketGCMainCharacter3_BGM;

typedef struct packet_main_character4_bgm_vol
{
	enum
	{
		MUSIC_NAME_MAX_LEN = 24,
	};
	uint8_t        header;
    uint32_t       dwVID;
	uint16_t		wRaceNum;
    char        szUserName[CHARACTER_NAME_MAX_LEN + 1];
	char        szBGMName[MUSIC_NAME_MAX_LEN + 1];
	float		fBGMVol;
    int32_t        lX, lY, lZ;
	uint8_t		byEmpire;
	uint8_t		bySkillGroup;
} TPacketGCMainCharacter4_BGM_VOL;
// END_OF_SUPPORT_BGM

enum EPointTypes
{
	POINT_NONE,
	POINT_LEVEL,
	POINT_VOICE,
	POINT_EXP,
	POINT_NEXT_EXP,
	POINT_HP,
	POINT_MAX_HP,
	POINT_SP,
	POINT_MAX_SP,
	POINT_STAMINA,
	POINT_MAX_STAMINA,
	POINT_GOLD,
	POINT_ST,
	POINT_HT,
	POINT_DX,
	POINT_IQ,
	POINT_ATT_POWER,
	POINT_ATT_SPEED,
	POINT_EVADE_RATE,
	POINT_MOV_SPEED,
	POINT_DEF_GRADE,
	POINT_CASTING_SPEED,
	POINT_MAGIC_ATT_GRADE,
	POINT_MAGIC_DEF_GRADE,
	POINT_EMPIRE_POINT,
	POINT_LEVEL_STEP,
	POINT_STAT,
	POINT_SUB_SKILL,
	POINT_SKILL,
	POINT_MIN_ATK,
	POINT_MAX_ATK,
	POINT_PLAYTIME,
	POINT_HP_REGEN,
	POINT_SP_REGEN,
	POINT_BOW_DISTANCE,
	POINT_HP_RECOVERY,
	POINT_SP_RECOVERY,
	POINT_POISON_PCT,
	POINT_STUN_PCT,
	POINT_SLOW_PCT,
	POINT_CRITICAL_PCT,
	POINT_PENETRATE_PCT,
	POINT_CURSE_PCT,
	POINT_ATTBONUS_HUMAN,
	POINT_ATTBONUS_ANIMAL,
	POINT_ATTBONUS_ORC,
	POINT_ATTBONUS_MILGYO,
	POINT_ATTBONUS_UNDEAD,
	POINT_ATTBONUS_DEVIL,
	POINT_ATTBONUS_INSECT,
	POINT_ATTBONUS_FIRE,
	POINT_ATTBONUS_ICE,
	POINT_ATTBONUS_DESERT,
	POINT_ATTBONUS_MONSTER,
	POINT_ATTBONUS_WARRIOR,
	POINT_ATTBONUS_ASSASSIN,
	POINT_ATTBONUS_SURA,
	POINT_ATTBONUS_SHAMAN,
	POINT_ATTBONUS_UNUSED5,
	POINT_RESIST_WARRIOR,
	POINT_RESIST_ASSASSIN,
	POINT_RESIST_SURA,
	POINT_RESIST_SHAMAN,
	POINT_STEAL_HP,
	POINT_STEAL_SP,
	POINT_MANA_BURN_PCT,
	POINT_DAMAGE_SP_RECOVER,
	POINT_BLOCK,
	POINT_DODGE,
	POINT_RESIST_SWORD,
	POINT_RESIST_TWOHAND,
	POINT_RESIST_DAGGER,
	POINT_RESIST_BELL,
	POINT_RESIST_FAN,
	POINT_RESIST_BOW,
	POINT_RESIST_FIRE,
	POINT_RESIST_ELEC,
	POINT_RESIST_MAGIC,
	POINT_RESIST_WIND,
	POINT_REFLECT_MELEE,
	POINT_REFLECT_CURSE,
	POINT_POISON_REDUCE,
	POINT_KILL_SP_RECOVER,
	POINT_EXP_DOUBLE_BONUS,
	POINT_GOLD_DOUBLE_BONUS,
	POINT_ITEM_DROP_BONUS,
	POINT_POTION_BONUS,
	POINT_KILL_HP_RECOVER,
	POINT_IMMUNE_STUN,
	POINT_IMMUNE_SLOW,
	POINT_IMMUNE_FALL,
	POINT_PARTY_ATT_GRADE,
	POINT_PARTY_DEF_GRADE,
	POINT_ATT_BONUS,
	POINT_DEF_BONUS,
	POINT_ATT_GRADE_BONUS,
	POINT_DEF_GRADE_BONUS,
	POINT_MAGIC_ATT_GRADE_BONUS,
	POINT_MAGIC_DEF_GRADE_BONUS,
	POINT_RESIST_NORMAL_DAMAGE,
	POINT_STAT_RESET_COUNT = 112,
	POINT_HORSE_SKILL = 113,
	POINT_MALL_ATTBONUS,
	POINT_MALL_DEFBONUS,
	POINT_MALL_EXPBONUS,
	POINT_MALL_ITEMBONUS,
	POINT_MALL_GOLDBONUS,
	POINT_MAX_HP_PCT,
	POINT_MAX_SP_PCT,
	POINT_SKILL_DAMAGE_BONUS,
	POINT_NORMAL_HIT_DAMAGE_BONUS,
	POINT_SKILL_DEFEND_BONUS,
	POINT_NORMAL_HIT_DEFEND_BONUS,
	POINT_PC_BANG_EXP_BONUS,
	POINT_PC_BANG_DROP_BONUS,
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
	POINT_MIN_WEP = 200,
	POINT_MAX_WEP,
	POINT_MIN_MAGIC_WEP,
	POINT_MAX_MAGIC_WEP,
	POINT_HIT_RATE,

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
	POINT_BLACK = 145,
#endif

#ifdef ELEMENT_NEW_BONUSES
		POINT_ATTBONUS_ELEC = 146,	// 146
		POINT_ATTBONUS_WIND,	// 147
		POINT_ATTBONUS_EARTH,	// 148
		POINT_ATTBONUS_DARK,	// 149
#endif
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
#ifdef ENABLE_STRONG_METIN
	POINT_ATTBONUS_METIN = 159,
#endif
#ifdef ENABLE_STRONG_BOSS
	POINT_ATTBONUS_BOSS	= 160,
#endif
#ifdef ENABLE_RESIST_MONSTER
	POINT_RESIST_MONSTER	= 161,
#endif
#ifdef ENABLE_MEDI_PVM
	POINT_ATTBONUS_MEDI_PVM	= 162,
#endif


#ifdef ENABLE_GAYA_SYSTEM
	POINT_GAYA = 163,
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
};

typedef struct packet_points
{
	uint8_t        header;

	int64_t  points[POINT_MAX_NUM];

} TPacketGCPoints;

typedef struct packet_point_change
{
    int         header;

	uint32_t		dwVID;
	uint8_t		Type;

	int64_t	amount;
	int64_t	value;

} TPacketGCPointChange;

typedef struct packet_motion
{
	uint8_t		header;
	uint32_t		vid;
	uint32_t		victim_vid;
	uint16_t		motion;
} TPacketGCMotion;

typedef struct packet_set_item
{
	uint8_t		header;
	TItemPos	Cell;
	uint32_t		vnum;
	int			count;
	int32_t		alSockets[ITEM_SOCKET_SLOT_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
} TPacketGCItemSet;

typedef struct packet_set_item2
{
	uint8_t		header;
	TItemPos	Cell;
	uint32_t		vnum;
	int			count;
	uint32_t		flags;
	uint32_t		anti_flags;
	bool		highlight;
	int32_t		alSockets[ITEM_SOCKET_SLOT_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
} TPacketGCItemSet2;


typedef struct packet_item_del
{
	uint8_t        header;
	uint16_t        pos;
} TPacketGCItemDel;

typedef struct packet_use_item
{
	uint8_t		header;
	TItemPos	Cell;
	uint32_t		ch_vid;
	uint32_t		victim_vid;

	uint32_t		vnum;
} TPacketGCItemUse;

typedef struct packet_update_item
{
	uint8_t		header;
	TItemPos	Cell;
	int			count;
	int32_t		alSockets[ITEM_SOCKET_SLOT_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
} TPacketGCItemUpdate;

typedef struct packet_ground_add_item
{
	uint8_t        bHeader;
    int32_t        lX;
	int32_t		lY;
	int32_t		lZ;

    uint32_t       dwVID;
    uint32_t       dwVnum;
} TPacketGCItemGroundAdd;

typedef struct packet_ground_del_item
{
	uint8_t		header;
	uint32_t		vid;
} TPacketGCItemGroundDel;

typedef struct packet_item_ownership
{
	uint8_t        bHeader;
    uint32_t       dwVID;
    char        szName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketGCItemOwnership;

typedef struct packet_quickslot_add
{
	uint8_t        header;
	uint8_t        pos;
	TQuickSlot	slot;
} TPacketGCQuickSlotAdd;

typedef struct packet_quickslot_del
{
	uint8_t        header;
	uint8_t        pos;
} TPacketGCQuickSlotDel;

typedef struct packet_quickslot_swap
{
	uint8_t        header;
	uint8_t        pos;
	uint8_t        change_pos;
} TPacketGCQuickSlotSwap;

typedef struct packet_shop_start
{
	struct packet_shop_item		items[SHOP_HOST_ITEM_MAX_NUM];
} TPacketGCShopStart;

typedef struct packet_shop_start_ex // 다음에 TSubPacketShopTab* shop_tabs 이 따라옴.
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
	uint8_t						pos;
	struct packet_shop_item		item;
} TPacketGCShopUpdateItem;

typedef struct packet_shop_update_price
{

	int64_t iElkAmount;

} TPacketGCShopUpdatePrice;

enum EPacketShopSubHeaders
{
	SHOP_SUBHEADER_GC_START,
	SHOP_SUBHEADER_GC_END,
	SHOP_SUBHEADER_GC_UPDATE_ITEM,
	SHOP_SUBHEADER_GC_UPDATE_PRICE,
	SHOP_SUBHEADER_GC_OK,
	SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY,
#ifdef ENABLE_BUY_WITH_ITEM
	SHOP_SUBHEADER_GC_NOT_ENOUGH_ITEM,
#endif
	SHOP_SUBHEADER_GC_SOLDOUT,
	SHOP_SUBHEADER_GC_INVENTORY_FULL,
	SHOP_SUBHEADER_GC_INVALID_POS,
	SHOP_SUBHEADER_GC_SOLD_OUT,
	SHOP_SUBHEADER_GC_START_EX,
	SHOP_SUBHEADER_GC_NOT_ENOUGH_MONEY_EX,
};

typedef struct packet_shop
{
	uint8_t        header;
	uint16_t		size;
	uint8_t        subheader;
} TPacketGCShop;

typedef struct packet_exchange
{
	uint8_t        header;
	uint8_t        subheader;
	uint8_t        is_me;

	int64_t	arg1;

    TItemPos       arg2;
    uint32_t       arg3;
#ifdef WJ_ENABLE_TRADABLE_ICON
    TItemPos       arg4;
#endif
	int32_t		alValues[ITEM_SOCKET_SLOT_MAX_NUM];
    TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
} TPacketGCExchange;

enum
{
    EXCHANGE_SUBHEADER_GC_START,			// arg1 == vid
    EXCHANGE_SUBHEADER_GC_ITEM_ADD,		// arg1 == vnum  arg2 == pos  arg3 == count
	EXCHANGE_SUBHEADER_GC_ITEM_DEL,		// arg1 == pos
    EXCHANGE_SUBHEADER_GC_ELK_ADD,			// arg1 == elk
    EXCHANGE_SUBHEADER_GC_ACCEPT,			// arg1 == accept
    EXCHANGE_SUBHEADER_GC_END,				// arg1 == not used
    EXCHANGE_SUBHEADER_GC_ALREADY,			// arg1 == not used
    EXCHANGE_SUBHEADER_GC_LESS_ELK,		// arg1 == not used
};

typedef struct packet_position
{
	uint8_t        header;
	uint32_t		vid;
	uint8_t        position;
} TPacketGCPosition;

typedef struct packet_ping
{
	uint8_t		header;
} TPacketGCPing;

typedef struct packet_pong
{
	uint8_t		bHeader;
} TPacketCGPong;

typedef struct packet_script
{
	uint8_t		header;
	uint16_t        size;
	uint8_t		skin;
	uint16_t        src_size;
} TPacketGCScript;

typedef struct packet_target
{
	uint8_t        header;
    uint32_t       dwVID;
	uint8_t        bHPPercent;
#ifdef ENABLE_VIEW_TARGET_DECIMAL_HP
	int64_t		iMinHP;
	int64_t		iMaxHP;
#endif
#ifdef ENABLE_VIEW_ELEMENT
	uint8_t	bElement;
#endif
} TPacketGCTarget;

#ifdef ENABLE_SEND_TARGET_INFO
typedef struct packet_target_info
{
	uint8_t	header;
	uint32_t	dwVID;
	uint32_t	race;
	uint32_t	dwVnum;
	uint32_t	count;

} TPacketGCTargetInfo;

typedef struct packet_target_info_load
{
	uint8_t header;
	uint32_t dwVID;
} TPacketCGTargetInfoLoad;
#endif
typedef struct packet_damage_info
{
	uint8_t header;
	uint32_t dwVID;
	uint8_t flag;
	int  damage;
} TPacketGCDamageInfo;

typedef struct packet_change_speed
{
	uint8_t		header;
	uint32_t		vid;
	uint16_t		moving_speed;
} TPacketGCChangeSpeed;

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

enum
{
	QUEST_SEND_IS_BEGIN         = 1 << 0,
    QUEST_SEND_TITLE            = 1 << 1,  // 28자 까지
    QUEST_SEND_CLOCK_NAME       = 1 << 2,  // 16자 까지
    QUEST_SEND_CLOCK_VALUE      = 1 << 3,
    QUEST_SEND_COUNTER_NAME     = 1 << 4,  // 16자 까지
    QUEST_SEND_COUNTER_VALUE    = 1 << 5,
	QUEST_SEND_ICON_FILE		= 1 << 6,  // 24자 까지
};

typedef struct packet_quest_info
{
	uint8_t header;
	uint16_t size;
	uint16_t index;
#ifdef ENABLE_QUEST_RENEWAL
	uint16_t c_index;
#endif
	uint8_t flag;
#ifdef ENABLE_BUGFIXES_NOTDONE
	char	szTitle[30 + 1];
	uint8_t	isBegin;
	char	szClockName[16 + 1];
	int		iClockValue;
	char	szCounterName[16 + 1];
	int		iCounterValue;
	char	szIconFileName[24 + 1];
#endif
} TPacketGCQuestInfo;

typedef struct packet_quest_confirm
{
	uint8_t header;
    char msg[64+1];
	int32_t timeout;
    uint32_t requestPID;
} TPacketGCQuestConfirm;

typedef struct packet_attack
{
	uint8_t        header;
    uint32_t       dwVID;
    uint32_t       dwVictimVID;    // 적 VID
	uint8_t        bType;          // 공격 유형
} TPacketGCAttack;

typedef struct packet_c2c
{
	uint8_t		header;
	uint16_t		wSize;
} TPacketGCC2C;

typedef struct packetd_sync_position_element
{
    uint32_t       dwVID;
	int32_t        lX;
	int32_t        lY;
} TPacketGCSyncPositionElement;

typedef struct packetd_sync_position
{
	uint8_t        bHeader;
	uint16_t		wSize;
} TPacketGCSyncPosition;

typedef struct packet_ownership
{
    uint8_t                bHeader;
    uint32_t               dwOwnerVID;
    uint32_t               dwVictimVID;
} TPacketGCOwnership;

#define	SKILL_MAX_NUM 255

typedef struct packet_skill_level
{
	uint8_t        bHeader;
	uint8_t        abSkillLevels[SKILL_MAX_NUM];
} TPacketGCSkillLevel;

typedef struct SPlayerSkill
{
	uint8_t bMasterType;
	uint8_t bLevel;
	uint64_t tNextRead; // time_t
} TPlayerSkill;

typedef struct packet_skill_level_new
{
	uint8_t bHeader;
	TPlayerSkill skills[SKILL_MAX_NUM];
} TPacketGCSkillLevelNew;

// fly
typedef struct packet_fly
{
    uint8_t        bHeader;
    uint8_t        bType;
    uint32_t       dwStartVID;
    uint32_t       dwEndVID;
} TPacketGCCreateFly;

enum EPVPModes
{
	PVP_MODE_NONE,
    PVP_MODE_AGREE,
    PVP_MODE_FIGHT,
    PVP_MODE_REVENGE,
};

typedef struct packet_duel_start
{
	uint8_t	header ;
	uint16_t	wSize ;	// uint32_t가 몇개? 개수 = (wSize - sizeof(TPacketGCPVPList)) / 4
} TPacketGCDuelStart ;

typedef struct packet_pvp
{
	uint8_t		header;
	uint32_t		dwVIDSrc;
	uint32_t		dwVIDDst;
	uint8_t		bMode;
} TPacketGCPVP;

typedef struct packet_skill_cooltime_end
{
	uint8_t		header;
	uint8_t		bSkill;
} TPacketGCSkillCoolTimeEnd;

typedef struct packet_warp
{
	uint8_t			bHeader;
	int32_t			lX;
	int32_t			lY;
	uint32_t			lAddr;
	uint16_t			wPort;
} TPacketGCWarp;

typedef struct packet_party_invite
{
	uint8_t header;
    uint32_t leader_pid;
} TPacketGCPartyInvite;

typedef struct packet_party_add
{
	uint8_t header;
    uint32_t pid;
    char name[CHARACTER_NAME_MAX_LEN+1];
} TPacketGCPartyAdd;

typedef struct packet_party_update
{
	uint8_t header;
    uint32_t pid;
	uint8_t state;
	uint8_t percent_hp;
    short affects[PARTY_AFFECT_SLOT_MAX_NUM];
} TPacketGCPartyUpdate;

typedef struct packet_party_remove
{
	uint8_t header;
    uint32_t pid;
} TPacketGCPartyRemove;

typedef TPacketCGSafeboxCheckout TPacketGCSafeboxCheckout;
typedef TPacketCGSafeboxCheckin TPacketGCSafeboxCheckin;

typedef struct packet_safebox_wrong_password
{
	uint8_t        bHeader;
} TPacketGCSafeboxWrongPassword;

typedef struct packet_safebox_size
{
	uint8_t bHeader;
	uint8_t bSize;
} TPacketGCSafeboxSize;

typedef struct packet_safebox_money_change
{
	uint8_t bHeader;
    uint32_t dwMoney;
} TPacketGCSafeboxMoneyChange;


#define MOUNT_INVENTORY_SLOT_MAX_NUM 192
typedef struct packet_mount_inventory
{
	uint8_t bHeader;
	uint16_t size;
	uint8_t bWidth;
	uint8_t bHeight;
	uint16_t wCount;
} TPacketGCMountInventory;

typedef struct packet_mount_inventory_item
{
	uint16_t wSlot;
	uint32_t dwVnum;
	uint32_t dwCount;
	uint32_t alSockets[ITEM_SOCKET_SLOT_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
} TMountInventoryItemData;


typedef struct command_mount_inventory_checkin
{
	uint8_t bHeader;
	TItemPos	ItemPos;
	uint16_t	wMountPos;
} TPacketCGMountInventoryCheckin;

typedef struct command_mount_inventory_checkout
{
	uint8_t bHeader;
	uint16_t	wMountPos;
	TItemPos	ItemPos;
} TPacketCGMountInventoryCheckout;

typedef struct command_mount_inventory_item_move
{
	uint8_t bHeader;
	uint16_t	wMountPos;
	uint16_t	wDestPos;
} TPacketCGMountInventoryItemMove;

typedef struct command_empire
{
    uint8_t        bHeader;
	uint8_t        bEmpire;
} TPacketCGEmpire;

typedef struct packet_empire
{
    uint8_t        bHeader;
	uint8_t        bEmpire;
} TPacketGCEmpire;

enum
{
	FISHING_SUBHEADER_GC_START,
	FISHING_SUBHEADER_GC_STOP,
	FISHING_SUBHEADER_GC_REACT,
	FISHING_SUBHEADER_GC_SUCCESS,
	FISHING_SUBHEADER_GC_FAIL,
    FISHING_SUBHEADER_GC_FISH,
};

typedef struct packet_fishing
{
	uint8_t header;
	uint8_t subheader;
    uint32_t info;
	uint8_t dir;
} TPacketGCFishing;

typedef struct paryt_parameter
{
	uint8_t        bHeader;
	uint8_t        bDistributeMode;
} TPacketGCPartyParameter;

//////////////////////////////////////////////////////////////////////////
// Guild

enum
{
    GUILD_SUBHEADER_GC_LOGIN,
	GUILD_SUBHEADER_GC_LOGOUT,
	GUILD_SUBHEADER_GC_LIST,
	GUILD_SUBHEADER_GC_GRADE,
	GUILD_SUBHEADER_GC_ADD,
	GUILD_SUBHEADER_GC_REMOVE,
	GUILD_SUBHEADER_GC_GRADE_NAME,
	GUILD_SUBHEADER_GC_GRADE_AUTH,
	GUILD_SUBHEADER_GC_INFO,
	GUILD_SUBHEADER_GC_COMMENTS,
    GUILD_SUBHEADER_GC_CHANGE_EXP,
    GUILD_SUBHEADER_GC_CHANGE_MEMBER_GRADE,
	GUILD_SUBHEADER_GC_SKILL_INFO,
	GUILD_SUBHEADER_GC_CHANGE_MEMBER_GENERAL,
	GUILD_SUBHEADER_GC_GUILD_INVITE,
    GUILD_SUBHEADER_GC_WAR,
    GUILD_SUBHEADER_GC_GUILD_NAME,
    GUILD_SUBHEADER_GC_GUILD_WAR_LIST,
    GUILD_SUBHEADER_GC_GUILD_WAR_END_LIST,
    GUILD_SUBHEADER_GC_WAR_POINT,
	GUILD_SUBHEADER_GC_MONEY_CHANGE,
#ifdef ADVANCED_GUILD_INFO
	GUILD_SUBHEADER_GC_CHANGE_TROPHIES,
#endif
};

typedef struct packet_guild
{
	uint8_t header;
	uint16_t size;
	uint8_t subheader;
} TPacketGCGuild;

// SubHeader - Grade
enum
{
    GUILD_AUTH_ADD_MEMBER       = (1 << 0),
    GUILD_AUTH_REMOVE_MEMBER    = (1 << 1),
    GUILD_AUTH_NOTICE           = (1 << 2),
    GUILD_AUTH_SKILL            = (1 << 3),
};

typedef struct packet_guild_sub_grade
{
	char grade_name[GUILD_GRADE_NAME_MAX_LEN+1]; // 8+1 길드장, 길드원 등의 이름
	uint8_t auth_flag;
} TPacketGCGuildSubGrade;

typedef struct packet_guild_sub_member
{
	uint32_t pid;
	uint8_t byGrade;
	uint8_t byIsGeneral;
	uint8_t byJob;
	uint8_t byLevel;
	uint32_t dwOffer;
	uint8_t byNameFlag;
// if NameFlag is TRUE, name is sent from server.
//	char szName[CHARACTER_ME_MAX_LEN+1];
} TPacketGCGuildSubMember;

typedef struct packet_guild_sub_info
{
	uint16_t member_count;
	uint16_t max_member_count;
	uint32_t guild_id;
    uint32_t master_pid;
    uint32_t exp;
	uint8_t level;
    char name[GUILD_NAME_MAX_LEN+1];
	uint32_t gold;
	uint8_t hasLand;
#ifdef ADVANCED_GUILD_INFO
	int trophies;
	int win;
	int loss;
	int draw;
#endif
} TPacketGCGuildInfo;

enum EGuildWarState
{
    GUILD_WAR_NONE,
    GUILD_WAR_SEND_DECLARE,
    GUILD_WAR_REFUSE,
    GUILD_WAR_RECV_DECLARE,
    GUILD_WAR_WAIT_START,
    GUILD_WAR_CANCEL,
    GUILD_WAR_ON_WAR,
    GUILD_WAR_END,

    GUILD_WAR_DURATION = 2*60*60, // 2시간
};

typedef struct packet_guild_war
{
    uint32_t       dwGuildSelf;
    uint32_t       dwGuildOpp;
	uint8_t        bType;
	uint8_t        bWarState;
} TPacketGCGuildWar;

typedef struct SPacketGuildWarPoint
{
    uint32_t dwGainGuildID;
    uint32_t dwOpponentGuildID;
	int32_t lPoint;
} TPacketGuildWarPoint;

// SubHeader - Dungeon
enum
{
	DUNGEON_SUBHEADER_GC_TIME_ATTACK_START = 0,
	DUNGEON_SUBHEADER_GC_DESTINATION_POSITION = 1,
};

typedef struct packet_dungeon
{
	uint8_t		bHeader;
	uint16_t		size;
	uint8_t		subheader;
} TPacketGCDungeon;

// Private Shop
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
	int      iBeltCount;
} TPacketGCFakeShopSign;
#endif
#ifdef LEADERBOARD_RAZOR93
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

typedef struct SPacketGCTime
{
    uint8_t        bHeader;
	uint64_t      time; // time_t
} TPacketGCTime;

enum
{
    WALKMODE_RUN,
    WALKMODE_WALK,
};

typedef struct SPacketGCWalkMode
{
	uint8_t        header;
    uint32_t       vid;
	uint8_t        mode;
} TPacketGCWalkMode;

typedef struct SPacketGCChangeSkillGroup
{
	uint8_t        header;
	uint8_t        skill_group;
} TPacketGCChangeSkillGroup;

struct TMaterial
{
    uint32_t vnum;
    uint32_t count;
};

typedef struct SRefineTable
{
    uint32_t src_vnum;
    uint32_t result_vnum;
	uint8_t material_count;
    int64_t cost; // 소요 비용
    int prob; // 확률
    TMaterial materials[REFINE_MATERIAL_MAX_NUM];
} TRefineTable;

typedef struct SPacketGCRefineInformation
{
	uint8_t			header;
	uint8_t			pos;
	TRefineTable	refine_table;
} TPacketGCRefineInformation;

typedef struct SPacketGCRefineInformationNew
{
	uint8_t			header;
	uint8_t			type;
	uint8_t			pos;
	TRefineTable	refine_table;
} TPacketGCRefineInformationNew;

enum SPECIAL_EFFECT
{
	SE_NONE,
	SE_HPUP_RED,
	SE_SPUP_BLUE,
	SE_SPEEDUP_GREEN,
	SE_DXUP_PURPLE,
	SE_CRITICAL,
	SE_PENETRATE,
	SE_BLOCK,
	SE_DODGE,
	SE_CHINA_FIREWORK,
	SE_SPIN_TOP,
	SE_SUCCESS,
	SE_FAIL,
	SE_FR_SUCCESS,
    SE_LEVELUP_ON_14_FOR_GERMANY,	//레벨업 14일때 ( 독일전용 )
    SE_LEVELUP_UNDER_15_FOR_GERMANY,//레벨업 15일때 ( 독일전용 )
    SE_PERCENT_DAMAGE1,
    SE_PERCENT_DAMAGE2,
    SE_PERCENT_DAMAGE3,
	SE_AUTO_HPUP,
	SE_AUTO_SPUP,
	SE_EQUIP_RAMADAN_RING,			// 초승달의 반지를 착용하는 순간에 발동하는 이펙트
	SE_EQUIP_HALLOWEEN_CANDY,		// 할로윈 사탕을 착용(-_-;)한 순간에 발동하는 이펙트
	SE_EQUIP_HAPPINESS_RING,		// 크리스마스 행복의 반지를 착용하는 순간에 발동하는 이펙트
	SE_EQUIP_LOVE_PENDANT,		// 발렌타인 사랑의 팬던트(71145) 착용할 때 이펙트 (발동이펙트임, 지속이펙트 아님)
#ifdef ENABLE_ACCE_SYSTEM
	SE_EFFECT_ACCE_SUCCEDED,
	SE_EFFECT_ACCE_EQUIP,
#endif
#ifdef VERSION_162_ENABLED
	SE_EFFECT_HEALER,
#endif
#ifdef ENABLE_TALISMAN_EFFECT
	SE_EFFECT_TALISMAN_EQUIP_FIRE,
	SE_EFFECT_TALISMAN_EQUIP_ICE,
	SE_EFFECT_TALISMAN_EQUIP_WIND,
	SE_EFFECT_TALISMAN_EQUIP_EARTH,
	SE_EFFECT_TALISMAN_EQUIP_DARK,
	SE_EFFECT_TALISMAN_EQUIP_ELEC,
#endif
#ifdef __EFFETTO_MANTELLO__
	SE_MANTELLO,
#endif
};

typedef struct SPacketGCSpecialEffect
{
	uint8_t header;
	uint8_t type;
    uint32_t vid;
} TPacketGCSpecialEffect;

struct TNPCPosition
{
	uint8_t bType;
#ifdef ENABLE_MULTI_NAMES
	uint32_t	name;
#else
	char	name[CHARACTER_NAME_MAX_LEN + 1];
#endif
	int32_t x;
	int32_t y;
};

typedef struct SPacketGCNPCPosition
{
	uint8_t header;
	uint16_t size;
	uint16_t count;
} TPacketGCNPCPosition;



typedef struct SPacketGCChangeName
{
	uint8_t header;
    uint32_t pid;
    char name[CHARACTER_NAME_MAX_LEN+1];
} TPacketGCChangeName;

enum EBlockAction
{
    BLOCK_EXCHANGE              = (1 << 0),
    BLOCK_PARTY_INVITE          = (1 << 1),
    BLOCK_GUILD_INVITE          = (1 << 2),
    BLOCK_WHISPER               = (1 << 3),
    BLOCK_MESSENGER_INVITE      = (1 << 4),
    BLOCK_PARTY_REQUEST         = (1 << 5),
};

typedef struct packet_login_key
{
	uint8_t	bHeader;
	uint32_t	dwLoginKey;
} TPacketGCLoginKey;

typedef struct packet_auth_success
{
	uint8_t		bHeader;
	uint32_t		dwLoginKey;
	uint8_t		bResult;
} TPacketGCAuthSuccess;

typedef struct packet_channel
{
	uint8_t header;
	uint8_t channel;
} TPacketGCChannel;

typedef struct SEquipmentItemSet
{
	uint32_t   vnum;
	uint8_t    count;
	int32_t    alSockets[ITEM_SOCKET_SLOT_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
} TEquipmentItemSet;

typedef struct pakcet_view_equip
{
	uint8_t	header;
	uint32_t	dwVID;
#ifdef EQUIP_ENABLE_VIEW_SASH
		TEquipmentItemSet equips[23];
#else	
		TEquipmentItemSet equips[16];	
#endif
} TPacketGCViewEquip;

typedef struct
{
    uint32_t       dwID;
    int32_t        x, y;
	int32_t        width, height;
    uint32_t       dwGuildID;
} TLandPacketElement;

typedef struct packet_land_list
{
	uint8_t        header;
	uint16_t        size;
} TPacketGCLandList;

typedef struct
{
	uint8_t        bHeader;
	int32_t        lID;
    char        szTargetName[32+1];
} TPacketGCTargetCreate;

enum
{
	CREATE_TARGET_TYPE_NONE,
	CREATE_TARGET_TYPE_LOCATION,
	CREATE_TARGET_TYPE_CHARACTER,
};

typedef struct
{
	uint8_t		bHeader;
	int32_t		lID;
	char		szTargetName[32+1];
	uint32_t		dwVID;
	uint8_t		byType;
} TPacketGCTargetCreateNew;

typedef struct
{
	uint8_t        bHeader;
	int32_t        lID;
	int32_t        lX, lY;
} TPacketGCTargetUpdate;

typedef struct
{
	uint8_t        bHeader;
	int32_t        lID;
} TPacketGCTargetDelete;

typedef struct
{
    uint32_t       dwType;
    uint8_t        bPointIdxApplyOn;
    int32_t        lApplyValue;
    uint32_t       dwFlag;
	int32_t        lDuration;
	int32_t        lSPCost;
} TPacketAffectElement;

typedef struct
{
    uint8_t bHeader;
    TPacketAffectElement elem;
} TPacketGCAffectAdd;

typedef struct
{
    uint8_t bHeader;
    uint32_t dwType;
    uint8_t bApplyOn;
} TPacketGCAffectRemove;

typedef struct packet_mall_open
{
	uint8_t bHeader;
	uint8_t bSize;
} TPacketGCMallOpen;

typedef struct packet_lover_info
{
	uint8_t bHeader;
	char szName[CHARACTER_NAME_MAX_LEN + 1];
	uint8_t byLovePoint;
} TPacketGCLoverInfo;

typedef struct packet_love_point_update
{
	uint8_t bHeader;
	uint8_t byLovePoint;
} TPacketGCLovePointUpdate;

typedef struct packet_dig_motion
{
    uint8_t header;
    uint32_t vid;
    uint32_t target_vid;
	uint8_t count;
} TPacketGCDigMotion;

typedef struct SPacketGCOnTime
{
    uint8_t header;
    int ontime;     // sec
} TPacketGCOnTime;

typedef struct SPacketGCResetOnTime
{
    uint8_t header;
} TPacketGCResetOnTime;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Client To Client

typedef struct packet_state
{
	uint8_t			bHeader;
	uint8_t			bFunc;
	uint8_t			bArg;
	uint8_t			bRot;
	uint32_t			dwVID;
	uint32_t			dwTime;
	TPixelPosition	kPPos;
} TPacketCCState;

// AUTOBAN
typedef struct packet_autoban_quiz
{
	uint8_t bHeader;
	uint8_t bDuration;
	uint8_t bCaptcha[64*32];
    char szQuiz[256];
} TPacketGCAutoBanQuiz;
// END_OF_AUTOBAN

#ifdef _IMPROVED_PACKET_ENCRYPTION_
struct TPacketKeyAgreement
{
	static const int MAX_DATA_LEN = 256;
	uint8_t bHeader;
	uint16_t wAgreedLength;
	uint16_t wDataLength;
	uint8_t data[MAX_DATA_LEN];
};

struct TPacketKeyAgreementCompleted
{
	uint8_t bHeader;
	uint8_t data[3]; // dummy (not used)
};
#endif // _IMPROVED_PACKET_ENCRYPTION_

typedef struct SPacketGCSpecificEffect
{
	uint8_t header;
	uint32_t vid;
	char effect_file[128];
} TPacketGCSpecificEffect;

// 용혼석
enum EDragonSoulRefineWindowRefineType
{
	DragonSoulRefineWindow_UPGRADE,
	DragonSoulRefineWindow_IMPROVEMENT,
	DragonSoulRefineWindow_REFINE,
};

enum EPacketCGDragonSoulSubHeaderType
{
	DS_SUB_HEADER_OPEN,
	DS_SUB_HEADER_CLOSE,
	DS_SUB_HEADER_DO_UPGRADE,
	DS_SUB_HEADER_DO_IMPROVEMENT,
	DS_SUB_HEADER_DO_REFINE,
	DS_SUB_HEADER_REFINE_FAIL,
	DS_SUB_HEADER_REFINE_FAIL_MAX_REFINE,
	DS_SUB_HEADER_REFINE_FAIL_INVALID_MATERIAL,
	DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MONEY,
	DS_SUB_HEADER_REFINE_FAIL_NOT_ENOUGH_MATERIAL,
	DS_SUB_HEADER_REFINE_FAIL_TOO_MUCH_MATERIAL,
	DS_SUB_HEADER_REFINE_SUCCEED,
};

typedef struct SPacketCGDragonSoulRefine
{
	SPacketCGDragonSoulRefine() : header(HEADER_CG_DRAGON_SOUL_REFINE), bSubType(0)
	{
	}

	uint8_t header;
	uint8_t bSubType;
	TItemPos ItemGrid[DS_REFINE_WINDOW_MAX_NUM];
} TPacketCGDragonSoulRefine;

#ifdef ENABLE_DS_REFINE_ALL
typedef struct SPacketDragonSoulRefineAll {
	uint8_t header, subheader, type, grade;
} TPacketDragonSoulRefineAll;
#endif

typedef struct SPacketGCDragonSoulRefine
{
	SPacketGCDragonSoulRefine() : header(HEADER_GC_DRAGON_SOUL_REFINE), bSubType(0)
	{
	}

	uint8_t header;
	uint8_t bSubType;
	TItemPos Pos;
} TPacketGCDragonSoulRefine;

typedef struct SChannelStatus
{
	uint16_t nPort;
	uint8_t bStatus;
} TChannelStatus;

#ifdef ENABLE_RANKING
enum
{
	MAX_RANKING_LIST = 51,
	HEADER_GC_RANKING_SEND = 218,
};

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
#endif

#ifdef ENABLE_ACCE_SYSTEM
enum EAcceInfo
{
	ACCE_ABSORPTION_SOCKET = 0,
	ACCE_ABSORBED_SOCKET = 1,
	ACCE_CLEAN_ATTR_VALUE0 = 7,
	ACCE_WINDOW_MAX_MATERIALS = 2,
};

enum
{
	HEADER_GC_ACCE = 215,
	ACCE_SUBHEADER_GC_OPEN = 0,
	ACCE_SUBHEADER_GC_CLOSE,
	ACCE_SUBHEADER_GC_ADDED,
	ACCE_SUBHEADER_GC_REMOVED,
	ACCE_SUBHEADER_CG_REFINED,
	ACCE_SUBHEADER_CG_CLOSE = 0,
	ACCE_SUBHEADER_CG_ADD,
	ACCE_SUBHEADER_CG_REMOVE,
	ACCE_SUBHEADER_CG_REFINE,
};

typedef struct SPacketAcce
{
	uint8_t	header;
	uint8_t	subheader;
	bool	bWindow;
	uint32_t	dwPrice;
	uint8_t	bPos;
	TItemPos	tPos;
	uint32_t	dwItemVnum;
	uint32_t	dwMinAbs;
	uint32_t	dwMaxAbs;
} TPacketAcce;

typedef struct SAcceMaterial
{
	uint8_t	bHere;
	uint16_t	wCell;
} TAcceMaterial;

typedef struct SAcceResult
{
	uint32_t	dwItemVnum;
	uint32_t	dwMinAbs;
	uint32_t	dwMaxAbs;
} TAcceResult;
#endif


// @fixme007 length 2
typedef struct packet_unk_213
{
	uint8_t bHeader;
	uint8_t bUnk2;
} TPacketGCUnk213;
#ifdef __ENABLE_NEW_OFFLINESHOP__

//ACTIONS PACKETS
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
	uint8_t bHeader;
	uint16_t wSize;
	uint8_t bSubHeader;
} TPacketCGNewOfflineShop;





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
		long long	illYang;
#ifdef ENABLE_CHEQUE_SYSTEM
		int			iCheque;
#endif

		SPriceInfo() : illYang(0)
#ifdef ENABLE_CHEQUE_SYSTEM
			,iCheque(0)
#endif
		{}

	} TPriceInfo;

	typedef struct
	{
		uint32_t	dwVnum;
		uint32_t	dwCount;
		int32_t	alSockets[ITEM_SOCKET_SLOT_MAX_NUM];
		TPlayerItemAttribute    aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];

#ifdef ENABLE_CHANGELOOK_SYSTEM
		uint32_t	dwTransmutation;
#endif
#ifdef ATTR_LOCK
		int		iLockedAttr;
#endif

		//patch 08-03-2020
		ExpirationType expiration;
	} TItemInfoEx;

	typedef struct TItemInfo
	{
		uint32_t dwOwnerID;
		uint32_t dwItemID;
		TPriceInfo price;
		TItemInfoEx item;

		TItemInfo() : dwOwnerID(0), dwItemID(0)
		{
			memset(&price, 0, sizeof(price));
			memset(&item, 0, sizeof(item));
		}
	} TItemInfo;

	typedef struct TOfferInfo
	{
		uint32_t dwOfferID;
		uint32_t dwOwnerID;
		uint32_t dwItemID;
		uint32_t dwOffererID;

		TPriceInfo price;
		bool bNoticed;
		bool bAccepted;

		char szBuyerName[CHARACTER_NAME_MAX_LEN + 1];

		TOfferInfo() : dwOfferID(0), dwOwnerID(0), dwItemID(0), dwOffererID(0), bNoticed(false), bAccepted(false)
		{
			memset(&price, 0, sizeof(price));
			memset(szBuyerName, 0, sizeof(szBuyerName));
		}
	} TOfferInfo;



	//offlineshop-updated 03/08/19
	typedef struct TMyOfferExtraInfo
	{
		TItemInfo item;
		char szShopName[OFFLINE_SHOP_NAME_MAX_LEN];

		TMyOfferExtraInfo()
		{
			item = TItemInfo();
			memset(szShopName, 0, sizeof(szShopName));
		}
	} TMyOfferExtraInfo;

	typedef struct SValutesInfoa
	{
		long long	illYang;
#ifdef ENABLE_CHEQUE_SYSTEM
		int			iCheque;
#endif


		void operator +=(const SValutesInfoa& r)
		{
			illYang += r.illYang;
#ifdef ENABLE_CHEQUE_SYSTEM
			iCheque += r.iCheque;
#endif
		}

		void operator -=(const SValutesInfoa& r)
		{
			illYang -= r.illYang;
#ifdef ENABLE_CHEQUE_SYSTEM
			iCheque -= r.iCheque;
#endif
		}

		SValutesInfoa() : illYang(0)
#ifdef ENABLE_CHEQUE_SYSTEM
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



	typedef struct filter {
		uint8_t		bType;
		uint8_t		bSubType;

		char		szName[OFFLINE_SHOP_ITEM_MAX_LEN];
		//char		szOwnerName[CHARACTER_NAME_MAX_LEN + 1];
		TPriceInfo	priceStart, priceEnd;
		int			iLevelStart, iLevelEnd;

		uint32_t		dwWearFlag;
		TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_NORM_NUM];
		int			iRarity = -1;

		filter() : bType(0), bSubType(0), iLevelStart(0), iLevelEnd(0), dwWearFlag(0), iRarity(0)
		{
			memset(szName, 0, sizeof(szName));
			//memset(szOwnerName, 0, sizeof(szOwnerName));
			memset(&priceStart, 0, sizeof(priceStart));
			memset(&priceEnd, 0, sizeof(priceEnd));
			memset(aAttr, 0, sizeof(aAttr));
		}
	}TFilterInfo;


	typedef struct {
		TItemPos	pos;
		TPriceInfo	price;
	}TShopItemInfo;



	//AUCTION
	typedef struct TAuctionInfo
	{
		uint32_t dwOwnerID;
		char szOwnerName[CHARACTER_NAME_MAX_LEN + 1];
		uint32_t dwDuration;

		TPriceInfo init_price;
		TItemInfoEx item;

		TAuctionInfo()
			: dwOwnerID(0), dwDuration(0)
		{
			memset(szOwnerName, 0, sizeof(szOwnerName));
			memset(&init_price, 0, sizeof(init_price));
			memset(&item, 0, sizeof(item));
		}
	} TAuctionInfo;



	typedef struct TAuctionOfferInfo
	{
		TPriceInfo price;
		uint32_t   dwOwnerID;
		uint32_t   dwBuyerID;
		char       szBuyerName[CHARACTER_NAME_MAX_LEN + 1];

		TAuctionOfferInfo() : dwOwnerID(0), dwBuyerID(0)
		{
			memset(&price, 0, sizeof(price));
			memset(szBuyerName, 0, sizeof(szBuyerName));
		}
	} TAuctionOfferInfo;



	typedef struct TAuctionListElement
	{
		TAuctionInfo auction;
		TPriceInfo   actual_best;
		uint32_t     dwOfferCount;

		TAuctionListElement() : dwOfferCount(0)
		{
			auction = TAuctionInfo();
			memset(&actual_best, 0, sizeof(actual_best));
		}
	} TAuctionListElement;







	//GAME TO CLIENT
	enum eSubHeaderGC
	{
		SUBHEADER_GC_SHOP_LIST,
		SUBHEADER_GC_SHOP_OPEN,
		SUBHEADER_GC_SHOP_OPEN_OWNER,
		SUBHEADER_GC_SHOP_OPEN_OWNER_NO_SHOP,
		SUBHEADER_GC_SHOP_CLOSE,
		SUBHEADER_GC_SHOP_BUY_ITEM_FROM_SEARCH,

		SUBHEADER_GC_OFFER_LIST,

		SUBHEADER_GC_SHOP_FILTER_RESULT,
		SUBHEADER_GC_SHOP_SAFEBOX_REFRESH,

		//AUCTION
		SUBHEADER_GC_AUCTION_LIST,
		SUBHEADER_GC_OPEN_MY_AUCTION,
		SUBHEADER_GC_OPEN_MY_AUCTION_NO_AUCTION,
		SUBHEADER_GC_OPEN_AUCTION,
#ifdef ENABLE_NEW_SHOP_IN_CITIES
		SUBHEADER_GC_INSERT_SHOP_ENTITY,
		SUBHEADER_GC_REMOVE_SHOP_ENTITY,
#endif

	};


	typedef struct {
		uint32_t	dwShopCount;
	} TSubPacketGCShopList;


	typedef struct {
		TShopInfo	shop;

	} TSubPacketGCShopOpen;


	typedef struct {
		TShopInfo	shop;
		uint32_t		dwSoldCount;
		uint32_t		dwOfferCount;

	} TSubPacketGCShopOpenOwner;



	typedef struct {
		uint32_t dwCount;
	} TSubPacketGCShopFilterResult;


	typedef struct {
		uint32_t dwOfferCount;
		
	} TSubPacketGCShopOfferList;


	typedef struct {
		TValutesInfo	valute;
		uint32_t			dwItemCount;

	}TSubPacketGCShopSafeboxRefresh;

	typedef struct {
		uint32_t dwOwnerID;
		uint32_t dwItemID;
	}TSubPacketGCShopBuyItemFromSearch;



	//AUCTION
	typedef struct {
		uint32_t dwCount;
		bool bOwner;
	}TSubPacketGCAuctionList;



	typedef struct {
		TAuctionInfo auction;
		uint32_t dwOfferCount;


	}TSubPacketGCAuctionOpen;



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



	// CLIENT TO GAME
	enum eSubHeaderCG
	{
		SUBHEADER_CG_SHOP_CREATE_NEW,
		SUBHEADER_CG_SHOP_CHANGE_NAME,
		SUBHEADER_CG_SHOP_FORCE_CLOSE,
		SUBHEADER_CG_SHOP_REQUEST_SHOPLIST,
		SUBHEADER_CG_SHOP_OPEN,
		SUBHEADER_CG_SHOP_OPEN_OWNER,
		SUBHEADER_CG_SHOP_BUY_ITEM,

		SUBHEADER_CG_SHOP_ADD_ITEM,
		SUBHEADER_CG_SHOP_REMOVE_ITEM,
		SUBHEADER_CG_SHOP_EDIT_ITEM,

		SUBHEADER_CG_SHOP_FILTER_REQUEST,

		SUBHEADER_CG_SHOP_OFFER_CREATE,
		SUBHEADER_CG_SHOP_OFFER_ACCEPT,
		SUBHEADER_CG_SHOP_OFFER_CANCEL,
		SUBHEADER_CG_SHOP_REQUEST_OFFER_LIST,

		SUBHEADER_CG_SHOP_SAFEBOX_OPEN,
		SUBHEADER_CG_SHOP_SAFEBOX_GET_ITEM,
		SUBHEADER_CG_SHOP_SAFEBOX_GET_VALUTES,
		SUBHEADER_CG_SHOP_SAFEBOX_CLOSE,


		//AUCTION
		SUBHEADER_CG_AUCTION_LIST_REQUEST,
		SUBHEADER_CG_AUCTION_OPEN_REQUEST,
		SUBHEADER_CG_MY_AUCTION_OPEN_REQUEST,
		SUBHEADER_CG_CREATE_AUCTION,
		SUBHEADER_CG_AUCTION_ADD_OFFER,
		SUBHEADER_CG_EXIT_FROM_AUCTION,


		SUBHEADER_CG_CLOSE_BOARD,

#ifdef ENABLE_NEW_SHOP_IN_CITIES
		SUBHEADER_CG_CLICK_ENTITY,
#endif
		SUBHEADER_CG_AUCTION_CLOSE,
	};




	typedef struct
	{
		TShopInfo shop;
	}TSubPacketCGShopCreate;


	typedef struct
	{
		char szName[OFFLINE_SHOP_NAME_MAX_LEN];
	}TSubPacketCGShopChangeName;



	typedef struct
	{
		uint32_t dwOwnerID;
	}TSubPacketCGShopOpen;



	typedef struct
	{
		TItemPos	pos;
		TPriceInfo  price;
	}TSubPacketCGAddItem;


	typedef struct
	{
		uint32_t dwItemID;
	}TSubPacketCGRemoveItem;



	typedef struct
	{
		uint32_t dwItemID;
		TPriceInfo price;
	}TSubPacketCGEditItem;



	typedef struct
	{
		TFilterInfo filter;
	}TSubPacketCGFilterRequest;



	typedef struct
	{
		TOfferInfo offer;
	}TSubPacketCGOfferCreate;


	typedef struct
	{
		uint32_t dwOfferID;
	}TSubPacketCGOfferAccept;


	typedef struct
	{
		uint32_t dwOfferID;
		uint32_t dwOwnerID;
	}TSubPacketCGOfferCancel;


	typedef struct
	{
		uint32_t dwItemID;
	}TSubPacketCGShopSafeboxGetItem;


	typedef struct
	{
		TValutesInfo valutes;
	}TSubPacketCGShopSafeboxGetValutes;

	typedef struct
	{
		uint32_t dwOwnerID;
		uint32_t dwItemID;
		bool  bIsSearch;
		long long TotalPriceSeen;
	}TSubPacketCGShopBuyItem;



	//AUCTION
	typedef struct {
		uint32_t dwOwnerID;
	} TSubPacketCGAuctionOpenRequest;

	typedef struct {
		uint32_t		dwDuration;
		TItemPos	pos;

		TPriceInfo	init_price;
	} TSubPacketCGAuctionCreate;


	typedef struct {
		uint32_t		dwOwnerID;
		TPriceInfo	price;
	}TSubPacketCGAuctionAddOffer;

	typedef struct {
		uint32_t dwOwnerID;
	} TSubPacketCGAuctionExitFrom;


#ifdef ENABLE_NEW_SHOP_IN_CITIES
	typedef struct {
		uint32_t dwShopVID;
	} TSubPacketCGShopClickEntity;
#endif
}

#endif
#ifdef ENABLE_SWITCHBOT
enum ECGSwitchbotSubheader
{
	SUBHEADER_CG_SWITCHBOT_START,
	SUBHEADER_CG_SWITCHBOT_STOP,
};

struct TPacketCGSwitchbot
{
	uint8_t header;
	int size;
	uint8_t subheader;
	uint8_t slot;
};

enum EGCSwitchbotSubheader
{
	SUBHEADER_GC_SWITCHBOT_UPDATE,
	SUBHEADER_GC_SWITCHBOT_UPDATE_ITEM,
	SUBHEADER_GC_SWITCHBOT_SEND_ATTRIBUTE_INFORMATION,
};

struct TPacketGCSwitchbot
{
	uint8_t header;
	int size;
	uint8_t subheader;
	uint8_t slot;
};

struct TSwitchbotUpdateItem
{
	uint8_t	slot;
	uint8_t	vnum;
	uint8_t	count;
	int32_t	alSockets[ITEM_SOCKET_SLOT_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
};
#endif
#ifdef ENABLE_MAP_TELEPORTER
typedef struct SPacketCGMapTeleporter
{
	uint8_t	bHeader;
	int		iMapCode;

} TPacketCGMapTeleporter;
#endif



#ifdef ENABLE_CUBE_RENEWAL_WORLDARD

enum
{
	CUBE_RENEWAL_SUB_HEADER_OPEN_RECEIVE,
	CUBE_RENEWAL_SUB_HEADER_CLEAR_DATES_RECEIVE,
	CUBE_RENEWAL_SUB_HEADER_DATES_RECEIVE,
	CUBE_RENEWAL_SUB_HEADER_DATES_LOADING,

	CUBE_RENEWAL_SUB_HEADER_MAKE_ITEM,
	CUBE_RENEWAL_SUB_HEADER_CLOSE,
};


typedef struct  packet_send_cube_renewal
{
	uint8_t header;
	uint8_t subheader;
	uint32_t index_item;
	uint32_t count_item;
	uint32_t index_item_improve;
}TPacketCGCubeRenewalSend;



typedef struct dates_cube_renewal
{
	uint32_t npc_vnum;
	uint32_t index;

	uint32_t	vnum_reward;
	int		count_reward;

	bool 	item_reward_stackable;

	uint32_t	vnum_material_1;
	int		count_material_1;

	uint32_t	vnum_material_2;
	int		count_material_2;

	uint32_t	vnum_material_3;
	int		count_material_3;

	uint32_t	vnum_material_4;
	int		count_material_4;

	uint32_t	vnum_material_5;
	int		count_material_5;

	int64_t 	gold;
	int 	percent;

#ifdef ENABLE_GAYA_SYSTEM
	int 	gaya;
#endif
#ifdef ENABLE_CUBE_RENEWAL_COPY_WORLDARD
	uint32_t   allowCopy;
#endif
	
	char 	category[100];
}TInfoDateCubeRenewal;

typedef struct packet_receive_cube_renewal
{
	packet_receive_cube_renewal() : header(HEADER_GC_CUBE_RENEWAL), subheader(0), date_cube_renewal()
	{
	}

	uint8_t header;
	uint8_t subheader;
	TInfoDateCubeRenewal	date_cube_renewal;
}TPacketGCCubeRenewalReceive;
#endif

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
typedef struct SPacketCGWhisperAdmin
{
	uint8_t	header;
	char	szText[512 + 1];
	char	szLang[2 + 1];
	int		color;
} TPacketCGWhisperAdmin;

typedef struct SPacketCGGetWhisperDetails {
	uint8_t	header;
	char	name[CHARACTER_NAME_MAX_LEN + 1];
} TPacketCGGetWhisperDetails;

typedef struct SPacketGCGetWhisperDetails {
	uint8_t	header;
	char	name[CHARACTER_NAME_MAX_LEN + 1];
	uint8_t	bLanguage;
	uint8_t	bEmpire;
} TPacketGCGetWhisperDetails;

#endif

#ifdef ENABLE_MULTI_LANGUAGE
enum
{

	HEADER_GC_REQUEST_CHANGE_LANGUAGE = 140,
	HEADER_GC_RECV_LANGUAGE = 141,
};

typedef struct SPacketRequestLang
{
	uint8_t	bHeader;
	char	targetName[CHARACTER_NAME_MAX_LEN + 1];
} TPacketRequestLang;

typedef struct SPacketRecvLang
{
	uint8_t	bHeader;
	char	targetName[CHARACTER_NAME_MAX_LEN + 1];
	char	targetLanguage[2 + 1];
} TPacketRecvLang;

typedef struct SPacketChangeLanguage
{
	uint8_t	bHeader;
	uint8_t	bLanguage;
} TPacketChangeLanguage;
#endif

#if defined(ENABLE_PLAYER_PIN_SYSTEM)
enum
{
	HEADER_CG_PLAYER_PIN_CODE = 14,
	HEADER_GC_PLAYER_PIN_CODE = 108,
};

typedef struct SPacketCGCharacterPinCode
{
	uint8_t bHeader;
	uint8_t bIndex;
	char szPinCode[PIN_CODE_LENGTH + 1];
} TPacketCGCharacterPinCode;

typedef struct SPacketGCCharacterPinCode
{
	uint8_t bHeader;
	bool bVerified;
} TPacketGCCharacterPinCode;
#endif

#ifdef ENABLE_SKILL_COLOR_SYSTEM
typedef struct packet_skill_color
{
	uint8_t		bheader;
	uint8_t		skill;
	uint32_t		col1;
	uint32_t		col2;
	uint32_t		col3;
	uint32_t		col4;
	uint32_t		col5;
}TPacketCGSkillColor;
#endif

#ifdef ENABLE_BATTLE_PASS
enum
{
	HEADER_GC_BATTLE_PASS_OPEN = 160,
	HEADER_GC_BATTLE_PASS_UPDATE = 161,
	HEADER_GC_BATTLE_PASS_RANKING = 162,
};

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

typedef struct SPacketCGBattlePassAction
{
	uint8_t	bHeader;
	uint8_t	bAction;
} TPacketCGBattlePassAction;

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

#ifdef TEXTS_IMPROVEMENT
typedef struct SPacketGCChatNew {
	uint8_t header;
	uint8_t type;
	uint32_t idx;
	uint16_t size;
} TPacketGCChatNew;
#endif

#ifdef ENABLE_NEW_FISHING_SYSTEM
enum {
	FISHING_SUBHEADER_NEW_START = 0,
	FISHING_SUBHEADER_NEW_STOP = 1,
	FISHING_SUBHEADER_NEW_CATCH = 2,
	FISHING_SUBHEADER_NEW_CATCH_FAIL = 3,
	FISHING_SUBHEADER_NEW_CATCH_SUCCESS = 4,
	FISHING_SUBHEADER_NEW_CATCH_FAILED = 5,
};

typedef struct SPacketFishingNew
{
	uint8_t header;
	uint8_t subheader;
	uint32_t vid;
	int dir;
	uint8_t need;
	uint8_t count;
} TPacketFishingNew;
#endif

#ifdef ENABLE_OPENSHOP_PACKET
typedef struct SPacketOpenShop {
	uint8_t header;
	int32_t shopid;
} TPacketOpenShop;
#endif
#if defined(ENABLE_CHRISTMAS_WHEEL_OF_DESTINY)
typedef struct command_wheel
{
	uint8_t	header;
	uint8_t option;
} TPacketCGWheelDestiny;
#endif
#ifdef ENABLE_EVENT_MANAGER
typedef struct SPacketGCEventManager
{
	uint8_t	header;
	uint32_t	size;
} TPacketGCEventManager;
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
	int32_t alSocket[ITEM_SOCKET_SLOT_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
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
typedef struct SPacketGCItemShop
{
	uint8_t		header;
	uint32_t	size;
} TPacketGCItemShop;

#endif


#pragma pack(pop)
