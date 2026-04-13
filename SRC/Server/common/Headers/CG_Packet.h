#pragma once
#include "../CommonDefines.h"
#include "../service.h"

#pragma pack(1)

typedef struct command_text
{
	uint8_t	bHeader;
} TPacketCGText;

typedef struct command_handshake
{
	uint8_t	bHeader;
	uint32_t	dwHandshake;
	uint32_t	dwTime;
	int32_t	lDelta;
} TPacketCGHandshake;

typedef struct command_login
{
	uint8_t	header;
	char	login[LOGIN_MAX_LEN + 1];
	char	passwd[PASSWD_MAX_LEN + 1];
} TPacketCGLogin;

typedef struct command_login2
{
	uint8_t	header;
	char	login[LOGIN_MAX_LEN + 1];
	uint32_t	dwLoginKey;
	uint32_t	adwClientKey[4];
} TPacketCGLogin2;

typedef struct command_login3
{
	uint8_t	header;
	char	login[LOGIN_MAX_LEN + 1];
	char	passwd[PASSWD_MAX_LEN + 1];
	uint32_t	adwClientKey[4];
#ifdef ENABLE_HWID
	char hwid[HWID_LENGTH + 1];
#endif
} TPacketCGLogin3;

typedef struct command_login5
{
	uint8_t	header;
	char	authKey[OPENID_AUTHKEY_LEN + 1];
	uint32_t	adwClientKey[4];
} TPacketCGLogin5;

#ifdef __NEWPET_SYSTEM__
typedef struct packet_RequestPetName
{
	uint8_t byHeader;
	char petname[13];

}TPacketCGRequestPetName;
#endif

typedef struct packet_login_key
{
	uint8_t	bHeader;
	uint32_t	dwLoginKey;
} TPacketGCLoginKey;

typedef struct command_player_select
{
	uint8_t	header;
	uint8_t	index;
} TPacketCGPlayerSelect;

typedef struct command_player_delete
{
	uint8_t	header;
	uint8_t	index;
	char	private_code[8];
} TPacketCGPlayerDelete;

typedef struct command_player_create
{
	uint8_t	header;
	uint8_t	index;
	char	name[CHARACTER_NAME_MAX_LEN + 1];
	uint16_t	job;
	uint8_t	shape;
	uint8_t	Con;
	uint8_t	Int;
	uint8_t	Str;
	uint8_t	Dex;
} TPacketCGPlayerCreate;

typedef struct command_attack
{
	uint8_t	bHeader;
	uint8_t	bType;
	uint32_t	dwVID;
	uint8_t	bCRCMagicCubeProcPiece;
	uint8_t	bCRCMagicCubeFilePiece;
} TPacketCGAttack;

typedef struct command_move
{
	uint8_t	bHeader;
	uint8_t	bFunc;
	uint8_t	bArg;
	float	bRot;
	int32_t	lX;
	int32_t	lY;
	uint32_t	dwTime;
} TPacketCGMove;

typedef struct command_sync_position_element
{
	uint32_t	dwVID;
	int32_t	lX;
	int32_t	lY;
} TPacketCGSyncPositionElement;

typedef struct command_sync_position	// 가변 패킷
{
	uint8_t	bHeader;
	uint16_t	wSize;
} TPacketCGSyncPosition;

typedef struct command_chat	// 가변 패킷
{
	uint8_t	header;
	uint16_t	size;
	uint8_t	type;
} TPacketCGChat;

typedef struct command_whisper
{
	uint8_t	bHeader;
	uint16_t	wSize;
	char 	szNameTo[CHARACTER_NAME_MAX_LEN + 1];
} TPacketCGWhisper;

typedef struct command_entergame
{
	uint8_t	header;
} TPacketCGEnterGame;

typedef struct command_item_use
{
	uint8_t 	header;
	TItemPos 	Cell;
} TPacketCGItemUse;

typedef struct command_item_use_to_item
{
	uint8_t	header;
	TItemPos	Cell;
	TItemPos	TargetCell;
} TPacketCGItemUseToItem;

typedef struct command_item_drop
{
	uint8_t 	header;
	TItemPos 	Cell;
	int64_t	gold;
} TPacketCGItemDrop;

typedef struct command_item_drop2
{
	uint8_t 	header;
	TItemPos 	Cell;
	int64_t	gold;
#ifdef ENABLE_NEW_STACK_LIMIT
	int
#else
	uint8_t
#endif
		count;
} TPacketCGItemDrop2;

