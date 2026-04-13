#ifndef __INC_SERVICE_H__
#define __INC_SERVICE_H__

//#define M2_USE_POOL
//#define DEBUG_ALLOC


#define ENABLE_NEW_CHANGES
#define _IMPROVED_PACKET_ENCRYPTION_

#define __ATTR_TRANSFER_SYSTEM__
#define __PET_SYSTEM__
#define __UDP_BLOCK__
#define ENABLE_NEW_SECONDARY_SKILLS
#define ENABLE_NEW_PASSIVE_SKILLS
#define ENABLE_ATTR_COSTUMES
#define __ENABLE_BERAN_ADDONS_
#define GROUP_BUFF
#define __ENABLE_SPIDER_ADDONS_
#define ADVANCED_GUILD_INFO
#define ENABLE_ANNOUNCEMENT_LEVELUP
#define ENABLE_MAX_ADD_ATTRIBUTE
#define ENABLE_BUY_WITH_ITEM
#define ENABLE_SOUL_SYSTEM
#define ATTR_LOCK
#define __SKILL_COLOR_SYSTEM__ // Skill color system
#define ENABLE_BATTLE_PASS
#ifdef ENABLE_BATTLE_PASS
#define ENABLE_BATTLE_PASS_STAY_ONLINE
#define ENABLE_BATTLE_PASS_CHAT_CNT
#define ENABLE_BATTLE_PASS_SECURITY_KILL
#endif
#define __NEW_EXCHANGE_WINDOW__
#define ENABLE_PVP_ADVANCED
#define EQUIP_ENABLE_VIEW_SASH
#define __NEWPET_SYSTEM__
#ifdef ENABLE_PVP_ADVANCED
	#define BLOCK_CHANGEITEM	"pvp.BlockChangeItem"
	#define BLOCK_BUFF			"pvp.BlockBuff"
	#define BLOCK_POTION		"pvp.BlockPotion"
	#define BLOCK_RIDE			"pvp.BlockRide"
	#define BLOCK_PET			"pvp.BlockPet"
	#define BLOCK_POLY			"pvp.BlockPoly"
	#define BLOCK_PARTY			"pvp.BlockParty"
	#define BLOCK_EXCHANGE_		"pvp.BlockExchange"
	#define BLOCK_EQUIPMENT_	"pvp.BLOCK_VIEW_EQUIPMENT"
	#define BET_WINNER			"pvp.BetMoney"
	#define CHECK_IS_FIGHT		"pvp.IsFight"
#endif
#define ENABLE_RANKING
#define ENABLE_LOCKED_EXTRA_INVENTORY
#define ENABLE_DS_SET
#define ENABLE_DS_EDITS
#define ENABLE_DS_ENCHANT
#define ENABLE_MOUNT_COSTUME_SYSTEM
#define __HIGHLIGHT_SYSTEM__
#define ENABLE_NEW_PET_EDITS
#define ENABLE_REMOTE_ATTR_SASH_REMOVE
#define ENABLE_ATLAS_BOSS
#define ENABLE_STOLE_REAL
#define ENABLE_STOLE_COSTUME
#define ENABLE_COSTUME_PET
#define ENABLE_COSTUME_MOUNT
#define ENABLE_COSTUME_EFFECT
#define ENABLE_FIX_LEVELUP_EFFECT
#define KASMIR_PAKET_SYSTEM
#define ENABLE_WHISPER_ADMIN_SYSTEM
#define ENABLE_BUG_FIXES
#define ENABLE_DS_RUNE
#define ENABLE_BLOCK_MULTIFARM
#define ENABLE_ANCIENT_PYRAMID
#ifdef ENABLE_ANCIENT_PYRAMID
	#define PYRAMID_BOSSVNUM 4158
#endif
#define BL_OFFLINE_MESSAGE
#define ENABLE_DUNGEON_MANAGER
#define ENABLE_RUNE_SYSTEM
#ifdef ENABLE_RUNE_SYSTEM
	#define RUNE_SHOP 54
	#define RUNE_EFFECT_FROM 60
