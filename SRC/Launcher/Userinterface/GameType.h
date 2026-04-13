#pragma once
#include "../Game/ItemData.h"

struct SAffects
{
	enum
	{
		AFFECT_MAX_NUM = 32,
	};

	SAffects() : dwAffects(0) {}
	SAffects(const uint32_t & c_rAffects)
	{
		__SetAffects(c_rAffects);
	}
	int operator = (const uint32_t & c_rAffects)
	{
		__SetAffects(c_rAffects);
	}

	bool IsAffect(uint8_t byIndex)
	{
		return dwAffects & (1 << byIndex);
	}

	void __SetAffects(const uint32_t & c_rAffects)
	{
		dwAffects = c_rAffects;
	}

	uint32_t dwAffects;
};

#ifdef WJ_ENABLE_TRADABLE_ICON
enum ETopWindowTypes
{
	ON_TOP_WND_NONE,
	ON_TOP_WND_SHOP,
	ON_TOP_WND_EXCHANGE,
	ON_TOP_WND_SAFEBOX,
	ON_TOP_WND_PRIVATE_SHOP,
	ON_TOP_WND_ITEM_COMB,
	ON_TOP_WND_PET_FEED,

	ON_TOP_WND_MAX,
};
#endif

extern std::string g_strGuildSymbolPathName;

constexpr uint32_t c_Name_Max_Length = 64;
constexpr uint32_t c_FileName_Max_Length = 128;
constexpr uint32_t c_Short_Name_Max_Length = 32;

constexpr uint32_t c_Inventory_Page_Column = 5;
constexpr uint32_t c_Inventory_Page_Row = 9;
constexpr uint32_t c_Inventory_Page_Size = c_Inventory_Page_Column*c_Inventory_Page_Row; // x*y
#ifdef ENABLE_EXTEND_INVEN_SYSTEM
constexpr uint32_t c_Inventory_Page_Count = 4;
#else
const uint32_t c_Inventory_Page_Count = 2;
#endif
constexpr uint32_t c_ItemSlot_Count = c_Inventory_Page_Size * c_Inventory_Page_Count;
constexpr uint32_t c_Equipment_Count = 12;

constexpr uint32_t c_Equipment_Start = c_ItemSlot_Count;

constexpr uint32_t c_Equipment_Body	= c_Equipment_Start + 0;
constexpr uint32_t c_Equipment_Head	= c_Equipment_Start + 1;
constexpr uint32_t c_Equipment_Shoes	= c_Equipment_Start + 2;
constexpr uint32_t c_Equipment_Wrist	= c_Equipment_Start + 3;
constexpr uint32_t c_Equipment_Weapon	= c_Equipment_Start + 4;
constexpr uint32_t c_Equipment_Neck	= c_Equipment_Start + 5;
constexpr uint32_t c_Equipment_Ear		= c_Equipment_Start + 6;
constexpr uint32_t c_Equipment_Unique1	= c_Equipment_Start + 7;
constexpr uint32_t c_Equipment_Unique2	= c_Equipment_Start + 8;
constexpr uint32_t c_Equipment_Arrow	= c_Equipment_Start + 9;
constexpr uint32_t c_Equipment_Shield	= c_Equipment_Start + 10;
#ifdef ENABLE_STOLE_REAL
constexpr uint32_t c_Equipment_Stole = c_Equipment_Start + 18;
#endif

#ifdef ENABLE_NEW_EQUIPMENT_SYSTEM
constexpr uint32_t c_New_Equipment_Start = c_Equipment_Start + 21;
#ifdef ENABLE_PENDANT
constexpr uint32_t c_New_Equipment_Count = 5;
#else
	const uint32_t c_New_Equipment_Count = 3;
#endif
constexpr uint32_t c_Equipment_Ring1 = c_New_Equipment_Start + 0;
constexpr uint32_t c_Equipment_Ring2 = c_New_Equipment_Start + 1;
constexpr uint32_t c_Equipment_Belt  = c_New_Equipment_Start + 2;
#ifdef ENABLE_PENDANT
constexpr uint32_t c_Equipment_Pendant = c_New_Equipment_Start + 4;
#endif
#endif

#ifdef ENABLE_SKILL_COLOR_SYSTEM
enum ESkillColorLength
{
	MAX_SKILL_COUNT = 6,
	MAX_EFFECT_COUNT = 5,
	BUFF_BEGIN = MAX_SKILL_COUNT,
	MAX_BUFF_COUNT = 5,
};
#endif