typedef struct command_item_destroy
{
	uint8_t		header;
	TItemPos	Cell;
} TPacketCGItemDestroy;

typedef struct command_item_division
{
	uint8_t		header;
	TItemPos	pos;
} TPacketCGItemDivision;

typedef struct command_item_move
{
	uint8_t 	header;
	TItemPos	Cell;
	TItemPos	CellTo;
#ifdef ENABLE_NEW_STACK_LIMIT
	int
#else
	uint8_t
#endif
		count;
} TPacketCGItemMove;

#ifdef __ENABLE_EXTEND_INVEN_SYSTEM__
typedef struct envanter_paketi
{
	uint8_t	header;
} TPacketCGEnvanter;
#endif

typedef struct command_item_pickup
{
	uint8_t 	header;
	uint32_t	vid;
} TPacketCGItemPickup;

typedef struct command_quickslot_add
{
	uint8_t	header;
	uint8_t	pos;
	TQuickslot	slot;
} TPacketCGQuickslotAdd;

typedef struct command_quickslot_del
{
	uint8_t	header;
	uint8_t	pos;
} TPacketCGQuickslotDel;

typedef struct command_quickslot_swap
{
	uint8_t	header;
	uint8_t	pos;
	uint8_t	change_pos;
} TPacketCGQuickslotSwap;

typedef struct command_shop_buy
{
	uint8_t	count;
} TPacketCGShopBuy;

typedef struct command_shop_sell
{
	uint8_t	pos;
	uint8_t	count;
} TPacketCGShopSell;

typedef struct command_shop
{
	uint8_t	header;
	uint8_t	subheader;
} TPacketCGShop;

typedef struct command_on_click
{
	uint8_t	header;
	uint32_t	vid;
} TPacketCGOnClick;

typedef struct command_exchange
{
	uint8_t	header;
	uint8_t	sub_header;
	int64_t arg1;

	uint8_t arg2;
	TItemPos	Pos;
} TPacketCGExchange;

typedef struct command_position
{
	uint8_t	header;
	uint8_t	position;
} TPacketCGPosition;

typedef struct command_script_answer
{
	uint8_t	header;
	uint8_t	answer;
	//char	file[32 + 1];
	//uint8_t	answer[16 + 1];
} TPacketCGScriptAnswer;


typedef struct command_script_button
{
	uint8_t        header;
	uint32_t	idx;
} TPacketCGScriptButton;

typedef struct command_quest_input_string
{
	uint8_t header;
	char msg[64 + 1];
} TPacketCGQuestInputString;

typedef struct command_quest_confirm
{
	uint8_t header;
	uint8_t answer;
	uint32_t requestPID;
} TPacketCGQuestConfirm;

#ifdef ENABLE_WHISPER_ADMIN_SYSTEM
typedef struct SPacketCGWhisperAdmin
{
	uint8_t	header;
	char	szText[512 + 1];
	char	szLang[2 + 1];
	int		color;
} TPacketCGWhisperAdmin;

#endif

typedef struct command_fly_targeting
{
	uint8_t		bHeader;
	uint32_t		dwTargetVID;
	int32_t		x, y;
} TPacketCGFlyTargeting;

typedef struct packet_shoot
{
	uint8_t		bHeader;
	uint8_t		bType;
} TPacketCGShoot;

typedef struct command_use_skill
{
	uint8_t	bHeader;
	uint32_t	dwVnum;
	uint32_t	dwVID;
} TPacketCGUseSkill;

typedef struct command_target
{
	uint8_t	header;
	uint32_t	dwVID;
} TPacketCGTarget;

#ifdef __SEND_TARGET_INFO__
typedef struct packet_target_info_load
{
	uint8_t header;
	uint32_t dwVID;
} TPacketCGTargetInfoLoad;
#endif

typedef struct command_warp
{
	uint8_t	bHeader;
} TPacketCGWarp;

typedef struct command_messenger
{
	uint8_t header;
	uint8_t subheader;
} TPacketCGMessenger;

typedef struct command_messenger_add_by_vid
{
	uint32_t vid;
} TPacketCGMessengerAddByVID;

typedef struct command_messenger_add_by_name
{
	uint8_t length;
	//char login[LOGIN_MAX_LEN+1];
} TPacketCGMessengerAddByName;

typedef struct command_messenger_remove
{
	char login[LOGIN_MAX_LEN + 1];
	//uint32_t account;
} TPacketCGMessengerRemove;

