#pragma once

//#define __USE_CYTHON__
#define NEEDED_COMMAND_ARGUMENT


#define _IMPROVED_PACKET_ENCRYPTION_


#define LINK_IN_CHAT


//#define ENABLE_ANTICHEAT

#define ENABLE_LOAD_PROPERTY_XML



/* common */
#define ENABLE_EXTRA_INVENTORY
#define ENABLE_SCREEN_FREEZE_BUGFIX



#define ENABLE_TEXT_RENEWAL











//#define __USE_EXTRA_CYTHON__

//////////////////////////////////////////////////////////////////////////
// ### Default Ymir Macros ###
#define ENABLE_RANKING
#define LOCALE_SERVICE_EUROPE
#define ENABLE_COSTUME_SYSTEM
#define ENABLE_ENERGY_SYSTEM
#define ENABLE_DRAGON_SOUL_SYSTEM
#define ENABLE_NEW_EQUIPMENT_SYSTEM
#define ENABLE_BUY_WITH_ITEM
#ifdef ENABLE_BUY_WITH_ITEM
	#define MAX_SHOP_PRICES 5
#endif
#define ENABLE_NEW_SECONDARY_SKILLS
// ### Default Ymir Macros ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### New From LocaleInc ###
#define ENABLE_PACK_GET_CHECK
//#define ENABLE_CANSEEHIDDENTHING_FOR_GM
#define ENABLE_PROTOSTRUCT_AUTODETECT

#define ATTR_LOCK

#define ENABLE_PLAYER_PER_ACCOUNT5
#define ENABLE_LEVEL_IN_TRADE
#define ENABLE_DICE_SYSTEM
#define ENABLE_EXTEND_INVEN_SYSTEM
#define ENABLE_LVL115_ARMOR_EFFECT
#define ENABLE_SLOT_WINDOW_EX
#define ENABLE_TEXT_LEVEL_REFRESH
#define ENABLE_USE_COSTUME_ATTR

#define WJ_SHOW_MOB_INFO
#ifdef WJ_SHOW_MOB_INFO
#define ENABLE_SHOW_MOBAIFLAG
#define ENABLE_SHOW_MOBLEVEL
#endif

#define ENABLE_MAGIC_REDUCTION_SYSTEM
#define ENABLE_MOUNT_COSTUME_SYSTEM
#define ENABLE_WEAPON_COSTUME_SYSTEM
#define ENABLE_SWITCHBOT
#define ENABLE_SEND_TARGET_INFO
#define ENABLE_MAP_TELEPORTER
#define ENABLE_SAVEPOINT_SYSTEM
#define ENABLE_DEFENSE_WAVE
#define VERSION_162_ENABLED
#define NEW_PET_SYSTEM
#define ENABLE_WHISPER_ADMIN_SYSTEM
#define ENABLE_MELEY_LAIR_DUNGEON
#ifdef ENABLE_MELEY_LAIR_DUNGEON
	#define MELEY_LAIR_DUNGEON_STATUE 6118
#endif
#define ADVANCED_GUILD_INFO
#define ENABLE_CUBE_RENEWAL_WORLDARD
#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
	// #define ENABLE_CUBE_RENEWAL_GEM_WORLDARD		//Unused - Adapted with ENABLE_GAYA_SYSTEM (Don't Active this define!!)
	#define ENABLE_CUBE_RENEWAL_COPY_WORLDARD