#endif
#define ENABLE_NEW_USE_POTION
#define ENABLE_ENCHANT_CHANGES
#define ENABLE_STATUS_MAX_344_POINTS
#define __EFFETTO_MANTELLO__
#define ENABLE_RECALL
#define ENABLE_SKILLS_BUFF_ALTERNATIVE
#define ENABLE_NEW_STACK_LIMIT
#define ENABLE_NEW_COMMON_BONUSES
#define ENABLE_CHANGE_ATTRIBUTE_RULES
#define ENABLE_NEW_CHAT
#define ENABLE_DS_GRADE_MYTH
#define TEXTS_IMPROVEMENT
#define BLOCK_RIDING_INSIDE_WAR
#define ENABLE_INFINITE_RAFINES
#define ENABLE_BIOLOGIST_UI
#define ENABLE_DS_POTION_DIFFRENT
#define ENABLE_NEW_FISHING_SYSTEM
#if defined(ENABLE_NEW_FISHING_SYSTEM) && !defined(FISHING_NEED_CATCH)
#define FISHING_NEED_CATCH 3
#endif
#define ENABLE_NEW_UNIQUE_WEAR_LIMITED
#define ENABLE_EXTRA_INVENTORY
#define ENABLE_NO_MALUS_JEONGWIHON
#define __INGAME_WIKI__
#define WJ_ENABLE_TRADABLE_ICON
#define ENABLE_NEW_GYEONGGONG_SKILL
#define ENABLE_REWARD_AT_START
//#define ENABLE_NO_ATTBONUS_MONSTER_FOR_STONES
#define ENABLE_25082021
#ifdef ENABLE_25082021
//#define ENABLE_LIMIT_BUY_SPEED
//#define ENABLE_EFFECT_PENETRATE
//#define ENABLE_CHAT_LOGGING
//#define ENABLE_CHAT_SPAMLIMIT
//#define ENABLE_WHISPER_CHAT_SPAMLIMIT
#define ENABLE_EXTEND_ITEM_AWARD
#endif
#define ENABLE_MULTI_NAMES

#define ENABLE_01092021
#ifdef ENABLE_01092021
#define ENABLE_SPECIAL_INV_TO_SAFEBOX
#endif
#define ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
#define ENABLE_CHOOSE_DOCTRINE_GUI
//#define ENABLE_GENERAL_CH
#define ENABLE_ITEMSHOP_ITEM
//#define __NEW_EVENT_HANDLER__
#define ENABLE_RESTRICT_GM_PERMISSIONS
#define STATIC_NUMBER_GUILD
#define ENABLE_SPAM_CHECK
#define ENABLE_DS_REFINE_ALL
//#define ENABLE_ANTICHEAT
#define ENABLE_BUY_STACK_FROM_SHOP
#ifdef ENABLE_BUY_STACK_FROM_SHOP
#define MULTIPLE_BUY_LIMIT 100
#else
#define MULTIPLE_BUY_LIMIT 0
#endif
#define ENABLE_OPENSHOP_PACKET
#define ENABLE_HWID
#ifdef ENABLE_HWID
#define EANBLE_HWID_BAN
#endif
//#define ENABLE_VOTE_FOR_BONUS
#define ENABLE_VOTE4BUFF									// Ixtreeme 20250216
#define ENABLE_MELEY_LAIR
#define ENABLE_MESSENGER_TEAM
#define ENABLE_MESSENGER_HELPER

//#define __IMPROVED_HANDSHAKE_PROCESS__
//#define ENABLE_DUNGEON_BUGFIXES
/* finire
#define ENABLE_ORDER_BY_LASTPLAY
*/
#define ENABLE_USEITEM_COOLDOWN
// #define ENABLE_CHECK_BATTLE

#define NEW_READ_COMMON_DROP_ITEM

#define ENABLE_EVENT_MANAGER		// Ixtreeme
#define __AUTO_QUQUE_ATTACK__		// Ixtreeme
#define ENABLE_ITEMSHOP				// Ixtreeme




//Razor93 defines//
//#define ENABLE_INGAME_DEBUG_RAZOR93 //irja chatre realtime mikor milyen fügvényt hiv meg a szerver
#define ENABLE_TARGET_DAMAGE_RAZOR93//Razor93
#define ENABLE_MONKEY_DUNGI_BY_RAZOR93 ////Razor93 majom run mob SetHP logika
#define ENABLE_UPGRADE_NOTICE_BY_RAZOR93 //Razor93 +7 fölött sikeres fejlesztes ingame akkor hyperlinket kuld chatre
#define ENABLE_RARE_DROP_NOTICE_RAZOR93 // Cahr_battle.cpp ben listázott itemeket ha dropol ingame akkor hyperlinket kuld chatre
#define ENABLE_APPLY_NORMAL_HIT_DAMAGE_BONUS_50_NOTICE_RAZOR93 //50 fölött ak t forgat a player ingame akkor hyperlinket kuld chatre
#ifdef ENABLE_APPLY_NORMAL_HIT_DAMAGE_BONUS_50_NOTICE_RAZOR93
#define BONUSZ 50 // ertek fölött küldi a BroadcastNotice-t
#define BONUSZ_TIME 30 //spam védelem