typedef struct command_safebox_checkout
{
	uint8_t	bHeader;
	uint32_t	bSafePos;
	TItemPos	ItemPos;
} TPacketCGSafeboxCheckout;

typedef struct command_safebox_checkin
{
	uint8_t	bHeader;
	uint32_t	bSafePos;
	TItemPos	ItemPos;
} TPacketCGSafeboxCheckin;


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

typedef struct command_party_parameter
{
	uint8_t	bHeader;
	uint8_t	bDistributeMode;
} TPacketCGPartyParameter;

typedef struct command_party_invite
{
	uint8_t	header;
	uint32_t	vid;
} TPacketCGPartyInvite;

typedef struct command_party_invite_answer
{
	uint8_t	header;
	uint32_t	leader_vid;
	uint8_t	accept;
} TPacketCGPartyInviteAnswer;

typedef struct command_party_remove
{
	uint8_t header;
	uint32_t pid;
} TPacketCGPartyRemove;

typedef struct command_party_set_state
{
	uint8_t header;
	uint32_t pid;
	uint8_t byRole;
	uint8_t flag;
} TPacketCGPartySetState;

typedef struct command_party_use_skill
{
	uint8_t header;
	uint8_t bySkillIndex;
	uint32_t vid;
} TPacketCGPartyUseSkill;

typedef struct packet_safebox_size
{
	uint8_t bHeader;
	uint8_t bSize;
} TPacketCGSafeboxSize;

typedef struct packet_safebox_wrong_password
{
	uint8_t	bHeader;
} TPacketCGSafeboxWrongPassword;

typedef struct command_empire
{
	uint8_t	bHeader;
	uint8_t	bEmpire;
} TPacketCGEmpire;

typedef struct command_safebox_money
{
	uint8_t        bHeader;
	uint8_t        bState;
	int32_t	lMoney;
} TPacketCGSafeboxMoney;

typedef struct command_guild
{
	uint8_t header;
	uint8_t subheader;
} TPacketCGGuild;

typedef struct command_guild_answer_make_guild
{
	uint8_t header;
	char guild_name[GUILD_NAME_MAX_LEN + 1];
} TPacketCGAnswerMakeGuild;

typedef struct command_guild_use_skill
{
	uint32_t	dwVnum;
	uint32_t	dwPID;
} TPacketCGGuildUseSkill;

// Guild Mark
typedef struct command_mark_login
{
	uint8_t    header;
	uint32_t   handle;
	uint32_t   random_key;
} TPacketCGMarkLogin;

typedef struct command_mark_upload
{
	uint8_t	header;
	uint32_t	gid;
	uint8_t	image[16 * 12 * 4];
} TPacketCGMarkUpload;

typedef struct command_mark_idxlist
{
	uint8_t	header;
} TPacketCGMarkIDXList;

typedef struct command_mark_crclist
{
	uint8_t	header;
	uint8_t	imgIdx;
	uint32_t	crclist[80];
} TPacketCGMarkCRCList;

typedef struct command_symbol_upload
{
	uint8_t	header;
	uint16_t	size;
	uint32_t	guild_id;
} TPacketCGGuildSymbolUpload;

typedef struct command_symbol_crc
{
	uint8_t header;
	uint32_t guild_id;
	uint32_t crc;
	uint32_t size;
} TPacketCGSymbolCRC;

typedef struct command_fishing
{
	uint8_t header;
	uint8_t dir;
} TPacketCGFishing;

typedef struct command_give_item
{
	uint8_t byHeader;
	uint32_t dwTargetVID;
	TItemPos ItemPos;
#ifdef ENABLE_NEW_STACK_LIMIT
	int
#else
	uint8_t
#endif
		byItemCount;
} TPacketCGGiveItem;

typedef struct SPacketCGHack
{
	uint8_t	bHeader;
	char	szBuf[255 + 1];
} TPacketCGHack;

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
	uint8_t	header;
	uint8_t	pos;
	uint8_t	type;
#ifdef ENABLE_FEATURES_REFINE_SYSTEM
	uint8_t		lLow;
	uint8_t		lMedium;
	uint8_t		lExtra;
	uint8_t		lTotal;
#endif
} TPacketCGRefine;

typedef struct SPacketCGRequestRefineInfo
{
	uint8_t	header;
	uint8_t	pos;
} TPacketCGRequestRefineInfo;