enum EDragonSoulDeckType
{
	DS_DECK_1,
	DS_DECK_2,
	DS_DECK_MAX_NUM = 2,
};


#ifdef ENABLE_EXTRA_INVENTORY
constexpr uint32_t c_Extra_Inventory_Page_Column = 5;
constexpr uint32_t c_Extra_Inventory_Page_Row = 9;
constexpr uint32_t c_Extra_Inventory_Page_Size = c_Extra_Inventory_Page_Column * c_Extra_Inventory_Page_Row;
constexpr uint32_t c_Extra_Inventory_Page_Count = 24;
constexpr uint32_t c_Extra_Inventory_Count = c_Extra_Inventory_Page_Size * c_Extra_Inventory_Page_Count;
constexpr uint32_t c_Extra_Inventory_Category_Count = 6;
//To fix search item in refine (0)
constexpr uint32_t c_Extra_Inventory_Refine = c_Extra_Inventory_Count * c_Extra_Inventory_Category_Count;
#endif

constexpr uint32_t c_Mount_Inventory_Width = 12;
constexpr uint32_t c_Mount_Inventory_Height = 16;
constexpr uint32_t c_Mount_Inventory_Count = c_Mount_Inventory_Width * c_Mount_Inventory_Height;

enum EDragonSoulGradeTypes
{
	DRAGON_SOUL_GRADE_NORMAL,
	DRAGON_SOUL_GRADE_BRILLIANT,
	DRAGON_SOUL_GRADE_RARE,
	DRAGON_SOUL_GRADE_ANCIENT,
	DRAGON_SOUL_GRADE_LEGENDARY,
#ifdef ENABLE_DS_GRADE_MYTH
	DRAGON_SOUL_GRADE_MYTH,
#endif
	DRAGON_SOUL_GRADE_MAX,
};

enum EDragonSoulStepTypes
{
	DRAGON_SOUL_STEP_LOWEST,
	DRAGON_SOUL_STEP_LOW,
	DRAGON_SOUL_STEP_MID,
	DRAGON_SOUL_STEP_HIGH,
	DRAGON_SOUL_STEP_HIGHEST,
	DRAGON_SOUL_STEP_MAX,
};

#ifdef ENABLE_COSTUME_SYSTEM
constexpr uint32_t c_Costume_Slot_Start	= c_Equipment_Start + 19;	
constexpr uint32_t	c_Costume_Slot_Body		= c_Costume_Slot_Start + 0;
constexpr uint32_t	c_Costume_Slot_Hair		= c_Costume_Slot_Start + 1;
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
constexpr uint32_t	c_Costume_Slot_Mount	= c_Costume_Slot_Start + 2;
#endif
#ifdef ENABLE_ACCE_SYSTEM
constexpr uint32_t	c_Costume_Slot_Acce		= c_Costume_Slot_Start + 3;
#endif

#if defined(ENABLE_WEAPON_COSTUME_SYSTEM) || defined(ENABLE_ACCE_SYSTEM)
constexpr uint32_t c_Costume_Slot_Count	= 4;
#elif defined(ENABLE_MOUNT_COSTUME_SYSTEM)
	const uint32_t c_Costume_Slot_Count	= 3;
#else
	const uint32_t c_Costume_Slot_Count	= 2;
#endif

constexpr uint32_t c_Costume_Slot_End		= c_Costume_Slot_Start + c_Costume_Slot_Count;

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
constexpr uint32_t	c_Costume_Slot_Weapon	= c_Costume_Slot_End + 1;
#endif
#ifdef ENABLE_COSTUME_PET
constexpr uint32_t c_Costume_PetSkin = c_Equipment_Start + 26;
#endif
#ifdef ENABLE_COSTUME_MOUNT
constexpr uint32_t c_Costume_MountSkin = c_Equipment_Start + 27;
#endif
#ifdef ENABLE_COSTUME_EFFECT
constexpr uint32_t c_Costume_EffectBody = c_Equipment_Start + 28;
constexpr uint32_t c_Costume_EffectWeapon = c_Equipment_Start + 29;
#endif
#endif

