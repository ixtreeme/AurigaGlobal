#include "StdAfx.h"
#include "GameType.h"
#include "Packet.h"

std::string g_strResourcePath = "d:/ymir work/";
std::string g_strImagePath = "d:/ymir work/ui/";

std::string g_strGuildSymbolPathName = "mark/10/";

// DEFAULT_FONT
static std::string gs_strDefaultFontName = "굴림체:12.fnt";
static std::string gs_strDefaultItalicFontName = "굴림체:12i.fnt";
static CResource* gs_pkDefaultFont = nullptr;
static CResource* gs_pkDefaultItalicFont = nullptr;

static bool gs_isReloadDefaultFont = false;

void DefaultFont_Startup()
{
	gs_pkDefaultFont = nullptr;
}

void DefaultFont_Cleanup()
{
	if (gs_pkDefaultFont)
		gs_pkDefaultFont->Release();
}

void DefaultFont_SetName(const char * c_szFontName)
{
	gs_strDefaultFontName = c_szFontName;
	gs_strDefaultFontName += ".fnt";

	gs_strDefaultItalicFontName = c_szFontName;
	if(strchr(c_szFontName, ':'))
		gs_strDefaultItalicFontName += "i";
	gs_strDefaultItalicFontName += ".fnt";

	gs_isReloadDefaultFont = true;
}

bool ReloadDefaultFonts()
{
	CResourceManager& rkResMgr = CResourceManager::Instance();

	gs_isReloadDefaultFont = false;

	CResource* pkNewFont = rkResMgr.GetResourcePointer(gs_strDefaultFontName.c_str());
	pkNewFont->AddReference();
	if (gs_pkDefaultFont)
		gs_pkDefaultFont->Release();
	gs_pkDefaultFont = pkNewFont;

	CResource* pkNewItalicFont = rkResMgr.GetResourcePointer(gs_strDefaultItalicFontName.c_str());
	pkNewItalicFont->AddReference();
	if (gs_pkDefaultItalicFont)
		gs_pkDefaultItalicFont->Release();
	gs_pkDefaultItalicFont = pkNewItalicFont;

	return true;
}

CResource* DefaultFont_GetResource()
{
	if (!gs_pkDefaultFont || gs_isReloadDefaultFont)
		ReloadDefaultFonts();
	return gs_pkDefaultFont;
}

CResource* DefaultItalicFont_GetResource()
{
	if (!gs_pkDefaultItalicFont || gs_isReloadDefaultFont)
		ReloadDefaultFonts();
	return gs_pkDefaultItalicFont;
}

// END_OF_DEFAULT_FONT

void SetGuildSymbolPath(const char * c_szPathName)
{
	g_strGuildSymbolPathName = "mark/";
	g_strGuildSymbolPathName += c_szPathName;
	g_strGuildSymbolPathName += "/";
}

const char * GetGuildSymbolFileName(uint32_t dwGuildID)
{
	return _getf("%s%03d.jpg", g_strGuildSymbolPathName.c_str(), dwGuildID);
}

uint8_t c_aSlotTypeToInvenType[SLOT_TYPE_MAX] =
{
	RESERVED_WINDOW,		// SLOT_TYPE_NONE
	INVENTORY,				// SLOT_TYPE_INVENTORY
	RESERVED_WINDOW,		// SLOT_TYPE_SKILL
	RESERVED_WINDOW,		// SLOT_TYPE_EMOTION
	RESERVED_WINDOW,		// SLOT_TYPE_SHOP
	RESERVED_WINDOW,		// SLOT_TYPE_EXCHANGE_OWNER
	RESERVED_WINDOW,		// SLOT_TYPE_EXCHANGE_TARGET
	RESERVED_WINDOW,		// SLOT_TYPE_QUICK_SLOT
	RESERVED_WINDOW,		// SLOT_TYPE_SAFEBOX	<- SAFEBOX, MALL의 경우 하드 코딩되어있는 LEGACY 코드를 유지함.
	RESERVED_WINDOW,		// SLOT_TYPE_PRIVATE_SHOP
	RESERVED_WINDOW,		// SLOT_TYPE_MALL		<- SAFEBOX, MALL의 경우 하드 코딩되어있는 LEGACY 코드를 유지함.
	DRAGON_SOUL_INVENTORY,	// SLOT_TYPE_DRAGON_SOUL_INVENTORY
	MOUNT_INVENTORY,
#ifdef ENABLE_EXTRA_INVENTORY
	EXTRA_INVENTORY,		// SLOT_TYPE_EXTRA_INVENTORY <- SPECIAL_SLOT
#endif
#ifdef ENABLE_SWITCHBOT
	SWITCHBOT,
#endif
};

uint8_t SlotTypeToInvenType(uint8_t bSlotType)
{
	if (bSlotType >= SLOT_TYPE_MAX)
		return RESERVED_WINDOW;
	else
		return c_aSlotTypeToInvenType[bSlotType];
}
typedef struct SApplyInfo
{
	uint8_t	bPointType;
} TApplyInfo;