typedef struct SPacketCGChangeName
{
	uint8_t header;
	uint8_t index;
	char name[CHARACTER_NAME_MAX_LEN + 1];
} TPacketCGChangeName;

typedef struct command_client_version
{
	uint8_t header;
	char filename[32 + 1];
	char timestamp[32 + 1];
} TPacketCGClientVersion;

typedef struct command_client_version2
{
	uint8_t header;
	char filename[32 + 1];
	char timestamp[32 + 1];
} TPacketCGClientVersion2;

typedef struct command_script_select_item
{
	uint8_t header;
	uint32_t selection;
} TPacketCGScriptSelectItem;

typedef struct SPacketCGDragonSoulRefine
{
	SPacketCGDragonSoulRefine() : header(HEADER_CG_DRAGON_SOUL_REFINE), bSubType(0)
	{
	}

	uint8_t header;
	uint8_t bSubType;
	TItemPos ItemGrid[DRAGON_SOUL_REFINE_GRID_SIZE];
} TPacketCGDragonSoulRefine;

typedef struct SPacketCGStateCheck
{
	uint8_t header;
	unsigned long key;
	unsigned long index;
} TPacketCGStateCheck;

typedef struct {
	uint8_t bHeader;
	uint16_t wSize;
	uint8_t bSubHeader;
} TPacketCGNewOfflineShop;

typedef struct
{
	offlineshop::TShopInfo shop;
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
	offlineshop::TPriceInfo  price;
}TSubPacketCGAddItem;

typedef struct
{
	uint32_t dwItemID;
}TSubPacketCGRemoveItem;

typedef struct
{
	uint32_t dwItemID;
	offlineshop::TPriceInfo price;
}TSubPacketCGEditItem;

typedef struct filter {
	uint8_t		bType;
	uint8_t		bSubType;

	char		szName[ITEM_NAME_MAX_LEN];

	offlineshop::TPriceInfo	priceStart, priceEnd;
	int			iLevelStart, iLevelEnd;

	uint32_t		dwWearFlag;
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_NORM_NUM];
	int         iRarity = -1;
}TFilterInfo;

typedef struct
{
	TFilterInfo filter;
}TSubPacketCGFilterRequest;

typedef struct
{
	offlineshop::TOfferInfo offer;
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
	offlineshop::TValutesInfo valutes;
}TSubPacketCGShopSafeboxGetValutes;

typedef struct
{
	uint32_t dwOwnerID;
	uint32_t dwItemID;
	bool  bIsSearch;
	int64_t TotalPriceSeen;
}TSubPacketCGShopBuyItem;

typedef struct {
	uint32_t dwOwnerID;
} TSubPacketCGAuctionOpenRequest;

typedef struct {
	uint32_t		dwDuration;
	TItemPos	pos;

	offlineshop::TPriceInfo	init_price;
} TSubPacketCGAuctionCreate;

typedef struct {
	uint32_t		dwOwnerID;
	offlineshop::TPriceInfo	price;
}TSubPacketCGAuctionAddOffer;

typedef struct {
	uint32_t dwOwnerID;
} TSubPacketCGAuctionExitFrom;

#ifdef ENABLE_NEW_SHOP_IN_CITIES
typedef struct {
	uint32_t dwShopVID;
} TSubPacketCGShopClickEntity;
#endif

struct TPacketCGSwitchbot
{
	uint8_t header;
	int size;
	uint8_t subheader;
	uint8_t slot;
};

#ifdef ENABLE_MAP_TELEPORTER
typedef struct SPacketCGMapTeleporter
{
	uint8_t	bHeader;
	int		iMapCode;

} TPacketCGMapTeleporter;
#endif

typedef struct  packet_send_cube_renewal
{
	uint8_t header;
	uint8_t subheader;
	uint32_t index_item;
	uint32_t count_item;
	uint32_t index_item_improve;
}TPacketCGCubeRenewalSend;

#ifdef __SKILL_COLOR_SYSTEM__
typedef struct packet_skill_color
{
	uint8_t		bheader;
	uint8_t		skill;
	uint32_t		col1;
	uint32_t		col2;
	uint32_t		col3;
	uint32_t		col4;
	uint32_t		col5;
} TPacketCGSkillColor;
#endif

typedef struct SPacketCGBattlePassAction
{
	uint8_t	bHeader;
	uint8_t	bAction;
} TPacketCGBattlePassAction;







#pragma pack()