#ifdef ENABLE_RUNE_SYSTEM
constexpr uint32_t c_Wear_Rune_Start = c_Equipment_Start + 32;
constexpr uint32_t c_Wear_Rune_Count = 7;
constexpr uint32_t c_Wear_Max = 39;
#else
const uint32_t c_Wear_Max = 32;
#endif
constexpr uint32_t c_DragonSoul_Equip_Start = c_ItemSlot_Count + c_Wear_Max;
constexpr uint32_t c_DragonSoul_Equip_Slot_Max = 6;
constexpr uint32_t c_DragonSoul_Equip_End = c_DragonSoul_Equip_Start + c_DragonSoul_Equip_Slot_Max * DS_DECK_MAX_NUM;

constexpr uint32_t c_DragonSoul_Equip_Reserved_Count = c_DragonSoul_Equip_Slot_Max * 3;

#ifdef ENABLE_NEW_EQUIPMENT_SYSTEM
constexpr uint32_t c_Belt_Inventory_Slot_Start = 500;
constexpr uint32_t c_Belt_Inventory_Width = 10;
constexpr uint32_t c_Belt_Inventory_Height= 16;
constexpr uint32_t c_Belt_Inventory_Slot_Count = c_Belt_Inventory_Width * c_Belt_Inventory_Height;
constexpr uint32_t c_Belt_Inventory_Slot_End = c_Belt_Inventory_Slot_Start + c_Belt_Inventory_Slot_Count;

constexpr uint32_t c_Inventory_Count	= c_Belt_Inventory_Slot_End;
#else
	const uint32_t c_Inventory_Count	= c_DragonSoul_Equip_End;
#endif

constexpr uint32_t c_DragonSoul_Inventory_Start = 0;
constexpr uint32_t c_DragonSoul_Inventory_Box_Size = 32;
constexpr uint32_t c_DragonSoul_Inventory_Count = 300 + (CItemData::DS_SLOT_NUM_TYPES * DRAGON_SOUL_GRADE_MAX * c_DragonSoul_Inventory_Box_Size);
constexpr uint32_t c_DragonSoul_Inventory_End = c_DragonSoul_Inventory_Start + c_DragonSoul_Inventory_Count;

enum ESlotType
{
	SLOT_TYPE_NONE,
	SLOT_TYPE_INVENTORY,
	SLOT_TYPE_SKILL,
	SLOT_TYPE_EMOTION,
	SLOT_TYPE_SHOP,
	SLOT_TYPE_EXCHANGE_OWNER,
	SLOT_TYPE_EXCHANGE_TARGET,
	SLOT_TYPE_QUICK_SLOT,
	SLOT_TYPE_SAFEBOX,
	SLOT_TYPE_PRIVATE_SHOP,
	SLOT_TYPE_MALL,
	SLOT_TYPE_DRAGON_SOUL_INVENTORY,
	SLOT_TYPE_MOUNT_INVENTORY,
#ifdef ENABLE_EXTRA_INVENTORY
	SLOT_TYPE_EXTRA_INVENTORY,
#endif
#ifdef ENABLE_SWITCHBOT
	SLOT_TYPE_SWITCHBOT,
#endif
	SLOT_TYPE_MAX,
};

enum EWindows
{
	RESERVED_WINDOW,
	INVENTORY,				// 기본 인벤토리. (45칸 짜리가 2페이지 존재 = 90칸)
	EQUIPMENT,
	SAFEBOX,
	MALL,
	MOUNT_INVENTORY,
	DRAGON_SOUL_INVENTORY,
	BELT_INVENTORY,			// NOTE: W2.1 버전에 새로 추가되는 벨트 슬롯 아이템이 제공하는 벨트 인벤토리
#ifdef ENABLE_EXTRA_INVENTORY
	EXTRA_INVENTORY,
#endif
#ifdef ENABLE_SWITCHBOT
	SWITCHBOT,
#endif
	GROUND,					// NOTE: 2013년 2월5일 현재까지 unused.. 왜 있는거지???
	WINDOW_TYPE_MAX,
};

enum EDSInventoryMaxNum
{
	DS_INVENTORY_MAX_NUM = c_DragonSoul_Inventory_Count,
	DS_REFINE_WINDOW_MAX_NUM = 15,
};
#ifdef ENABLE_SWITCHBOT
enum ESwitchbotValues
{
	SWITCHBOT_SLOT_COUNT = 5,
	SWITCHBOT_ALTERNATIVE_COUNT = 2,
	MAX_NORM_ATTR_NUM = 5,
};