const TApplyInfo aApplyInfo[CItemData::MAX_APPLY_NUM] =
{
	{ POINT_NONE,						},
	{ POINT_MAX_HP,		        		},
	{ POINT_MAX_SP,		        		},
	{ POINT_HT,			        		},
	{ POINT_IQ,			        		},
	{ POINT_ST,			        		},
	{ POINT_DX,			        		},
	{ POINT_ATT_SPEED,		    		},
	{ POINT_MOV_SPEED,		    		},
	{ POINT_CASTING_SPEED,	    		},
	{ POINT_HP_REGEN,					},
	{ POINT_SP_REGEN,					},
	{ POINT_POISON_PCT,		    		},
	{ POINT_STUN_PCT,		    		},
	{ POINT_SLOW_PCT,		    		},
	{ POINT_CRITICAL_PCT,				},
	{ POINT_PENETRATE_PCT,				},
	{ POINT_ATTBONUS_HUMAN,				},
	{ POINT_ATTBONUS_ANIMAL,			},
	{ POINT_ATTBONUS_ORC,				},
	{ POINT_ATTBONUS_MILGYO,			},
	{ POINT_ATTBONUS_UNDEAD,			},
	{ POINT_ATTBONUS_DEVIL,				},
	{ POINT_STEAL_HP,					},
	{ POINT_STEAL_SP,					},
	{ POINT_MANA_BURN_PCT,				},
	{ POINT_DAMAGE_SP_RECOVER,			},
	{ POINT_BLOCK,		        		},
	{ POINT_DODGE,		        		},
	{ POINT_RESIST_SWORD,				},
	{ POINT_RESIST_TWOHAND,				},
	{ POINT_RESIST_DAGGER,				},
	{ POINT_RESIST_BELL,				},
	{ POINT_RESIST_FAN,					},
	{ POINT_RESIST_BOW,					},
	{ POINT_RESIST_FIRE,				},
	{ POINT_RESIST_ELEC,				},
	{ POINT_RESIST_MAGIC,				},
	{ POINT_RESIST_WIND,				},
	{ POINT_REFLECT_MELEE,				},
	{ POINT_REFLECT_CURSE,				},
	{ POINT_POISON_REDUCE,				},
	{ POINT_KILL_SP_RECOVER,			},
	{ POINT_EXP_DOUBLE_BONUS,			},
	{ POINT_GOLD_DOUBLE_BONUS,			},
	{ POINT_ITEM_DROP_BONUS,			},
	{ POINT_POTION_BONUS,				},
	{ POINT_KILL_HP_RECOVER,			},
	{ POINT_IMMUNE_STUN,				},
	{ POINT_IMMUNE_SLOW,				},
	{ POINT_IMMUNE_FALL,				},
	{ POINT_NONE,						},
	{ POINT_BOW_DISTANCE,				},
	{ POINT_ATT_GRADE_BONUS,			},
	{ POINT_DEF_GRADE_BONUS,			},
	{ POINT_MAGIC_ATT_GRADE_BONUS,		},
	{ POINT_MAGIC_DEF_GRADE_BONUS,		},
	{ POINT_CURSE_PCT,					},
	{ POINT_MAX_STAMINA					},
	{ POINT_ATTBONUS_WARRIOR			},
	{ POINT_ATTBONUS_ASSASSIN			},
	{ POINT_ATTBONUS_SURA				},
	{ POINT_ATTBONUS_SHAMAN				},
	{ POINT_ATTBONUS_MONSTER			},
	{ POINT_ATT_BONUS					},
	{ POINT_MALL_DEFBONUS				},
	{ POINT_MALL_EXPBONUS				},
	{ POINT_MALL_ITEMBONUS				},
	{ POINT_MALL_GOLDBONUS				},
	{ POINT_MAX_HP_PCT					},
	{ POINT_MAX_SP_PCT					},
	{ POINT_SKILL_DAMAGE_BONUS			},
	{ POINT_NORMAL_HIT_DAMAGE_BONUS		},
	{ POINT_SKILL_DEFEND_BONUS			},
	{ POINT_NORMAL_HIT_DEFEND_BONUS		},
	{ POINT_PC_BANG_EXP_BONUS			},
	{ POINT_PC_BANG_DROP_BONUS			},
	{ POINT_NONE,						},
	{ POINT_RESIST_WARRIOR,				},
	{ POINT_RESIST_ASSASSIN,			},
	{ POINT_RESIST_SURA,				},
	{ POINT_RESIST_SHAMAN,				},
	{ POINT_ENERGY						},
	{ POINT_DEF_GRADE					},
	{ POINT_COSTUME_ATTR_BONUS			},
	{ POINT_MAGIC_ATT_BONUS_PER 		},
	{ POINT_MELEE_MAGIC_ATT_BONUS_PER	},
	{ POINT_RESIST_ICE,					},
	{ POINT_RESIST_EARTH,				},
	{ POINT_RESIST_DARK,				},
	{ POINT_RESIST_CRITICAL,			},
	{ POINT_RESIST_PENETRATE,			},
	{ POINT_ATTBONUS_METIN,			},
};

uint8_t ApplyTypeToPointType(uint8_t bApplyType)
{
	if (bApplyType >= CItemData::MAX_APPLY_NUM)
		return POINT_NONE;
	else
		return aApplyInfo[bApplyType].bPointType;
}