#endif
#define ENABLE_GAYA_SYSTEM					// Gaya System Metin2
#define ENABLE_CAPITALE_SYSTEM				// Expanded Capital System
#define	__ENABLE_RANGE_ALCHEMY__			// Open shop everywhere
#define __ENABLE_REFINE_ALCHEMY__			// Open Refine Window everywhere
#define	ENABLE_STRONG_BOSS					// Bonus strong against Boss
#define ENABLE_STRONG_METIN					// Bonus strong against Metin
#define ENABLE_RESIST_MONSTER				// New Bonus Resist Monster
#define ENABLE_MEDI_PVM						// New Bonus Medi Pvm
#define ELEMENT_NEW_BONUSES					// Bonus Elemental Resistence (Fire, Ice, Dark, Earth, Elect, Wind)
#ifdef ELEMENT_NEW_BONUSES					//
#define ENABLE_PENDANT						// New Item Talisman Resistence (Item Metin2 17.5)
#define ENABLE_NEW_BONUS_TALISMAN			// New Bonus DB for Talisman (Metin2 17.5)
#define ENABLE_TALISMAN_EFFECT  // Effect Talisman like Acce
#endif
#define ENABLE_VIEW_TARGET_PLAYER_HP		// Show Decimal HP
#ifdef ENABLE_VIEW_TARGET_PLAYER_HP			//
#define ENABLE_VIEW_TARGET_DECIMAL_HP		//
#endif
#define __CHANNEL_CHANGE_SYSTEM__			// Instant Change Channel
#define ENABLE_DATETIME_UNDER_MINIMAP		// DateTime (ONly PythonApplicationModule.cpp)
#define ENABLE_HIDE_COSTUME_SYSTEM			// Hide costume parts (Only PythonApplicationModule.cpp)
#define ENABLE_SORT_INVEN					// Sort Inventory (Only PythonApplicationModule.cpp)
#define __ENABLE_EXTEND_INVEN_SYSTEM__		// Extend Official Inventory witk Key
#define ENABLE_SOUL_SYSTEM					//System Anima
#define ENABLE_NEW_PASSIVE_SKILLS			//Systemm skill passive
// if is define ENABLE_ACCE_SYSTEM the players can use shoulder sash 
// if you want to use object scaling function you must defined ENABLE_OBJ_SCALLING
#define ENABLE_ACCE_SYSTEM
#define ENABLE_OBJ_SCALLING
//#define ENABLE_PLAYER_PIN_SYSTEM // Player PIN
#define ENABLE_QUEST_RENEWAL				// Quest page renewal
#define	ENABLE_FEATURES_REFINE_SYSTEM		// Refine System
#define __ENABLE_NEW_EFFECT_CIANITE__		// New Effect for Armor Cianite	
#define __ENABLE_NEW_EFFECT_CIANITE_WEAPON__	// New Effect for Weapon Cianite	
#define __ENABLE_NEW_EFFECT_ZODIACO_WEAPON1__	// New Effect for Weapon1 Zodiaco	
#define __ENABLE_NEW_EFFECT_ZODIACO__		// New Effect for Armor Zodiaco	
#define __ENABLE_NEW_EFFECT_ZODIACO1__		// New Effect for Armor1 Zodiaco	
#define __ENABLE_NEW_EFFECT_ZODIACO_WEAPON__	// New Effect for Weapon Zodiaco	
#define ENABLE_SKILL_COLOR_SYSTEM 			// Skill color
#ifdef ENABLE_SKILL_COLOR_SYSTEM
#define ENABLE_5LAYER_SKILL_COLOR 			// Enable 5 layers for skill color
#endif
#define ENABLE_CONFIG_MODULE 					// Enable configuration module for saving settings
#define ENABLE_BATTLE_PASS
#define ENABLE_NEW_EXCHANGE_WINDOW
#define ENABLE_PVP_ADVANCED
#define EQUIP_ENABLE_VIEW_SASH
#define ENABLE_MULTI_LANGUAGE				// MultiLanguage
//OFFSHOP
#define __ENABLE_EMOJI_SYSTEM__
#define __ENABLE_NEW_OFFLINESHOP__
#define ENABLE_OFFLINESHOP_DEBUG
#define ENABLE_NEW_SHOP_IN_CITIES
#define BLOCK_AUTO_ATTACK_PLAYER


//HOTTIES IKARUS STUFFS
#define ENABLE_ITEM_EXTRA_PROTO
#define ENABLE_RARITY_SYSTEM
#define ENABLE_NEW_EXTRA_BONUS

#define ENABLE_ATTR_TRANSFER_SYSTEM
#define ENABLE_ATTR_COSTUMES
#if defined(ENABLE_OFFLINESHOP_DEBUG) && defined(_DEBUG)
#define OFFSHOP_DEBUG(fmt , ...) Tracenf("%s:%d >> " fmt , __FUNCTION__ , __LINE__, __VA_ARGS__)
#else
#define OFFSHOP_DEBUG(...)   
#endif

#define ENABLE_DUNGEON_INFO_SYSTEM
#define ENABLE_LOCKED_EXTRA_INVENTORY
#define ENABLE_DS_SET
#define ENABLE_DS_ENCHANT
#define ENABLE_HIGHLIGHT_SYSTEM
#define ENABLE_NEW_PET_EDITS
#define ENABLE_REMOTE_ATTR_SASH_REMOVE
#define __ENABLE_NEW_EFFECT_STOLE__
#define ENABLE_ATLAS_BOSS
#define ENABLE_STOLE_REAL
#define ENABLE_STOLE_COSTUME
#define ENABLE_COSTUME_PET
#define ENABLE_COSTUME_MOUNT
#define ENABLE_COSTUME_EFFECT