enum EAttributeSet
{
	ATTRIBUTE_SET_WEAPON,
	ATTRIBUTE_SET_BODY,
	ATTRIBUTE_SET_WRIST,
	ATTRIBUTE_SET_FOOTS,
	ATTRIBUTE_SET_NECK,
	ATTRIBUTE_SET_HEAD,
	ATTRIBUTE_SET_SHIELD,
	ATTRIBUTE_SET_EAR,
	ATTRIBUTE_SET_MAX_NUM,
};

#endif
#pragma pack (push, 1)
#define WORD_MAX 0xffff

typedef struct SItemPos
{
	uint8_t window_type;
	uint16_t cell;
    SItemPos ()
    {
		window_type =     INVENTORY;
		cell = WORD_MAX;
    }
	SItemPos (uint8_t _window_type, uint16_t _cell)
    {
        window_type = _window_type;
        cell = _cell;
    }

	// 기존에 cell의 형을 보면 BYTE가 대부분이지만, oi
	// 어떤 부분은 int, 어떤 부분은 WORD로 되어있어,
	// 가장 큰 자료형인 int로 받는다.
  //  int operator=(const int _cell)
  //  {
		//window_type = INVENTORY;
  //      cell = _cell;
  //      return cell;
  //  }
	bool IsValidCell()
	{
		switch (window_type)
		{
		case INVENTORY:
			return cell < c_Inventory_Count;
			break;
		case EQUIPMENT:
			return cell < c_DragonSoul_Equip_End;
			break;
		case DRAGON_SOUL_INVENTORY:
			return cell < (DS_INVENTORY_MAX_NUM);
			break;
		case MOUNT_INVENTORY:
			return cell < c_Mount_Inventory_Count;
			break;
#ifdef ENABLE_EXTRA_INVENTORY
		case EXTRA_INVENTORY:
			return cell < c_Extra_Inventory_Count;
#endif
#ifdef ENABLE_SWITCHBOT
		case SWITCHBOT:
			return cell < SWITCHBOT_SLOT_COUNT;
			break;
#endif
		default:
			return false;
		}
	}
	bool IsEquipCell()
	{
		switch (window_type)
		{
		case INVENTORY:
		case EQUIPMENT:
			return (c_Equipment_Start + c_Wear_Max > cell) && (c_Equipment_Start <= cell);
			break;

		case BELT_INVENTORY:
		case DRAGON_SOUL_INVENTORY:
			return false;
			break;

		default:
			return false;
		}
	}

#ifdef ENABLE_NEW_EQUIPMENT_SYSTEM
	bool IsBeltInventoryCell()
	{
		bool bResult = c_Belt_Inventory_Slot_Start <= cell && c_Belt_Inventory_Slot_End > cell;
		return bResult;
	}
#endif

	bool operator==(const struct SItemPos& rhs) const
	{
		return (window_type == rhs.window_type) && (cell == rhs.cell);
	}

	bool operator<(const struct SItemPos& rhs) const
	{
		return (window_type < rhs.window_type) || ((window_type == rhs.window_type) && (cell < rhs.cell));
	}
} TItemPos;
#pragma pack(pop)

constexpr uint32_t c_QuickBar_Line_Count = 3;
constexpr uint32_t c_QuickBar_Slot_Count = 12;

constexpr float c_Idle_WaitTime = 5.0f;

constexpr int c_Monster_Race_Start_Number = 6;
constexpr int c_Monster_Model_Start_Number = 20001;

constexpr float c_fAttack_Delay_Time = 0.2f;
constexpr float c_fHit_Delay_Time = 0.1f;
constexpr float c_fCrash_Wave_Time = 0.2f;
constexpr float c_fCrash_Wave_Distance = 3.0f;

constexpr float c_fHeight_Step_Distance = 50.0f;

enum
{
	DISTANCE_TYPE_FOUR_WAY,
	DISTANCE_TYPE_EIGHT_WAY,
	DISTANCE_TYPE_ONE_WAY,
	DISTANCE_TYPE_MAX_NUM,
};

