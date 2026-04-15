#include "stdafx.h"

#include <stack>

#include "utils.h"
#include "config.h"
#include "char.h"
#include "char_manager.h"
#include "item_manager.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "packet.h"
#include "affect.h"
#include "skill.h"
#include "start_position.h"
#include "mob_manager.h"
#include "db.h"
#include "log.h"
#include "vector.h"
#include "buffer_manager.h"
#include "questmanager.h"
#include "fishing.h"
#include "party.h"
#include "dungeon.h"
#include "refine.h"
#include "unique_item.h"
#include "war_map.h"
#include "marriage.h"
#include "polymorph.h"
#include "blend_item.h"
#include "BattleArena.h"
#include "arena.h"
#include "dev_log.h"
#include "pcbang.h"
#include "MountSystem.h"//@Razor93

#include "safebox.h"
#include "shop.h"

#ifdef __NEWPET_SYSTEM__
#include "New_PetSystem.h"
#define __NEWPET_SYSTEM_CHECK
#endif
#ifdef __PET_SYSTEM__
#include "PetSystem.h"
#endif
#ifdef ENABLE_NEWSTUFF
#include "pvp.h"
#endif
#ifdef ENABLE_SWITCHBOT
#include "new_switchbot.h"
#endif

#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

#include <common/VnumHelper.h>
#include "DragonSoul.h"
#include "buff_on_attributes.h"
#include "belt_inventory_helper.h"
#include <common/CommonDefines.h>
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
//#include "LostCastleDungeon.h"
#include "RuneDungeon.h"
#include "ItemUse.h"
#include "Halloween2022Dungeon.h"
#include "VikingDungeon.h"
#endif

//#include "../common/service.h"
//auction_temp
#ifdef ENABLE_STOLE_COSTUME
#include <common/stole_length.h>
#endif

const int ITEM_BROKEN_METIN_VNUM = 28960;
extern int stone_chance; // Chance Stone Chance
#define ENABLE_EFFECT_EXTRAPOT
#define ENABLE_BOOKS_STACKFIX
#define ENABLE_STONE_STACKFIX //USE ONLY 1 STONE TO ADD
//#define __USE_ADD_WITH_ALL_ITEMS__ //CAN ADD OR SWITH GREEN BONUS WITH ALL ITEMS (MAX LVL 40)

// CHANGE_ITEM_ATTRIBUTES