#define KASMIR_PAKET_SYSTEM
#define ENABLE_EFFECT_WEAPON_COSTUME
#define ENABLE_BUGFIX_EFFECT_FAN
#define ENABLE_DS_RUNE
#define ENABLE_BLOCK_MULTIFARM
#define ENABLE_BUGFIXES_NOTDONE
#define BL_OFFLINE_MESSAGE
#define ENABLE_RUNE_SYSTEM
#define ENABLE_NEW_USE_POTION
#define __EFFETTO_MANTELLO__
#define ENABLE_LIMIT_PUSH_DEST
#define ENABLE_NEW_RESTART
#define ENABLE_RACE_HEIGHT
#define DISABLE_MENU_IF_KEY_F10
#define ENABLE_VIEW_ELEMENT
#define ENABLE_RECALL
#define ENABLE_NEW_COMMON_BONUSES
#define ENABLE_PERSPECTIVE_VIEW
#define ENABLE_UI_EXTRA
#define ENABLE_NEW_CHAT
#define ENABLE_DS_GRADE_MYTH
#define ENABLE_NO_COLLISION
#define TEXTS_IMPROVEMENT
#define __ENABLE_LARGE_DYNAMIC_PACKET__
#define ENABLE_INFINITE_RAFINES
#define ENABLE_BIOLOGIST_UI
#define ENABLE_EMPIRE_EFFECT_LIMITED
#define ENABLE_DS_POTION_DIFFRENT
#define ENABLE_NEW_FISHING_SYSTEM
#define ENABLE_NEW_BUGFIXES
#define WJ_ENABLE_TRADABLE_ICON
#define ENABLE_3D_MODELS_TEXTURE_FIX
#define ENABLE_NEW_GYEONGGONG_SKILL
#define ENABLE_SAVECAMERA_PREFERENCES
//#define ENABLE_MAP_PERFORMANCE
#define ENABLE_GENDER_ALIGNMENT
#define DISABLE_CRITICAL_EFFECT
#define OUTLINE_NAMES_TEXTLINE
#define ENABLE_MULTI_NAMES
#define NORMAL_ROTATION_SPEED 2400.0f
#define MOUNT_ROTATION_SPEED 1200.0f
#define ENABLE_CHOOSE_DOCTRINE_GUI
#define ENABLE_ITEMSHOP_ITEM
#define ENABLE_BUGFIXES
#define ENABLE_CONQUEROR_LEVEL
#define SPECIAL_ACTION_START_INDEX 101



#define ENABLE_AUTO_PICKUP
#define ENABLE_INSTANT_PICKUP
#define ENABLE_DS_REFINE_ALL
#define ENABLE_NEW_ATTACK_METHOD
#define ENABLE_BUY_STACK_FROM_SHOP
#ifdef ENABLE_BUY_STACK_FROM_SHOP
#define MULTIPLE_BUY_LIMIT 100
#else
#define MULTIPLE_BUY_LIMIT 0
#endif
#define ENABLE_OPENSHOP_PACKET
#define ENABLE_HWID
//#define ENABLE_VOTE_FOR_BONUS
//#define ENABLE_VOTE4BUFF		// Ixtreeme 20250216
#define ENABLE_MESSENGER_TEAM
#define ENABLE_MESSENGER_HELPER
#define ENABLE_CHRISTMAS_2021
#ifdef ENABLE_CHRISTMAS_2021
#define ENABLE_CHRISTMAS_WHEEL_OF_DESTINY

#endif



#define ENABLE_EVENT_MANAGER	// Ixtreeme
#define __AUTO_QUQUE_ATTACK__	// Ixtreeme
#define ENABLE_ITEMSHOP			// Ixtreeme


//RAZOR93 DEFINES------------------------------------------------------------------
#define MEMORY_LEAK_FIX_RAZRO93
#define ENABLE_TARGET_DAMAGE_RAZOR93
#define ENABLE_GUILD_ATTRIBUTE						// Céh bónusz rendszer
//#define __BL_PICK_FILTER__

#define ENABLE_FAKE_SHOP_HEADER
#define ENABLE_120_SHOP_SLOT_RAZOR93
#define ENABLE_FMOD_SOUND_BUGFIX_RAZOR
#define DIFERENT_WATERTYPE_RAZOR93
#ifdef DIFERENT_WATERTYPE_RAZOR93
	#define WATERTYPE_COUNT	99
#endif
#define ENABLE_REFINE_RENEWAL
#define LEADERBOARD_RAZOR93


#ifdef __AUTO_QUQUE_ATTACK__
	#define AUTO_QUQUE_ATTACK_MAX_TARGET 5
#endif
#define ENABLE_APPLYTYPE5_RAZOR93 //applytype5 bónuszok
#ifndef ENABLE_APPLYTYPE5_RAZOR93
#define APPLY_MAX_NUM_ 3 
#else
#define APPLY_MAX_NUM_ 5 //applytype5 bónuszok miatt 5-re
#endif
#define AFFECT_EXP_ITEM_RAZOR93
/* unused
//#define ENABLE_MOUNT_COUNT_ABOVE_CHAR_RAZOR93
//#define ENABLE_GUILD_LV_SHOW_ABOVE_CHAR_RAZOR93 nincs kész packet funkcioja. csak te látod
//#define __PERFORMANCE_CHECKER__
//#define _PACKETDUMP
*/
#define ENABLE_FREE_PASS_RAZOR93
#define AUTO_SHOUT

#define ENABLE_REFINE_RENEWAL
#define MAX_HIT_COUNT 100
#define ENABLE_RENEWAL_AFFECT_SHOWER // improve affect shower

//NsBácsi includes

//#define ENABLE_LOADING_DELAY
// Guild Renewal Razor93
#define ENABLE_GUILD_RENEWAL_BY_RAZOR93
#define ENABLE_GUILD_LV_SHOW_ABOVE_CHAR_RAZOR93
#define ENABLE_SHINING_SYSTEM
#define ENABLE_ITEM_ON_TITLE_RAZOR93
#define ENABLE_CANSEEHIDDENTHING_FOR_GM