constexpr float c_fMagic_Script_Version = 1.0f;
constexpr float c_fSkill_Script_Version = 1.0f;
constexpr float c_fMagicSoundInformation_Version = 1.0f;
constexpr float c_fBattleCommand_Script_Version = 1.0f;
constexpr float c_fEmotionCommand_Script_Version = 1.0f;
constexpr float c_fActive_Script_Version = 1.0f;
constexpr float c_fPassive_Script_Version = 1.0f;

// Used by PushMove
constexpr float c_fWalkDistance = 175.0f;
constexpr float c_fRunDistance = 310.0f;

#define FILE_MAX_LEN 128

enum
{
	ITEM_SOCKET_SLOT_MAX_NUM = 3,
	// refactored attribute slot begin
	ITEM_ATTRIBUTE_SLOT_NORM_NUM	= 5,
	ITEM_ATTRIBUTE_SLOT_RARE_NUM	= 2,

	ITEM_ATTRIBUTE_SLOT_NORM_START	= 0,
	ITEM_ATTRIBUTE_SLOT_NORM_END	= ITEM_ATTRIBUTE_SLOT_NORM_START + ITEM_ATTRIBUTE_SLOT_NORM_NUM,

	ITEM_ATTRIBUTE_SLOT_RARE_START	= ITEM_ATTRIBUTE_SLOT_NORM_END,
	ITEM_ATTRIBUTE_SLOT_RARE_END	= ITEM_ATTRIBUTE_SLOT_RARE_START + ITEM_ATTRIBUTE_SLOT_RARE_NUM,

	ITEM_ATTRIBUTE_SLOT_MAX_NUM		= ITEM_ATTRIBUTE_SLOT_RARE_END, // 7
	// refactored attribute slot end
};

#pragma pack(push)
#pragma pack(1)

typedef struct SQuickSlot
{
	uint8_t Type;
	uint16_t Position;
} TQuickSlot;

typedef struct TPlayerItemAttribute
{
    uint8_t        bType;
    short       sValue;
} TPlayerItemAttribute;

typedef struct packet_item
{
	uint32_t		vnum;
	int			count;

	uint32_t		flags;
	uint32_t		anti_flags;
	int32_t		alSockets[ITEM_SOCKET_SLOT_MAX_NUM];
    TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
} TItemData;

#ifdef ENABLE_BUY_WITH_ITEM
typedef struct SShopItemPriceData
{
	uint32_t		vnum;
	uint32_t		count;
} TShopItemPriceData;
#endif

typedef struct packet_shop_item
{
	uint32_t		vnum;

	int64_t	price;
	int			count;

#ifdef ENABLE_BUY_WITH_ITEM
	TShopItemPriceData	itemprice[MAX_SHOP_PRICES];
#endif
	uint8_t		display_pos;
	int32_t		alSockets[ITEM_SOCKET_SLOT_MAX_NUM];
	TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
#ifdef ATTR_LOCK
	short	lockedattr;
#endif
} TShopItemData;

#ifdef ENABLE_BATTLE_PASS
typedef struct SBattlePassRewardItem
{
	uint32_t	dwVnum;
	int	bCount;
} TBattlePassRewardItem;

typedef struct SBattlePassMissionInfo
{
	uint8_t	bMissionType;
	uint32_t	dwMissionInfo[3];
	TBattlePassRewardItem aRewardList[3];
} TBattlePassMissionInfo;

typedef struct SBattlePassRanking
{
	uint8_t	bPos;
	char	playerName[24 + 1];
	uint32_t	dwFinishTime;
} TBattlePassRanking;
#endif

#pragma pack(pop)

inline float GetSqrtDistance(int ix1, int iy1, int ix2, int iy2) // By sqrt
{
	float dx, dy;

	dx = float(ix1 - ix2);
	dy = float(iy1 - iy2);

	return sqrtf(dx*dx + dy*dy);
}

// DEFAULT_FONT
void DefaultFont_Startup();
void DefaultFont_Cleanup();
void DefaultFont_SetName(const char * c_szFontName);
CResource* DefaultFont_GetResource();
CResource* DefaultItalicFont_GetResource();
// END_OF_DEFAULT_FONT

void SetGuildSymbolPath(const char * c_szPathName);
const char * GetGuildSymbolFileName(uint32_t dwGuildID);
uint8_t SlotTypeToInvenType(uint8_t bSlotType);
uint8_t ApplyTypeToPointType(uint8_t bApplyType);