#endif
#define ENABLE_GUILD_ATTRIBUTE					// Céh bónusz rendszer
//#define ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
#define ENABLE_FAKE_SHOP_HEADER
#define ENABLE_SHARED_MOUNT_INVENTORY_RAZOR93
//#define DISABLE_EXP_FROM_STONES_RAZOR93 //metinkövek nem adnak exp-t
#define ENABLE_COSTUME_EFFECT_ATTR_BONUS_RAZOR93 //fegyver,vért effect bónuszok olvasása proto apply_type
//#define DISABLE_CORE_PULSE_RAZOR93 // mountra fel le szállást nem tudják spamelni
//#define MOUNT_COUNT_UPDATE_TIME_RAZOR93 5 //eltarolja a mount countot,ha  valtozik a csak akkor küldi a packetet a kliensnek PASSES_PER_SEC
#define ENABLE_120_SHOP_SLOT_RAZOR93 // 3x nagyobb npc shop
#define ENABLE_MAX_100K_DMG_ON_EVENT_MAP_RAZOR93 // 1 es mapindexen mindenki 100k sebez 
#define DISABLE_PC_ATTACK_PC_ON_MAPIDEX1 // 1 es map indexen játékos nem sebez játékost
//#define DISABLE_DAMAGE_TYPE_NORMAL_RANGE_EVENT_MAP // event mapon nem sebez normál távolsági támadás
#define NAGYFASZU_MINING_CHANCE 60 // sikeres bányászat esélye
#define ENABLE_NINJA_SANGONG_X30_RAZOR93 //ninja köpés átlagos sebzés szorozva SKILLX értékkel
#ifdef ENABLE_NINJA_SANGONG_X30_RAZOR93
#define SKILLX 36 //
#endif

#define ENABLE_CHANGE_NORMAL_HIT_RAZOR93
//#define DISABLE_ZODIAC_ATT
#define ENABLE_EVENT_QUIZ_RAZOR93

#define LEADERBOARD_RAZOR93

#define ENABLE_EVENT_QUIZ_RAZOR93 //kvíz event
//#define ENABLE_METINSTONE_DROP_BUGFIX_RAZOR9// ez lufasz
#define SPAWN_COUNT 100 // /m valami 100 
//#define PET_EXPTABLE /home/Server/srv1/share/locale/germany/exppettable.txt


#define SSTONE_DROP_INFO_PTC 5 //constatts.cpp ben   érték szorzója //Razo93
#define LEADERBOARD_RAZOR93
#define ENABLE_MAP1_SKILL_MOB
#define ENABLE_APPLYTYPE5_RAZOR93 //applytype5 bónuszok
#ifndef ENABLE_APPLYTYPE5_RAZOR93
	#define APPLY_MAX_NUM_ 3 
#else
	#define APPLY_MAX_NUM_ 5 //applytype5 bónuszok miatt 5-re
#endif
#define ENABLE_NEWEXP_CALCULATION_RAZOR93	//új exp számítási mód Razor93
#define AFFECT_EXP_ITEM_RAZOR93 
#define DISABLE_USE_STAMINA_RAZOR93 //staminát nem von le
#define DISABLE_GOLD_DROP_FROM_TAKAKA	// takaka nem dob aranyat
	#ifdef DISABLE_GOLD_DROP_FROM_TAKAKA
#define TANAKA 5000 // takaka vnum
#endif
#define ENABLE_MINUS_COUNT_FIX_RAZOR93	// item count mínuszba nem mehet
#define ENABLE_AGGREGATE_MONSTER_PLUS_RAZOR93 
#define NEW_POINT_EXP_DOUBLE_BONUS_RAZOR93 // új pont exp dupla bónusz
#define ENABLE_MUSIN_SCROLL_REFINE_100_SUCCESS_RAZOR93 // musin scrollal 100% sikeres fejlesztés	

#define ENABLE_QUEST_SYSTEM_BUGFIXES // quest rendszer hibajavítások Razor93
#define ENABLE_FREE_PASS_RAZOR93 // ingame free pass item használat 
#define ENABLE_CHRISTMAS_WHEEL_OF_DESTINY //  sorskerék event battlepasshoz
#define ENABLE_MOUNT_INVENTORY_FIX_RAZOR93
#define HEADER_GD_PLAYER_SAVE_SIZE_CHECK
#define ENABLE_GIRD_BUG_FIX
#define ENABLE_DUNGEON_SHARED_DROP_HWID
#define ENABLE_STONE_SPAWN_STEP_PROCESSING_RAZOR93 // metinkövekböl kijön az összes mob ha 1 hitel ölik a követ 
#define ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93 // napi jutalom hwid limit
#define ENABLE_YANG_INSTANT_INVENTORY_RAZOR93
#define KET_BONUSZOS_KOVEK
#define ENABLE_ITEM_ON_TITLE_RAZOR93				// Item on title system (equip title item shows as "[tag]Name")
#define DISABLE_SKILL_BOOK_NEED_EXP
#define ENABLE_DROP_INSTANT_INVENTORY
#define ENABLE_OFFLINESHOP_CLEAR_CHACHE //napi 1 x éjjel törli az offline shop cache-t, hogy ne legyenek benne olyan itemek amik már sold
#define ENABLE_NEW_CRAFT_SYSTEM_RAZOR93
#endif