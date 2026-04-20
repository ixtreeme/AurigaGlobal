#include "stdafx.h"
#include "constants.h"
#include "config.h"
#include "utils.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "item.h"
#include "item_manager.h"
#include "packet.h"
#include "protocol.h"
#include "mob_manager.h"
#include "shop_manager.h"
#include "sectree_manager.h"
#include "skill.h"
#include "questmanager.h"
#include "p2p.h"
#include "guild.h"
#include "guild_manager.h"
#include "start_position.h"
#include "party.h"
#include "refine.h"
#include "banword.h"
#include "priv_manager.h"
#include "db.h"
#include "building.h"

#include "wedding.h"
#include "login_data.h"
#include "unique_item.h"

#include "affect.h"
#include "motion.h"

#include "dev_log.h"

#include "log.h"

#include "horsename_manager.h"
#include "pcbang.h"
#include "gm.h"
#include "map_location.h"
#include "DragonSoul.h"

#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
#endif

#include "shutdown_manager.h"
#include <common/CommonDefines.h>

#include "desc_client.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/VIDRegistry.hpp"
#include "ecs/components/vital_components.hpp"
#include "ecs/components/inventory_components.hpp"
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
#endif
#ifdef ENABLE_HWID
#include "hwidmanager.h"
#endif

#define MAPNAME_DEFAULT	"none"

bool GetServerLocation(TAccountTable & rTab, uint8_t bEmpire)
{
	bool bFound = false;

	for (auto& player : rTab.players)
	{
		if (0 == player.dwID)
			continue;

		bFound = true;
		int32_t lIndex = 0;

		if (!CMapLocation::instance().Get(
#ifdef ENABLE_GENERAL_CH
rTab.bChannel, 
#endif
player.x,
player.y,
					lIndex,
player.lAddr,
player.wPort))
		{
			sys_err("location error name %s mapindex %d %d x %d empire %d",
			        player.szName, lIndex, player.x, player.y, rTab.bEmpire);

			player.x = EMPIRE_START_X(rTab.bEmpire);
			player.y = EMPIRE_START_Y(rTab.bEmpire);

			lIndex = 0;

			if (!CMapLocation::instance().Get(
#ifdef ENABLE_GENERAL_CH
rTab.bChannel, 
#endif
player.x, player.y, lIndex, player.lAddr, player.wPort))
			{
				sys_err("cannot find server for mapindex %d %d x %d (name %s)",
						lIndex,
						player.x,
						player.y,
						player.szName);

				continue;
			}
		}

		struct in_addr in;
		in.s_addr = player.lAddr;
		sys_log(0, "success to %s:%d", inet_ntoa(in), player.wPort);
	}

	return bFound;
}


void CInputDB::LoginSuccess(uint32_t dwHandle, const char *data)
{
	sys_log(0, "LoginSuccess");

	TAccountTable* pTab = (TAccountTable*)data;

	LPDESC d = DESC_MANAGER::instance().FindByHandle(dwHandle);

	if (!d)
	{
		sys_log(0, "CInputDB::LoginSuccess - cannot find handle [%s]", pTab->login);

		TLogoutPacket pack;

		strlcpy(pack.login, pTab->login, sizeof(pack.login));
		db_clientdesc->DBPacket(HEADER_GD_LOGOUT, dwHandle, &pack, sizeof(pack));
		return;
	}

	if (strcmp(pTab->status, "OK")) // OK∞° æ∆¥œ∏È
	{
		sys_log(0, "CInputDB::LoginSuccess - status[%s] is not OK [%s]", pTab->status, pTab->login);

		TLogoutPacket pack;

		strlcpy(pack.login, pTab->login, sizeof(pack.login));
		db_clientdesc->DBPacket(HEADER_GD_LOGOUT, dwHandle, &pack, sizeof(pack));

		LoginFailure(d, pTab->status);
		return;
	}

	for (int i = 0; i != PLAYER_PER_ACCOUNT; ++i)
	{
		TSimplePlayer& player = pTab->players[i];
		sys_log(0, "\tplayer(%s).job(%d)", player.szName, player.byJob);
	}

	bool bFound = GetServerLocation(*pTab, pTab->bEmpire);

//#ifdef ENABLE_GENERAL_CH
//	pTab->bChannel = g_bChannel;
//#endif

	d->BindAccountTable(pTab);


	if (!bFound) // ƒ≥∏Ø≈Õ∞° æ¯¿∏∏È ∑£¥˝«— ¡¶±π¿∏∑Œ ∫∏≥Ω¥Ÿ.. -_-
	{
		TPacketGCEmpire pe;
		pe.bHeader = HEADER_GC_EMPIRE;
		pe.bEmpire = number(1, 3);
		d->Packet(&pe, sizeof(pe));
	}
	else
	{
		TPacketGCEmpire pe;
		pe.bHeader = HEADER_GC_EMPIRE;
		pe.bEmpire = d->GetEmpire();
		d->Packet(&pe, sizeof(pe));
	}

	d->SetPhase(PHASE_SELECT);
	d->SendLoginSuccessPacket();

	// __SHUTDOWN::Shutdown Register
	CShutdownManager::Instance().AddDesc(d);

	sys_log(0, "InputDB::login_success: %s", pTab->login);
}

void CInputDB::PlayerCreateFailure(LPDESC d, uint8_t bType)
{
	if (!d)
		return;

	TPacketGCCreateFailure pack;

	pack.header	= HEADER_GC_CHARACTER_CREATE_FAILURE;
	pack.bType	= bType;

	d->Packet(&pack, sizeof(pack));
}

void CInputDB::PlayerCreateSuccess(LPDESC d, const char * data)
{
	if (!d)
		return;

	TPacketDGCreateSuccess * pPacketDB = (TPacketDGCreateSuccess *) data;

	if (pPacketDB->bAccountCharacterIndex >= PLAYER_PER_ACCOUNT)
	{
		d->Packet(encode_byte(HEADER_GC_CHARACTER_CREATE_FAILURE), 1);
		return;
	}

	int32_t lIndex;

	if (!CMapLocation::instance().Get(
#ifdef ENABLE_GENERAL_CH
d->GetAccountTable().bChannel, 
#endif
				pPacketDB->player.x,
				pPacketDB->player.y,
				lIndex,
				pPacketDB->player.lAddr,
				pPacketDB->player.wPort))
	{
		sys_err("InputDB::PlayerCreateSuccess: cannot find server for mapindex %d %d x %d (name %s)",
				lIndex,
				pPacketDB->player.x,
				pPacketDB->player.y,
				pPacketDB->player.szName);
	}

	TAccountTable & r_Tab = d->GetAccountTable();
	r_Tab.players[pPacketDB->bAccountCharacterIndex] = pPacketDB->player;

	TPacketGCPlayerCreateSuccess pack;

	pack.header = HEADER_GC_CHARACTER_CREATE_SUCCESS;
	pack.bAccountCharacterIndex = pPacketDB->bAccountCharacterIndex;
	pack.player = pPacketDB->player;

	d->Packet(&pack, sizeof(TPacketGCPlayerCreateSuccess));

#ifdef ENABLE_REWARD_AT_START
	TPlayerItem t;
	memset(&t, 0, sizeof(t));
	t.owner	= r_Tab.players[pPacketDB->bAccountCharacterIndex].dwID;

	struct SInitialItem
	{
		uint8_t	window;
		uint16_t	pos;
		uint32_t	count;
		uint32_t	dwVnum;
		TPlayerItemAttribute	aAttr[5];
	};

	
	static SInitialItem initialItems[MAIN_RACE_MAX_NUM][13] =
	{
		/* MAIN_RACE_WARRIOR_M */
		{
			//{INVENTORY, 0, 1, 19, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 2 slot
			//{INVENTORY, 1, 1, 3009, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 3 slot
			//{INVENTORY, 2, 1, 11209, {APPLY_MAX_HP, 2500, APPLY_ATT_GRADE_BONUS, 5, APPLY_STEAL_HP, 1, APPLY_REFLECT_MELEE, 1, APPLY_CAST_SPEED, 20}},					// 2 slot
			{INVENTORY, 3, 1, 12209, {APPLY_HP_REGEN, 5, APPLY_POISON_PCT, 10, APPLY_DODGE, 3, APPLY_ATT_SPEED, 10, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 4, 1, 13009, {APPLY_IMMUNE_STUN, 1, APPLY_STR, 1, APPLY_BLOCK, 2, APPLY_REFLECT_MELEE, 1, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 8, 1, 14009, {APPLY_MAX_HP, 2500, APPLY_PENETRATE_PCT, 1, APPLY_STEAL_HP, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ITEM_DROP_BONUS, 1}},					// 1 slot
			{INVENTORY, 9, 1, 15009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_DODGE, 3, APPLY_STUN_PCT, 8, APPLY_GOLD_DOUBLE_BONUS, 20}},						// 1 slot
			{INVENTORY, 10, 1, 16009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_HP_REGEN, 5, APPLY_STUN_PCT, 8}},						// 1 slot
			{INVENTORY, 12, 1, 17009, {APPLY_POISON_REDUCE, 10, APPLY_ITEM_DROP_BONUS, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ATTBONUS_MILGYO, 20, APPLY_ATTBONUS_ANIMAL, 20}},	// 1 slot
			{INVENTORY, 13, 1, 50187, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			{INVENTORY, 14, 1, 50188, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			//{INVENTORY, 15, 1, 88224, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}

		},
		/* MAIN_RACE_ASSASSIN_W */
		{
			//{INVENTORY, 0, 1, 1009, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 1 slot
			//{INVENTORY, 1, 1, 2009, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 2 slot
			//{INVENTORY, 2, 1, 19, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 2 slot
			//{INVENTORY, 3, 1, 11409, {APPLY_MAX_HP, 2500, APPLY_ATT_GRADE_BONUS, 5, APPLY_STEAL_HP, 1, APPLY_REFLECT_MELEE, 1, APPLY_CAST_SPEED, 20}},					// 2 slot
			{INVENTORY, 4, 1, 12349, {APPLY_HP_REGEN, 5, APPLY_POISON_PCT, 10, APPLY_DODGE, 3, APPLY_ATT_SPEED, 10, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 5, 1, 13009, {APPLY_IMMUNE_STUN, 1, APPLY_STR, 1, APPLY_BLOCK, 2, APPLY_REFLECT_MELEE, 1, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 9, 1, 14009, {APPLY_MAX_HP, 2500, APPLY_PENETRATE_PCT, 1, APPLY_STEAL_HP, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ITEM_DROP_BONUS, 1}},					// 1 slot
			{INVENTORY, 10, 1, 15009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_DODGE, 3, APPLY_STUN_PCT, 8, APPLY_GOLD_DOUBLE_BONUS, 20}},						// 1 slot
			{INVENTORY, 11, 1, 16009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_HP_REGEN, 5, APPLY_STUN_PCT, 8}},						// 1 slot
			{INVENTORY, 12, 1, 17009, {APPLY_POISON_REDUCE, 10, APPLY_ITEM_DROP_BONUS, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ATTBONUS_MILGYO, 20, APPLY_ATTBONUS_ANIMAL, 20}},	// 1 slot
			{INVENTORY, 13, 1, 50187, {0, 10, 0, 0, 0, 0, 0, 0, 0, 0}},
			{INVENTORY, 14, 1, 50188, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0} },
			//{INVENTORY, 15, 1, 88224, {0, 10, 0, 0, 0, 0, 0, 0, 0, 0}}
		},
		/* MAIN_RACE_SURA_M */
		{
			//{INVENTORY, 0, 1, 19, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 2 slot
			//{INVENTORY, 1, 1, 11609, {APPLY_MAX_HP, 2500, APPLY_ATT_GRADE_BONUS, 5, APPLY_STEAL_HP, 1, APPLY_REFLECT_MELEE, 1, APPLY_CAST_SPEED, 20}},					// 2 slot
			{INVENTORY, 2, 1, 12489, {APPLY_HP_REGEN, 5, APPLY_POISON_PCT, 10, APPLY_DODGE, 3, APPLY_ATT_SPEED, 10, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 3, 1, 13009, {APPLY_IMMUNE_STUN, 1, APPLY_STR, 1, APPLY_BLOCK, 2, APPLY_REFLECT_MELEE, 1, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 4, 1, 14009, {APPLY_MAX_HP, 2500, APPLY_PENETRATE_PCT, 1, APPLY_STEAL_HP, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ITEM_DROP_BONUS, 1}},					// 1 slot
			{INVENTORY, 7, 1, 15009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_DODGE, 3, APPLY_STUN_PCT, 8, APPLY_GOLD_DOUBLE_BONUS, 20}},						// 1 slot
			{INVENTORY, 8, 1, 16009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_HP_REGEN, 5, APPLY_STUN_PCT, 8}},							// 1 slot
			{INVENTORY, 9, 1, 17009, {APPLY_POISON_REDUCE, 10, APPLY_ITEM_DROP_BONUS, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ATTBONUS_MILGYO, 20, APPLY_ATTBONUS_ANIMAL, 20}},	// 1 slot
			{INVENTORY, 10, 1, 50187, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			{INVENTORY, 14, 1, 50188, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			//{INVENTORY, 15, 1, 88224, {0, 10, 0, 0, 0, 0, 0, 0, 0, 0}}

		},
		/* MAIN_RACE_SHAMAN_W */
		{
			//{INVENTORY, 0, 1, 5009, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 1 slot
			//{INVENTORY, 1, 1, 7009, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 1 slot
			//{INVENTORY, 2, 1, 11809, {APPLY_MAX_HP, 2500, APPLY_ATT_GRADE_BONUS, 5, APPLY_STEAL_HP, 1, APPLY_REFLECT_MELEE, 1, APPLY_CAST_SPEED, 20}},					// 2 slot
			{INVENTORY, 3, 1, 12629, {APPLY_HP_REGEN, 5, APPLY_POISON_PCT, 10, APPLY_DODGE, 3, APPLY_ATT_SPEED, 10, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 4, 1, 13009, {APPLY_IMMUNE_STUN, 1, APPLY_STR, 1, APPLY_BLOCK, 2, APPLY_REFLECT_MELEE, 1, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 5, 1, 14009, {APPLY_MAX_HP, 2500, APPLY_PENETRATE_PCT, 1, APPLY_STEAL_HP, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ITEM_DROP_BONUS, 1}},					// 1 slot
			{INVENTORY, 6, 1, 15009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_DODGE, 3, APPLY_STUN_PCT, 8, APPLY_GOLD_DOUBLE_BONUS, 20}},						// 1 slot
			{INVENTORY, 8, 1, 16009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_HP_REGEN, 5, APPLY_STUN_PCT, 8}},							// 1 slot
			{INVENTORY, 9, 1, 17009, {APPLY_POISON_REDUCE, 10, APPLY_ITEM_DROP_BONUS, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ATTBONUS_MILGYO, 20, APPLY_ATTBONUS_ANIMAL, 20}},	// 1 slot
			{INVENTORY, 10, 1, 50187, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			{INVENTORY, 14, 1, 50188, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			//{INVENTORY, 15, 1, 88224, {0, 10, 0, 0, 0, 0, 0, 0, 0, 0}}
		},
		/* MAIN_RACE_WARRIOR_W */
		{
			//{INVENTORY, 0, 1, 19, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 2 slot
			//{INVENTORY, 1, 1, 3009, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 3 slot
			//{INVENTORY, 2, 1, 11209, {APPLY_MAX_HP, 2500, APPLY_ATT_GRADE_BONUS, 5, APPLY_STEAL_HP, 1, APPLY_REFLECT_MELEE, 1, APPLY_CAST_SPEED, 20}},					// 2 slot
			{INVENTORY, 3, 1, 12209, {APPLY_HP_REGEN, 5, APPLY_POISON_PCT, 10, APPLY_DODGE, 3, APPLY_ATT_SPEED, 10, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 4, 1, 13009, {APPLY_IMMUNE_STUN, 1, APPLY_STR, 1, APPLY_BLOCK, 2, APPLY_REFLECT_MELEE, 1, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 8, 1, 14009, {APPLY_MAX_HP, 2500, APPLY_PENETRATE_PCT, 1, APPLY_STEAL_HP, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ITEM_DROP_BONUS, 1}},					// 1 slot
			{INVENTORY, 9, 1, 15009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_DODGE, 3, APPLY_STUN_PCT, 8, APPLY_GOLD_DOUBLE_BONUS, 20}},						// 1 slot
			{INVENTORY, 10, 1, 16009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_HP_REGEN, 5, APPLY_STUN_PCT, 8}},						// 1 slot
			{INVENTORY, 12, 1, 17009, {APPLY_POISON_REDUCE, 10, APPLY_ITEM_DROP_BONUS, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ATTBONUS_MILGYO, 20, APPLY_ATTBONUS_ANIMAL, 20}},	// 1 slot
			{INVENTORY, 13, 1, 50187, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			{INVENTORY, 14, 1, 50188, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			//{INVENTORY, 15, 1, 88224, {0, 10, 0, 0, 0, 0, 0, 0, 0, 0}}
		},
		/* MAIN_RACE_ASSASSIN_M */
		{
			//{INVENTORY, 0, 1, 1009, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 1 slot
			//{INVENTORY, 1, 1, 2009, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 2 slot
			//{INVENTORY, 2, 1, 19, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 2 slot
			{INVENTORY, 3, 1, 11409, {APPLY_MAX_HP, 2500, APPLY_ATT_GRADE_BONUS, 5, APPLY_STEAL_HP, 1, APPLY_REFLECT_MELEE, 1, APPLY_CAST_SPEED, 20}},					// 2 slot
			{INVENTORY, 4, 1, 12349, {APPLY_HP_REGEN, 5, APPLY_POISON_PCT, 10, APPLY_DODGE, 3, APPLY_ATT_SPEED, 10, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 5, 1, 13009, {APPLY_IMMUNE_STUN, 1, APPLY_STR, 1, APPLY_BLOCK, 2, APPLY_REFLECT_MELEE, 1, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 9, 1, 14009, {APPLY_MAX_HP, 2500, APPLY_PENETRATE_PCT, 1, APPLY_STEAL_HP, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ITEM_DROP_BONUS, 1}},					// 1 slot
			{INVENTORY, 10, 1, 15009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_DODGE, 3, APPLY_STUN_PCT, 8, APPLY_GOLD_DOUBLE_BONUS, 20}},						// 1 slot
			{INVENTORY, 11, 1, 16009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_HP_REGEN, 5, APPLY_STUN_PCT, 8}},						// 1 slot
			{INVENTORY, 12, 1, 17009, {APPLY_POISON_REDUCE, 10, APPLY_ITEM_DROP_BONUS, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ATTBONUS_MILGYO, 20, APPLY_ATTBONUS_ANIMAL, 20}},	// 1 slot
			{INVENTORY, 13, 1, 50187, {0, 10, 0, 0, 0, 0, 0, 0, 0, 0}},
			{INVENTORY, 14, 1, 50188, {0, 10, 0, 0, 0, 0, 0, 0, 0, 0}},
			//{INVENTORY, 15, 1, 88224, {0, 10, 0, 0, 0, 0, 0, 0, 0, 0}}
		},
		/* MAIN_RACE_SURA_W */
		{
			//{INVENTORY, 0, 1, 19, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 2 slot
			//{INVENTORY, 1, 1, 11609, {APPLY_MAX_HP, 2500, APPLY_ATT_GRADE_BONUS, 5, APPLY_STEAL_HP, 1, APPLY_REFLECT_MELEE, 1, APPLY_CAST_SPEED, 20}},					// 2 slot
			{INVENTORY, 2, 1, 12489, {APPLY_HP_REGEN, 5, APPLY_POISON_PCT, 10, APPLY_DODGE, 3, APPLY_ATT_SPEED, 10, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 3, 1, 13009, {APPLY_IMMUNE_STUN, 1, APPLY_STR, 1, APPLY_BLOCK, 2, APPLY_REFLECT_MELEE, 1, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 4, 1, 14009, {APPLY_MAX_HP, 2500, APPLY_PENETRATE_PCT, 1, APPLY_STEAL_HP, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ITEM_DROP_BONUS, 1}},					// 1 slot
			{INVENTORY, 7, 1, 15009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_DODGE, 3, APPLY_STUN_PCT, 8, APPLY_GOLD_DOUBLE_BONUS, 20}},						// 1 slot
			{INVENTORY, 8, 1, 16009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_HP_REGEN, 5, APPLY_STUN_PCT, 8}},							// 1 slot
			{INVENTORY, 9, 1, 17009, {APPLY_POISON_REDUCE, 10, APPLY_ITEM_DROP_BONUS, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ATTBONUS_MILGYO, 20, APPLY_ATTBONUS_ANIMAL, 20}},	// 1 slot
			{INVENTORY, 10, 1, 50187, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			{INVENTORY, 14, 1, 50188, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			//{INVENTORY, 15, 1, 88224, {0, 10, 0, 0, 0, 0, 0, 0, 0, 0}}
		},
		/* MAIN_RACE_SHAMAN_M */
		{
			//{INVENTORY, 0, 1, 5009, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 1 slot
			//{INVENTORY, 1, 1, 7009, {APPLY_STR, 1, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_POISON_PCT, 8, APPLY_STUN_PCT, 8}},								// 1 slot
			//{INVENTORY, 2, 1, 11809, {APPLY_MAX_HP, 2500, APPLY_ATT_GRADE_BONUS, 5, APPLY_STEAL_HP, 1, APPLY_REFLECT_MELEE, 1, APPLY_CAST_SPEED, 20}},					// 2 slot
			{INVENTORY, 3, 1, 12629, {APPLY_HP_REGEN, 5, APPLY_POISON_PCT, 10, APPLY_DODGE, 3, APPLY_ATT_SPEED, 10, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 4, 1, 13009, {APPLY_IMMUNE_STUN, 1, APPLY_STR, 1, APPLY_BLOCK, 2, APPLY_REFLECT_MELEE, 1, APPLY_ATTBONUS_ORC, 20}},								// 1 slot
			{INVENTORY, 5, 1, 14009, {APPLY_MAX_HP, 2500, APPLY_PENETRATE_PCT, 1, APPLY_STEAL_HP, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ITEM_DROP_BONUS, 1}},					// 1 slot
			{INVENTORY, 6, 1, 15009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_DODGE, 3, APPLY_STUN_PCT, 8, APPLY_GOLD_DOUBLE_BONUS, 20}},						// 1 slot
			{INVENTORY, 8, 1, 16009, {APPLY_MAX_HP, 2500, APPLY_CRITICAL_PCT, 5, APPLY_PENETRATE_PCT, 1, APPLY_HP_REGEN, 5, APPLY_STUN_PCT, 8}},							// 1 slot
			{INVENTORY, 9, 1, 17009, {APPLY_POISON_REDUCE, 10, APPLY_ITEM_DROP_BONUS, 1, APPLY_ATTBONUS_ORC, 20, APPLY_ATTBONUS_MILGYO, 20, APPLY_ATTBONUS_ANIMAL, 20}},	// 1 slot
			{INVENTORY, 10, 1, 50187, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			{INVENTORY, 14, 1, 50188, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
			//{INVENTORY, 15, 1, 88224, {0, 10, 0, 0, 0, 0, 0, 0, 0, 0}}
		}
	};


	unsigned job = pPacketDB->player.byJob;
	for (int i = 0; i < 11; i++) {
		if (initialItems[job][i].dwVnum == 0)
			continue;

		t.id = ITEM_MANAGER::instance().GetNewID();
		t.window = initialItems[job][i].window;
		t.pos = initialItems[job][i].pos;
		t.count = initialItems[job][i].count;
		t.vnum = initialItems[job][i].dwVnum;
		for (int x = 0; x < ITEM_SOCKET_MAX_NUM; ++x) {
			t.alSockets[x] = 0;
		}

		for (int x = 0; x < 5; ++x) {
			t.aAttr[x].bType = initialItems[job][i].aAttr[x].bType;
			t.aAttr[x].sValue = initialItems[job][i].aAttr[x].sValue;
		}

#ifdef ATTR_LOCK
		t.lockedattr = -1;
#endif

		db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_SAVE, 0, sizeof(TPlayerItem));
		db_clientdesc->Packet(&t, sizeof(TPlayerItem));
	}
#endif

	LogManager::instance().CharLog(pack.player.dwID, 0, 0, 0, "CREATE PLAYER", "", d->GetHostName());
}

void CInputDB::PlayerDeleteSuccess(LPDESC d, const char * data)
{
	if (!d)
		return;

	uint8_t account_index;
	account_index = decode_byte(data);
	d->BufferedPacket(encode_byte(HEADER_GC_CHARACTER_DELETE_SUCCESS),	1);
	d->Packet(encode_byte(account_index),			1);

	d->GetAccountTable().players[account_index].dwID = 0;
}

void CInputDB::PlayerDeleteFail(LPDESC d)
{
	if (!d)
		return;

	d->Packet(encode_byte(HEADER_GC_CHARACTER_DELETE_WRONG_SOCIAL_ID),	1);
	//d->Packet(encode_byte(account_index),			1);

	//d->GetAccountTable().players[account_index].dwID = 0;
}

void CInputDB::ChangeName(LPDESC d, const char * data)
{
	if (!d)
		return;

	TPacketDGChangeName * p = (TPacketDGChangeName *) data;

	TAccountTable & r = d->GetAccountTable();

	if (!r.id)
		return;

	for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
		if (r.players[i].dwID == p->pid)
		{
			strlcpy(r.players[i].szName, p->name, sizeof(r.players[i].szName));
			r.players[i].bChangeName = 0;

			TPacketGCChangeName pgc;

			pgc.header = HEADER_GC_CHANGE_NAME;
			pgc.pid = p->pid;
			strlcpy(pgc.name, p->name, sizeof(pgc.name));

			d->Packet(&pgc, sizeof(TPacketGCChangeName));
			break;
		}
}

//#define ENABLE_GOHOME_IF_MAP_NOT_EXIST
void CInputDB::PlayerLoad(LPDESC d, const char * data)
{
	TPlayerTable * pTab = (TPlayerTable *) data;

	if (!d)
		return;

	int32_t lMapIndex = pTab->lMapIndex;
	PIXEL_POSITION pos;

	if (lMapIndex == 0)
	{
		lMapIndex = SECTREE_MANAGER::instance().GetMapIndex(pTab->x, pTab->y);

		if (lMapIndex == 0) // ¡¬«•∏¶ √£¿ª ºˆ æ¯¥Ÿ.
		{
			lMapIndex = EMPIRE_START_MAP(d->GetAccountTable().bEmpire);
			pos.x = EMPIRE_START_X(d->GetAccountTable().bEmpire);
			pos.y = EMPIRE_START_Y(d->GetAccountTable().bEmpire);
		}
		else
		{
			pos.x = pTab->x;
			pos.y = pTab->y;
		}
	}
	pTab->lMapIndex = lMapIndex;

	// Private ∏ ø° ¿÷æ˙¥¬µ•, Private ∏ ¿Ã ªÁ∂Û¡¯ ªÛ≈¬∂Û∏È √‚±∏∑Œ µπæ∆∞°æﬂ «—¥Ÿ.
	// ----
	// ±Ÿµ• √‚±∏∑Œ µπæ∆∞°æﬂ «—¥Ÿ∏Èº≠... ø÷ √‚±∏∞° æ∆¥œ∂Û private map ªÛø° ¥Î¿¿µ«¥¬ pulic map¿« ¿ßƒ°∏¶ √£≥ƒ∞Ì...
	// ø™ªÁ∏¶ ∏∏£¥œ... ∂« «œµÂƒ⁄µ˘ «—¥Ÿ.
	// æ∆±Õµø±º¿Ã∏È, √‚±∏∑Œ...
	// by rtsummit
	if (!SECTREE_MANAGER::instance().GetValidLocation(pTab->lMapIndex, pTab->x, pTab->y, lMapIndex, pos, d->GetEmpire()))
	{
		sys_err("InputDB::PlayerLoad : cannot find valid location %d x %d (name: %s)", pTab->x, pTab->y, pTab->name);
#ifdef ENABLE_GOHOME_IF_MAP_NOT_EXIST
		lMapIndex = EMPIRE_START_MAP(d->GetAccountTable().bEmpire);
		pos.x = EMPIRE_START_X(d->GetAccountTable().bEmpire);
		pos.y = EMPIRE_START_Y(d->GetAccountTable().bEmpire);
#else
		d->SetPhase(PHASE_CLOSE);
		return;
#endif
	}

	pTab->x = pos.x;
	pTab->y = pos.y;
	pTab->lMapIndex = lMapIndex;

	if (d->GetCharacter() || d->IsPhase(PHASE_GAME))
	{
		LPCHARACTER p = d->GetCharacter();
		sys_err("login state already has main state (character %s %p)", p->GetName(), get_pointer(p));
		return;
	}

	if (nullptr != CHARACTER_MANAGER::Instance().FindPC(pTab->name))
	{
		sys_err("InputDB: PlayerLoad : %s already exist in game", pTab->name);
		return;
	}

	LPCHARACTER ch = CHARACTER_MANAGER::instance().CreateCharacter(pTab->name, pTab->id);

	ch->BindDesc(d);
	ch->SetPlayerProto(pTab);
	ch->SetEmpire(d->GetEmpire());

	d->BindCharacter(ch);

    // Phase 7: create parallel ECS entity for this player
    {
        entt::entity ecs_e = EntityFactory::CreatePC(
            g_registry,
            *pTab,
            d,
            ch->GetLegacyVID());
        d->SetEntity(ecs_e);
        sys_log(0, "ECS: PC entity created VID=%u pid=%u",
                ch->GetLegacyVID(), ch->GetPlayerID());


        // Phase 7: sync ECS vital components from DB result
        if (g_registry.valid(ecs_e)) {
            auto& h = g_registry.get_or_emplace<ecs::Health>(ecs_e);
            h.current = ch->GetHP();
            h.max     = ch->GetMaxHP();

            auto& m = g_registry.get_or_emplace<ecs::Mana>(ecs_e);
            m.current = ch->GetSP();
            m.max     = ch->GetMaxSP();

            auto& lv = g_registry.get_or_emplace<ecs::LevelComponent>(ecs_e);
            lv.value = ch->GetLevel();

            auto& exp = g_registry.get_or_emplace<ecs::Experience>(ecs_e);
            exp.current = ch->GetExp();
            exp.next    = ch->GetNextExp();

            auto& gold = g_registry.get_or_emplace<ecs::GoldAmount>(ecs_e);
            gold.amount = ch->GetGold();
        }
    }

	{
		// P2P Login
		TPacketGGLogin p;

		p.bHeader = HEADER_GG_LOGIN;
		strlcpy(p.szName, ch->GetName(), sizeof(p.szName));
		p.dwPID = ch->GetPlayerID();
		p.bEmpire = ch->GetEmpire();
		p.lMapIndex = SECTREE_MANAGER::instance().GetMapIndex(ch->GetX(), ch->GetY());
		p.bChannel = g_bChannel;

		P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGLogin));

		char buf[51];

		snprintf(buf, sizeof(buf), "%s %lld %d %d %u",

				inet_ntoa(ch->GetDesc()->GetAddr().sin_addr), ch->GetGold(), g_bChannel, ch->GetMapIndex(), ch->GetAlignment());
		LogManager::instance().CharLog(ch, 0, "LOGIN", buf);

#ifdef ENABLE_PCBANG_FEATURE // @warme006
		{
			LogManager::instance().LoginLog(true,
					ch->GetDesc()->GetAccountTable().id, ch->GetPlayerID(), ch->GetLevel(), ch->GetJob(), ch->GetRealPoint(POINT_PLAYTIME));

			if (0)
				ch->SetPCBang(CPCBangManager::instance().IsPCBangIP(ch->GetDesc()->GetHostName()));
		}
#endif
	}

	d->SetPhase(PHASE_LOADING);
	ch->MainCharacterPacket();

	int32_t lPublicMapIndex = lMapIndex >= 10000 ? lMapIndex / 10000 : lMapIndex;
	//if (!map_allow_find(lMapIndex >= 10000 ? lMapIndex / 10000 : lMapIndex) || !CheckEmpire(ch, lMapIndex))
	if (!map_allow_find(lPublicMapIndex))
	{
		sys_err("InputDB::PlayerLoad : entering %d map is not allowed here (name: %s, empire %u)",
				lMapIndex, pTab->name, d->GetEmpire());

		ch->SetWarpLocation(EMPIRE_START_MAP(d->GetEmpire()),
				EMPIRE_START_X(d->GetEmpire()) / 100,
				EMPIRE_START_Y(d->GetEmpire()) / 100);

		d->SetPhase(PHASE_CLOSE);
		return;
	}

	quest::CQuestManager::instance().BroadcastEventFlagOnLogin(ch);

	for (int i = 0; i < QUICKSLOT_MAX_NUM; ++i)
		ch->SetQuickslot(i, pTab->quickslot[i]);

	ch->PointsPacket();
	ch->SkillLevelPacket();

	sys_log(0, "InputDB: player_load %s %dx%dx%d LEVEL %d MOV_SPEED %d JOB %d ATG %d DFG %d GMLv %d",
			pTab->name,
			ch->GetX(), ch->GetY(), ch->GetZ(),
			ch->GetLevel(),
			ch->GetPoint(POINT_MOV_SPEED),
			ch->GetJob(),
			ch->GetPoint(POINT_ATT_GRADE),
			ch->GetPoint(POINT_DEF_GRADE),
			ch->GetGMLevel());

	ch->QuerySafeboxSize();
	ch->QueryMountInventory();
}

void CInputDB::Boot(const char* data)
{
	signal_timer_disable();

	// ∆–≈∂ ªÁ¿Ã¡Ó √º≈©
	uint32_t dwPacketSize = decode_4bytes(data);
	data += 4;

	// ∆–≈∂ πˆ¿¸ √º≈©
	uint8_t bVersion = decode_byte(data);
	data += 1;

	sys_log(0, "BOOT: PACKET: %d", dwPacketSize);
	sys_log(0, "BOOT: VERSION: %d", bVersion);
	if (bVersion != 6)
	{
		sys_err("boot version error");
		thecore_shutdown();
	}

	sys_log(0, "sizeof(TMobTable) = %d", sizeof(TMobTable));
	sys_log(0, "sizeof(TItemTable) = %d", sizeof(TItemTable));
	sys_log(0, "sizeof(TShopTable) = %d", sizeof(TShopTable));
	sys_log(0, "sizeof(TSkillTable) = %d", sizeof(TSkillTable));
	sys_log(0, "sizeof(TRefineTable) = %d", sizeof(TRefineTable));
	sys_log(0, "sizeof(TItemAttrTable) = %d", sizeof(TItemAttrTable));
	sys_log(0, "sizeof(TItemRareTable) = %d", sizeof(TItemAttrTable));
	sys_log(0, "sizeof(TBanwordTable) = %d", sizeof(TBanwordTable));
	sys_log(0, "sizeof(TLand) = %d", sizeof(building::TLand));
	sys_log(0, "sizeof(TObjectProto) = %d", sizeof(building::TObjectProto));
	sys_log(0, "sizeof(TObject) = %d", sizeof(building::TObject));
	//ADMIN_MANAGER
	sys_log(0, "sizeof(TAdminManager) = %d", sizeof (TAdminInfo) );
	//END_ADMIN_MANAGER

	uint16_t size;

	/*
	 * MOB
	 */

	if (decode_2bytes(data)!=sizeof(TMobTable))
	{
		sys_err("mob table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	sys_log(0, "BOOT: MOB: %d", size);

	if (size)
	{
		CMobManager::instance().Initialize((TMobTable *) data, size);
		data += size * sizeof(TMobTable);
	}

	/*
	 * ITEM
	 */

	if (decode_2bytes(data) != sizeof(TItemTable))
	{
		sys_err("item table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	sys_log(0, "BOOT: ITEM: %d", size);


	if (size)
	{
		ITEM_MANAGER::instance().Initialize((TItemTable *) data, size);
		data += size * sizeof(TItemTable);
	}

	/*
	 * SHOP
	 */

	if (decode_2bytes(data) != sizeof(TShopTable))
	{
		sys_err("shop table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	sys_log(0, "BOOT: SHOP: %d", size);


	if (size)
	{
		if (!CShopManager::instance().Initialize((TShopTable *) data, size))
		{
			sys_err("shop table Initialize error");
			thecore_shutdown();
			return;
		}
		data += size * sizeof(TShopTable);
	}

	/*
	 * SKILL
	 */

	if (decode_2bytes(data) != sizeof(TSkillTable))
	{
		sys_err("skill table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	sys_log(0, "BOOT: SKILL: %d", size);

	if (size)
	{
		if (!CSkillManager::instance().Initialize((TSkillTable *) data, size))
		{
			sys_err("cannot initialize skill table");
			thecore_shutdown();
			return;
		}

		data += size * sizeof(TSkillTable);
	}


	/*
	 * REFINE RECIPE
	 */
	if (decode_2bytes(data) != sizeof(TRefineTable))
	{
		sys_err("refine table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	sys_log(0, "BOOT: REFINE: %d", size);

	if (size)
	{
		CRefineManager::instance().Initialize((TRefineTable*) data, size);
		data += size * sizeof(TRefineTable);
	}

	/*
	 * ITEM ATTR
	 */
	if (decode_2bytes(data) != sizeof(TItemAttrTable))
	{
		sys_err("item attr table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	sys_log(0, "BOOT: ITEM_ATTR: %d", size);

	if (size)
	{
		TItemAttrTable * p = (TItemAttrTable *) data;

		for (int i = 0; i < size; ++i, ++p)
		{
			if (p->dwApplyIndex >= MAX_APPLY_NUM)
				continue;

			g_map_itemAttr[p->dwApplyIndex] = *p;
			sys_log(0, "ITEM_ATTR[%d]: %s %u", p->dwApplyIndex, p->szApply, p->dwProb);
		}
	}

	data += size * sizeof(TItemAttrTable);


	/*
     * ITEM RARE
     */
	if (decode_2bytes(data) != sizeof(TItemAttrTable))
	{
		sys_err("item rare table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;
	sys_log(0, "BOOT: ITEM_RARE: %d", size);

	if (size)
	{
		TItemAttrTable * p = (TItemAttrTable *) data;

		for (int i = 0; i < size; ++i, ++p)
		{
			if (p->dwApplyIndex >= MAX_APPLY_NUM)
				continue;

			g_map_itemRare[p->dwApplyIndex] = *p;
			sys_log(0, "ITEM_RARE[%d]: %s %u", p->dwApplyIndex, p->szApply, p->dwProb);
		}
	}

	data += size * sizeof(TItemAttrTable);


	/*
	 * BANWORDS
	 */

	if (decode_2bytes(data) != sizeof(TBanwordTable))
	{
		sys_err("ban word table size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;

	CBanwordManager::instance().Initialize((TBanwordTable *) data, size);
	data += size * sizeof(TBanwordTable);

	{
		using namespace building;

		/*
		 * LANDS
		 */

		if (decode_2bytes(data) != sizeof(TLand))
		{
			sys_err("land table size error");
			thecore_shutdown();
			return;
		}
		data += 2;

		size = decode_2bytes(data);
		data += 2;

		TLand * kLand = (TLand *) data;
		data += size * sizeof(TLand);

		for (uint16_t i = 0; i < size; ++i, ++kLand)
			CManager::instance().LoadLand(kLand);

		/*
		 * OBJECT PROTO
		 */

		if (decode_2bytes(data) != sizeof(TObjectProto))
		{
			sys_err("object proto table size error");
			thecore_shutdown();
			return;
		}
		data += 2;

		size = decode_2bytes(data);
		data += 2;

		CManager::instance().LoadObjectProto((TObjectProto *) data, size);
		data += size * sizeof(TObjectProto);

		/*
		 * OBJECT
		 */
		if (decode_2bytes(data) != sizeof(TObject))
		{
			sys_err("object table size error");
			thecore_shutdown();
			return;
		}
		data += 2;

		size = decode_2bytes(data);
		data += 2;

		TObject * kObj = (TObject *) data;
		data += size * sizeof(TObject);

		for (uint16_t i = 0; i < size; ++i, ++kObj)
			CManager::instance().LoadObject(kObj, true);
	}

	set_global_time(*(time_t *) data);
	data += sizeof(time_t);

	if (decode_2bytes(data) != sizeof(TItemIDRangeTable) )
	{
		sys_err("ITEM ID RANGE size error");
		thecore_shutdown();
		return;
	}
	data += 2;

	size = decode_2bytes(data);
	data += 2;

	TItemIDRangeTable* range = (TItemIDRangeTable*) data;
	data += size * sizeof(TItemIDRangeTable);

	TItemIDRangeTable* rangespare = (TItemIDRangeTable*) data;
	data += size * sizeof(TItemIDRangeTable);

	//ADMIN_MANAGER
	//∞¸∏Æ¿⁄ µÓ∑œ
	int ChunkSize = decode_2bytes(data );
	data += 2;
	int HostSize = decode_2bytes(data );
	data += 2;
	sys_log(0, "GM Value Count %d %d", HostSize, ChunkSize  );
	for (int n = 0; n < HostSize; ++n )
	{
		gm_new_host_inert(data );
		sys_log(0, "GM HOST : IP[%s] ", data );
		data += ChunkSize;
	}


	data += 2;
	int adminsize = decode_2bytes(data );
	data += 2;

	for (int n = 0; n < adminsize; ++n )
	{
		tAdminInfo& rAdminInfo = *(tAdminInfo*)data;

		gm_new_insert(rAdminInfo );

		data += sizeof(rAdminInfo );
	}

	//END_ADMIN_MANAGER

	uint16_t endCheck=decode_2bytes(data);
	if (endCheck != 0xffff)
	{
		sys_err("boot packet end check error [%x]!=0xffff", endCheck);
		thecore_shutdown();
		return;
	}
	else
		sys_log(0, "boot packet end check ok [%x]==0xffff", endCheck );
	data +=2;

	if (!ITEM_MANAGER::instance().SetMaxItemID(*range))
	{
		sys_err("not enough item id contact your administrator!");
		thecore_shutdown();
		return;
	}

	if (!ITEM_MANAGER::instance().SetMaxSpareItemID(*rangespare))
	{
		sys_err("not enough item id for spare contact your administrator!");
		thecore_shutdown();
		return;
	}



	// LOCALE_SERVICE
	const int FILE_NAME_LEN = 256;
	char szCommonDropItemFileName[FILE_NAME_LEN];
	char szETCDropItemFileName[FILE_NAME_LEN];
	char szMOBDropItemFileName[FILE_NAME_LEN];
	char szDropItemGroupFileName[FILE_NAME_LEN];
	char szSpecialItemGroupFileName[FILE_NAME_LEN];
	char szMapIndexFileName[FILE_NAME_LEN];
	char szItemVnumMaskTableFileName[FILE_NAME_LEN];
	char szDragonSoulTableFileName[FILE_NAME_LEN];

	snprintf(szCommonDropItemFileName, sizeof(szCommonDropItemFileName),
			"%s/common_drop_item.txt", LocaleService_GetBasePath().c_str());
	snprintf(szETCDropItemFileName, sizeof(szETCDropItemFileName),
			"%s/etc_drop_item.txt", LocaleService_GetBasePath().c_str());
	snprintf(szMOBDropItemFileName, sizeof(szMOBDropItemFileName),
			"%s/mob_drop_item.txt", LocaleService_GetBasePath().c_str());
	snprintf(szSpecialItemGroupFileName, sizeof(szSpecialItemGroupFileName),
			"%s/special_item_group.txt", LocaleService_GetBasePath().c_str());
	snprintf(szDropItemGroupFileName, sizeof(szDropItemGroupFileName),
			"%s/drop_item_group.txt", LocaleService_GetBasePath().c_str());
	snprintf(szMapIndexFileName, sizeof(szMapIndexFileName),
			"%s/index", LocaleService_GetMapPath().c_str());
	snprintf(szItemVnumMaskTableFileName, sizeof(szItemVnumMaskTableFileName),
			"%s/ori_to_new_table.txt", LocaleService_GetBasePath().c_str());
	snprintf(szDragonSoulTableFileName, sizeof(szDragonSoulTableFileName),
			"%s/dragon_soul_table.txt", LocaleService_GetBasePath().c_str());

	sys_log(0, "Initializing Informations of Cube System");
	Cube_InformationInitialize();

	sys_log(0, "LoadLocaleFile: CommonDropItem: %s", szCommonDropItemFileName);
	if (!ITEM_MANAGER::instance().ReadCommonDropItemFile(szCommonDropItemFileName))
	{
		sys_err("cannot load CommonDropItem: %s", szCommonDropItemFileName);
		thecore_shutdown();
		return;
	}

	sys_log(0, "LoadLocaleFile: ETCDropItem: %s", szETCDropItemFileName);
	if (!ITEM_MANAGER::instance().ReadEtcDropItemFile(szETCDropItemFileName))
	{
		sys_err("cannot load ETCDropItem: %s", szETCDropItemFileName);
		thecore_shutdown();
		return;
	}

	sys_log(0, "LoadLocaleFile: DropItemGroup: %s", szDropItemGroupFileName);
	if (!ITEM_MANAGER::instance().ReadDropItemGroup(szDropItemGroupFileName))
	{
		sys_err("cannot load DropItemGroup: %s", szDropItemGroupFileName);
		thecore_shutdown();
		return;
	}

	sys_log(0, "LoadLocaleFile: SpecialItemGroup: %s", szSpecialItemGroupFileName);
	if (!ITEM_MANAGER::instance().ReadSpecialDropItemFile(szSpecialItemGroupFileName))
	{
		sys_err("cannot load SpecialItemGroup: %s", szSpecialItemGroupFileName);
		thecore_shutdown();
		return;
	}

	sys_log(0, "LoadLocaleFile: ItemVnumMaskTable : %s", szItemVnumMaskTableFileName);
	if (!ITEM_MANAGER::instance().ReadItemVnumMaskTable(szItemVnumMaskTableFileName))
	{
		sys_log(0, "Could not open MaskItemTable");
	}

	sys_log(0, "LoadLocaleFile: MOBDropItemFile: %s", szMOBDropItemFileName);
	if (!ITEM_MANAGER::instance().ReadMonsterDropItemGroup(szMOBDropItemFileName))
	{
		sys_err("cannot load MOBDropItemFile: %s", szMOBDropItemFileName);
		thecore_shutdown();
		return;
	}

	sys_log(0, "LoadLocaleFile: MapIndex: %s", szMapIndexFileName);
	if (!SECTREE_MANAGER::instance().Build(szMapIndexFileName, LocaleService_GetMapPath().c_str()))
	{
		sys_err("cannot load MapIndex: %s", szMapIndexFileName);
		thecore_shutdown();
		return;
	}

	sys_log(0, "LoadLocaleFile: DragonSoulTable: %s", szDragonSoulTableFileName);
	if (!DSManager::instance().ReadDragonSoulTableFile(szDragonSoulTableFileName))
	{
		sys_err("cannot load DragonSoulTable: %s", szDragonSoulTableFileName);
		//thecore_shutdown();
		//return;
	}

	// END_OF_LOCALE_SERVICE

#ifdef ENABLE_BATTLE_PASS
	sys_log(0, "LoadLocaleFile: BattlePassInfo");
	if (!CBattlePass::instance().ReadBattlePassFile())
	{
		sys_err("Cannot load battle_pass.txt");
	}
#endif

	building::CManager::instance().FinalizeBoot();

	CMotionManager::instance().Build();

	signal_timer_enable(30);

	if (test_server)
	{
		CMobManager::instance().DumpRegenCount("mob_count");
	}

	CPCBangManager::instance().RequestUpdateIPList(0);
}

EVENTINFO(quest_login_event_info)
{
	uint32_t dwPID;

	quest_login_event_info()
	: dwPID( 0 )
	{
	}
};

EVENTFUNC(quest_login_event)
{
	quest_login_event_info* info = dynamic_cast<quest_login_event_info*>( event->info );

	if ( info == nullptr)
	{
		sys_err( "quest_login_event> <Factor> Null pointer" );
		return 0;
	}

	uint32_t dwPID = info->dwPID;

	LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(dwPID);

	if (!ch)
		return 0;

	LPDESC d = ch->GetDesc();

	if (!d)
		return 0;

	if (d->IsPhase(PHASE_HANDSHAKE) ||
		d->IsPhase(PHASE_LOGIN) ||
		d->IsPhase(PHASE_SELECT) ||
		d->IsPhase(PHASE_DEAD) ||
		d->IsPhase(PHASE_LOADING))
	{
		return PASSES_PER_SEC(1);
	}
	else if (d->IsPhase(PHASE_CLOSE))
	{
		return 0;
	}
	else if (d->IsPhase(PHASE_GAME))
	{
		sys_log(0, "QUEST_LOAD: Login pc %d by event", ch->GetPlayerID());
		quest::CQuestManager::instance().Login(ch->GetPlayerID());
		return 0;
	}
	else
	{
		sys_err(0, "input_db.cpp:quest_login_event INVALID PHASE pid %d", ch->GetPlayerID());
		return 0;
	}
}

void CInputDB::QuestLoad(LPDESC d, const char * c_pData)
{
	if (nullptr == d)
		return;

	LPCHARACTER ch = d->GetCharacter();

	if (nullptr == ch)
		return;

	const uint32_t dwCount = decode_4bytes(c_pData);

	const TQuestTable* pQuestTable = reinterpret_cast<const TQuestTable*>(c_pData+4);

	if (nullptr != pQuestTable)
	{
		if (dwCount != 0)
		{
			if (ch->GetPlayerID() != pQuestTable[0].dwPID)
			{
				sys_err("PID differs %u %u", ch->GetPlayerID(), pQuestTable[0].dwPID);
				return;
			}
		}

		sys_log(0, "QUEST_LOAD: count %d", dwCount);

		quest::PC * pkPC = quest::CQuestManager::instance().GetPCForce(ch->GetPlayerID());

		if (!pkPC)
		{
			sys_err("null quest::PC with id %u", pQuestTable[0].dwPID);
			return;
		}

		if (pkPC->IsLoaded())
			return;

		for (unsigned int i = 0; i < dwCount; ++i)
		{
			std::string st(pQuestTable[i].szName);

			st += ".";
			st += pQuestTable[i].szState;

			sys_log(0,  "            %s %d", st.c_str(), pQuestTable[i].lValue);
#ifdef ENABLE_QUEST_SYSTEM_BUGFIXES
			int val = pQuestTable[i].lValue;
			bool skipSave = true; // load-n·l ne spameld a DB-t
			

				               // __status: ha a DB-ben rÈgi state CRC maradt, de a Lua-ban m·r nincs,
				               // akkor reset start-ra, k¸lˆnben "meghal" a quest (NPC nem reag·l, stb.)
				if (!strcmp(pQuestTable[i].szState, "__status"))
				 {
				const char* stateName = quest::CQuestManager::instance().GetQuestStateName(pQuestTable[i].szName, val);
				if (!stateName || !*stateName)
					 {
					const int startIdx = quest::CQuestManager::instance().GetQuestStateIndex(pQuestTable[i].szName, "start");
					sys_err("QUEST __status invalid: pid=%u quest=%s val=%d -> start=%d",
						+pQuestTable[i].dwPID, pQuestTable[i].szName, val, startIdx);
					val = startIdx ? startIdx : 0; // 0 -> DeleteFlag
					skipSave = false; // ezt ments¸k is vissza, hogy a DB kijavuljon
					}
				 }
			
				pkPC->SetFlag(st, val, skipSave);
#else
			pkPC->SetFlag(st, pQuestTable[i].lValue, false);
#endif
		}

		pkPC->SetLoaded();
		pkPC->Build();

		if (ch->GetDesc()->IsPhase(PHASE_GAME))
		{
			sys_log(0, "QUEST_LOAD: Login pc %d", pQuestTable[0].dwPID);
			quest::CQuestManager::instance().Login(pQuestTable[0].dwPID);
		}
		else
		{
			quest_login_event_info* info = AllocEventInfo<quest_login_event_info>();
			info->dwPID = ch->GetPlayerID();

			event_create(quest_login_event, info, PASSES_PER_SEC(1));
		}
	}
}

void CInputDB::SafeboxLoad(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	TSafeboxTable * p = (TSafeboxTable *) c_pData;

	if (d->GetAccountTable().id != p->dwID)
	{
		sys_err("SafeboxLoad: safebox has different id %u != %u", d->GetAccountTable().id, p->dwID);
		return;
	}

	if (!d->GetCharacter())
		return;

	uint8_t bSize = 1;

	LPCHARACTER ch = d->GetCharacter();

	//PREVENT_TRADE_WINDOW
	if (ch->GetShopOwner() || ch->GetExchange() || ch->GetMyShop() || ch->IsCubeOpen() )
	{
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 296, "");
#endif
		d->GetCharacter()->CancelSafeboxLoad();
		return;
	}
	
#ifdef __ATTR_TRANSFER_SYSTEM__
	if (ch->IsAttrTransferOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ch->ChatPacketNew(CHAT_TYPE_INFO, 296, "");
#endif
		d->GetCharacter()->CancelSafeboxLoad();
		return;
	}
#endif
	//END_PREVENT_TRADE_WINDOW

	// ADD_PREMIUM
	if (d->GetCharacter()->GetPremiumRemainSeconds(PREMIUM_SAFEBOX) > 0 || d->GetCharacter()->IsEquipUniqueGroup(UNIQUE_GROUP_LARGE_SAFEBOX))
		bSize = 3;
	// END_OF_ADD_PREMIUM

	//if (d->GetCharacter()->IsEquipUniqueItem(UNIQUE_ITEM_SAFEBOX_EXPAND))
	//bSize = 3; // √¢∞Ì»Æ¿Â±«

	//d->GetCharacter()->LoadSafebox(p->bSize * SAFEBOX_PAGE_SIZE, p->dwGold, p->wItemCount, (TPlayerItem *) (c_pData + sizeof(TSafeboxTable)));
	d->GetCharacter()->LoadSafebox(bSize * SAFEBOX_PAGE_SIZE, p->dwGold, p->wItemCount, (TPlayerItem *) (c_pData + sizeof(TSafeboxTable)));
}

void CInputDB::SafeboxChangeSize(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	uint8_t bSize = *(uint8_t *) c_pData;

	if (!d->GetCharacter())
		return;

	d->GetCharacter()->ChangeSafeboxSize(bSize);
}

//
// @version	05/06/20 Bang2ni - ReqSafeboxLoad ¿« √Îº“
//
void CInputDB::SafeboxWrongPassword(LPDESC d)
{
	if (!d)
		return;

	if (!d->GetCharacter())
		return;

	TPacketCGSafeboxWrongPassword p;
	p.bHeader = HEADER_GC_SAFEBOX_WRONG_PASSWORD;
	d->Packet(&p, sizeof(p));

	d->GetCharacter()->CancelSafeboxLoad();
}

void CInputDB::SafeboxChangePasswordAnswer(LPDESC d, const char* c_pData)
{
	if (!d)
		return;

	if (!d->GetCharacter())
		return;

#ifdef TEXTS_IMPROVEMENT
	TSafeboxChangePasswordPacketAnswer* p = (TSafeboxChangePasswordPacketAnswer*) c_pData;
	if (p->flag) {
		d->GetCharacter()->ChatPacketNew(CHAT_TYPE_INFO, 187, "");
	}
	else {
		d->GetCharacter()->ChatPacketNew(CHAT_TYPE_INFO, 186, "");
	}
#endif
}

void CInputDB::MallLoad(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	TSafeboxTable * p = (TSafeboxTable *) c_pData;

	if (d->GetAccountTable().id != p->dwID)
	{
		sys_err("safebox has different id %u != %u", d->GetAccountTable().id, p->dwID);
		return;
	}

	if (!d->GetCharacter())
		return;

	d->GetCharacter()->LoadMall(p->wItemCount, (TPlayerItem *) (c_pData + sizeof(TSafeboxTable)));
}

void CInputDB::LoginAlready(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	// INTERNATIONAL_VERSION ¿ÃπÃ ¡¢º”¡ﬂ¿Ã∏È ¡¢º” ≤˜¿Ω
	{
		TPacketDGLoginAlready * p = (TPacketDGLoginAlready *) c_pData;

		LPDESC d2 = DESC_MANAGER::instance().FindByLoginName(p->szLogin);

		if (d2)
			d2->DisconnectOfSameLogin();
		else
		{
			TPacketGGDisconnect pgg;

			pgg.bHeader = HEADER_GG_DISCONNECT;
			strlcpy(pgg.szLogin, p->szLogin, sizeof(pgg.szLogin));

			P2P_MANAGER::instance().Send(&pgg, sizeof(TPacketGGDisconnect));
		}
	}
	// END_OF_INTERNATIONAL_VERSION

	LoginFailure(d, "ALREADY");
}

void CInputDB::EmpireSelect(LPDESC d, const char * c_pData)
{
	sys_log(0, "EmpireSelect %p", get_pointer(d));

	if (!d)
		return;

	TAccountTable & rTable = d->GetAccountTable();
	rTable.bEmpire = *(uint8_t *)c_pData;

	TPacketGCEmpire pe;
	pe.bHeader = HEADER_GC_EMPIRE;
	pe.bEmpire = rTable.bEmpire;
	d->Packet(&pe, sizeof(pe));

	for (int i = 0; i < PLAYER_PER_ACCOUNT; ++i)
		if (rTable.players[i].dwID)
		{
			rTable.players[i].x = EMPIRE_START_X(rTable.bEmpire);
			rTable.players[i].y = EMPIRE_START_Y(rTable.bEmpire);
		}

	GetServerLocation(d->GetAccountTable(), rTable.bEmpire);

	d->SendLoginSuccessPacket();
}

void CInputDB::MapLocations(const char * c_pData)
{
	uint8_t bCount = *(uint8_t *) (c_pData++);

	sys_log(0, "InputDB::MapLocations %d", bCount);

	TMapLocation * pLoc = (TMapLocation *) c_pData;

	while (bCount--)
	{
		for (int i = 0; i < 32; ++i)
		{
			if (0 == pLoc->alMaps[i])
				break;

			CMapLocation::instance().Insert(pLoc->alMaps[i], pLoc->szHost, pLoc->wPort
#ifdef ENABLE_GENERAL_CH
			                                , pLoc->bChannel
#endif
			);
		}

		pLoc++;
	}
}

void CInputDB::P2P(const char * c_pData)
{
	extern LPFDWATCH main_fdw;

	TPacketDGP2P * p = (TPacketDGP2P *) c_pData;

	P2P_MANAGER& mgr = P2P_MANAGER::instance();

	if (false == DESC_MANAGER::instance().IsP2PDescExist(p->szHost, p->wPort))
	{
		sys_log(0, "InputDB:P2P %s:%u", p->szHost, p->wPort);
	    LPCLIENT_DESC pkDesc = DESC_MANAGER::instance().CreateConnectionDesc(
		    main_fdw, p->szHost, p->wPort, PHASE_P2P, false);
		mgr.RegisterConnector(pkDesc);
		pkDesc->SetP2P(p->szHost, p->wPort, p->bChannel);
	}
}

void CInputDB::GuildLoad(const char * c_pData)
{
	CGuildManager::instance().LoadGuild(*(uint32_t *) c_pData);
}

void CInputDB::GuildSkillUpdate(const char* c_pData)
{
	TPacketGuildSkillUpdate * p = (TPacketGuildSkillUpdate *) c_pData;

	CGuild * g = CGuildManager::instance().TouchGuild(p->guild_id);

	if (g)
	{
		g->UpdateSkill(p->skill_point, p->skill_levels);
		g->GuildPointChange(POINT_SP, p->amount, p->save?true:false);
	}
}

void CInputDB::GuildWar(const char* c_pData)
{
	TPacketGuildWar * p = (TPacketGuildWar*) c_pData;

	sys_log(0, "InputDB::GuildWar %u %u state %d", p->dwGuildFrom, p->dwGuildTo, p->bWar);

	switch (p->bWar)
	{
		case GUILD_WAR_SEND_DECLARE:
		case GUILD_WAR_RECV_DECLARE:
			CGuildManager::instance().DeclareWar(p->dwGuildFrom, p->dwGuildTo, p->bType);
			break;

		case GUILD_WAR_REFUSE:
			CGuildManager::instance().RefuseWar(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_WAIT_START:
			CGuildManager::instance().WaitStartWar(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_CANCEL:
			CGuildManager::instance().CancelWar(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_ON_WAR:
			CGuildManager::instance().StartWar(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_END:
			CGuildManager::instance().EndWar(p->dwGuildFrom, p->dwGuildTo);
			break;

		case GUILD_WAR_OVER:
			CGuildManager::instance().WarOver(p->dwGuildFrom, p->dwGuildTo, p->bType);
			break;

		case GUILD_WAR_RESERVE:
			CGuildManager::instance().ReserveWar(p->dwGuildFrom, p->dwGuildTo, p->bType);
			break;

		default:
			sys_err("Unknown guild war state");
			break;
	}
}

#ifdef ADVANCED_GUILD_INFO
void CInputDB::GuildResetStats(const char* c_pData)
{
	CGuildManager::instance().ResetStatsToAll();
}
#endif

void CInputDB::GuildWarScore(const char* c_pData)
{
	TPacketGuildWarScore* p = (TPacketGuildWarScore*) c_pData;
	CGuild * g = CGuildManager::instance().TouchGuild(p->dwGuildGainPoint);
	g->SetWarScoreAgainstTo(p->dwGuildOpponent, p->lScore);
}

void CInputDB::GuildSkillRecharge()
{
	CGuildManager::instance().SkillRecharge();
}

void CInputDB::GuildExpUpdate(const char* c_pData)
{
	TPacketGuildSkillUpdate * p = (TPacketGuildSkillUpdate *) c_pData;
	sys_log(1, "GuildExpUpdate %d", p->amount);

	CGuild * g = CGuildManager::instance().TouchGuild(p->guild_id);

	if (g)
		g->GuildPointChange(POINT_EXP, p->amount);
}

void CInputDB::GuildAddMember(const char* c_pData)
{
	TPacketDGGuildMember * p = (TPacketDGGuildMember *) c_pData;
	CGuild * g = CGuildManager::instance().TouchGuild(p->dwGuild);

	if (g)
		g->AddMember(p);
}

void CInputDB::GuildRemoveMember(const char* c_pData)
{
	TPacketGuild* p=(TPacketGuild*)c_pData;
	CGuild* g = CGuildManager::instance().TouchGuild(p->dwGuild);

	if (g)
		g->RemoveMember(p->dwInfo);
}

void CInputDB::GuildChangeGrade(const char* c_pData)
{
	TPacketGuild* p=(TPacketGuild*)c_pData;
	CGuild* g = CGuildManager::instance().TouchGuild(p->dwGuild);

	if (g)
		g->P2PChangeGrade((uint8_t)p->dwInfo);
}

void CInputDB::GuildChangeMemberData(const char* c_pData)
{
	sys_log(0, "Recv GuildChangeMemberData");
	TPacketGuildChangeMemberData * p = (TPacketGuildChangeMemberData *) c_pData;
	CGuild * g = CGuildManager::instance().TouchGuild(p->guild_id);

	if (g)
		g->ChangeMemberData(p->pid, p->offer, p->level, p->grade);
}

void CInputDB::GuildDisband(const char* c_pData)
{
	TPacketGuild * p = (TPacketGuild*) c_pData;
	CGuildManager::instance().DisbandGuild(p->dwGuild);
}

void CInputDB::GuildLadder(const char* c_pData)
{
	TPacketGuildLadder* p = (TPacketGuildLadder*) c_pData;
	sys_log(0, "Recv GuildLadder %u %d / w %d d %d l %d", p->dwGuild, p->lLadderPoint, p->lWin, p->lDraw, p->lLoss);
	CGuild * g = CGuildManager::instance().TouchGuild(p->dwGuild);

	g->SetLadderPoint(p->lLadderPoint);
	g->SetWarData(p->lWin, p->lDraw, p->lLoss);
}

#ifdef __SKILL_COLOR_SYSTEM__
void CInputDB::SkillColorLoad(LPDESC d, const char * c_pData)
{
	LPCHARACTER ch;

	if (!d || !(ch = d->GetCharacter()))
		return;

	ch->SetSkillColor((uint32_t*)c_pData);
}
#endif

void CInputDB::ItemLoad(LPDESC d, const char * c_pData)
{
	LPCHARACTER ch;

	if (!d || !(ch = d->GetCharacter()))
		return;

	if (ch->IsItemLoaded())
		return;

	uint32_t dwCount = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	sys_log(0, "ITEM_LOAD: COUNT %s %u", ch->GetName(), dwCount);

	std::vector<LPITEM> v;
	TPlayerItem * p = (TPlayerItem *) c_pData;

	for (uint32_t i = 0; i < dwCount; ++i, ++p)
	{
		if (LPITEM staleItem = ITEM_MANAGER::instance().Find(p->id))
		{
			const LPCHARACTER staleOwner = staleItem->GetOwner();
			const bool samePlayer =
				(staleOwner && staleOwner->GetPlayerID() == ch->GetPlayerID()) ||
				(staleItem->GetLastOwnerPID() == ch->GetPlayerID());

#ifdef ENABLE_EXTRA_INVENTORY
			const bool extraInventoryWindow = (p->window == EXTRA_INVENTORY);
#else
			const bool extraInventoryWindow = false;
#endif

			if (samePlayer || extraInventoryWindow)
			{
				sys_err("CInputDB::ItemLoad: purging stale duplicate item id=%u owner_pid=%u window=%u",
					p->id,
					ch->GetPlayerID(),
					p->window);
				staleItem->SetSkipSave(true);
				M2_DESTROY_ITEM(staleItem);
			}
		}

		LPITEM item = ITEM_MANAGER::instance().CreateItem(p->vnum, p->count, p->id);

		if (!item)
		{
			sys_err("cannot create item by vnum %u (name %s id %u)", p->vnum, ch->GetName(), p->id);
			continue;
		}

		item->SetSkipSave(true);
		item->SetSockets(p->alSockets);
		item->SetAttributes(p->aAttr);
#ifdef ATTR_LOCK
		item->SetLockedAttr(p->lockedattr);
#endif
#ifdef ENABLE_BELT_INVENTORY_EX
		if (p->window == BELT_INVENTORY)
		{
			p->window = INVENTORY;
			p->pos = p->pos + BELT_INVENTORY_SLOT_START;
		}
#endif

		if ((p->window == INVENTORY && ch->GetInventoryItem(p->pos)) ||
				(p->window == EQUIPMENT && ch->GetWear(p->pos)))
		{
			sys_log(0, "ITEM_RESTORE: %s %s", ch->GetName(), item->GetName());
			v.push_back(item);
		}
		else
		{
			switch (p->window)
			{
				case INVENTORY:
				case DRAGON_SOUL_INVENTORY:
#ifdef ENABLE_EXTRA_INVENTORY
				case EXTRA_INVENTORY:
#endif
#ifdef ENABLE_SWITCHBOT
				case SWITCHBOT:
#endif
#ifdef ENABLE_MOUNT_INVENTORY_FIX_RAZOR93_off
				 case MOUNT_INVENTORY:
					               // safety: never load these into CHARACTER inventory arrays
						v.push_back(item);
					break;
#else
				case MOUNT_INVENTORY:
#ifdef __HIGHLIGHT_SYSTEM__
					item->AddToCharacter(ch, TItemPos(p->window, p->pos), false);
#else
					item->AddToCharacter(ch, TItemPos(p->window, p->pos));
#endif
					break;
#endif
				case EQUIPMENT:
					if (item->CheckItemUseLevel(ch->GetLevel()) == true )
					{
						if (item->EquipTo(ch, p->pos) == false )
						{
							v.push_back(item);
						}
					}
					else
					{
						v.push_back(item);
					}
					break;
			}
		}

		if (false == item->OnAfterCreatedItem())
			sys_err("Failed to call ITEM::OnAfterCreatedItem (vnum: %d, id: %d)", item->GetVnum(), item->GetID());

		item->SetSkipSave(false);
	}

	auto it = v.begin();

	while (it != v.end())
	{
		LPITEM item = *(it++);

		int pos = ch->GetEmptyInventory(item->GetSize());

		if (pos < 0)
		{
			PIXEL_POSITION coord;
			coord.x = ch->GetX();
			coord.y = ch->GetY();

			item->AddToGround(ch->GetMapIndex(), coord);
			item->SetOwnership(ch, 180);
			item->StartDestroyEvent();
		}
		else
#ifdef __HIGHLIGHT_SYSTEM__
			item->AddToCharacter(ch, TItemPos(INVENTORY, pos), false);
#else
			item->AddToCharacter(ch, TItemPos(INVENTORY, pos));
#endif
	}


	ch->CheckMaximumPoints();
	ch->PointsPacket();

	ch->SetItemLoaded();
}

#ifdef ENABLE_BATTLE_PASS
void CInputDB::BattlePassLoad(LPDESC d, const char * c_pData)
{
	//sys_err("BattlePassLoad");
	if (!d || !d->GetCharacter())
		return;

	LPCHARACTER ch = d->GetCharacter();
	if (!ch)
		return;

	uint32_t dwPID = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	uint32_t dwCount = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	if (ch->GetPlayerID() != dwPID)
		return;

	ch->LoadBattlePass(dwCount, (TPlayerBattlePassMission *)c_pData);
}

void CInputDB::BattlePassLoadRanking(LPDESC d, const char * c_pData)
{
	//sys_err("BattlePassLoadRanking");
	if (!d || !d->GetCharacter())
		return;

	LPCHARACTER ch = d->GetCharacter();
	if (!ch)
		return;

	uint32_t dwPID = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);
	
	uint8_t bIsGlobal = decode_byte(c_pData);
	c_pData += sizeof(uint8_t);

	uint32_t dwCount = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	//sys_err("BattlePassLoadRanking count %d playerid %d", dwCount, dwPID);
	
	if (ch->GetPlayerID() != dwPID)
		return;
	
	if(dwCount)
	{
		std::vector<TBattlePassRanking> sendVector;
		sendVector.resize(dwCount);
		
		TBattlePassRanking* p = (TBattlePassRanking*) c_pData;
		
		for (unsigned int i = 0; i < dwCount; ++i, ++p)
		{
			TBattlePassRanking newRanking;
			newRanking.bPos = p->bPos;
			strlcpy(newRanking.playerName, p->playerName, sizeof(newRanking.playerName));
			newRanking.dwFinishTime = p->dwFinishTime;
			
			sendVector.push_back(newRanking);
		}
		
		if(!sendVector.empty())
		{
			TPacketGCBattlePassRanking packet;
			packet.bHeader = HEADER_GC_BATTLE_PASS_RANKING;
			packet.wSize = sizeof(packet) + sizeof(TBattlePassRanking) * sendVector.size();
			packet.bIsGlobal = bIsGlobal;

			ch->GetDesc()->BufferedPacket(&packet, sizeof(packet));
			ch->GetDesc()->Packet(&sendVector[0], sizeof(TBattlePassRanking) * sendVector.size());
		}
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ch->ChatPacketNew(CHAT_TYPE_INFO, 762, "");
	}
#endif
}
#endif

void CInputDB::AffectLoad(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	if (!d->GetCharacter())
		return;

	LPCHARACTER ch = d->GetCharacter();

	uint32_t dwPID = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	uint32_t dwCount = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	if (ch->GetPlayerID() != dwPID)
		return;

	ch->LoadAffect(dwCount, (TPacketAffectElement *) c_pData);
#ifdef ENABLE_BATTLE_PASS
#ifdef ENABLE_FREE_PASS_RAZOR93
	ch->EnsureFreeBattlePassActive();
	if (!ch->IsLoadedBattlePass())
		ch->LoadBattlePass(0, nullptr);
#endif
#endif


}



void CInputDB::PartyCreate(const char* c_pData)
{
	TPacketPartyCreate* p = (TPacketPartyCreate*) c_pData;
	CPartyManager::instance().P2PCreateParty(p->dwLeaderPID);
}

void CInputDB::PartyDelete(const char* c_pData)
{
	TPacketPartyDelete* p = (TPacketPartyDelete*) c_pData;
	CPartyManager::instance().P2PDeleteParty(p->dwLeaderPID);
}

void CInputDB::PartyAdd(const char* c_pData)
{
	TPacketPartyAdd* p = (TPacketPartyAdd*) c_pData;
	CPartyManager::instance().P2PJoinParty(p->dwLeaderPID, p->dwPID, p->bState);
}

void CInputDB::PartyRemove(const char* c_pData)
{
	TPacketPartyRemove* p = (TPacketPartyRemove*) c_pData;
	CPartyManager::instance().P2PQuitParty(p->dwPID);
}

void CInputDB::PartyStateChange(const char* c_pData)
{
	TPacketPartyStateChange * p = (TPacketPartyStateChange *) c_pData;
	LPPARTY pParty = CPartyManager::instance().P2PCreateParty(p->dwLeaderPID);

	if (!pParty)
		return;

	pParty->SetRole(p->dwPID, p->bRole, p->bFlag);
}

void CInputDB::PartySetMemberLevel(const char* c_pData)
{
	TPacketPartySetMemberLevel* p = (TPacketPartySetMemberLevel*) c_pData;
	LPPARTY pParty = CPartyManager::instance().P2PCreateParty(p->dwLeaderPID);

	if (!pParty)
		return;

	pParty->P2PSetMemberLevel(p->dwPID, p->bLevel);
}

void CInputDB::Time(const char * c_pData)
{
	set_global_time(*(time_t *) c_pData);
}

void CInputDB::ReloadProto(const char * c_pData)
{
	uint16_t wSize;

	/*
	 * Skill
	 */
	wSize = decode_2bytes(c_pData);
	c_pData += sizeof(uint16_t);
	if (wSize) CSkillManager::instance().Initialize((TSkillTable *) c_pData, wSize);
	c_pData += sizeof(TSkillTable) * wSize;

	/*
	 * Banwords
	 */

	wSize = decode_2bytes(c_pData);
	c_pData += sizeof(uint16_t);
	CBanwordManager::instance().Initialize((TBanwordTable *) c_pData, wSize);
	c_pData += sizeof(TBanwordTable) * wSize;

	/*
	 * ITEM
	 */
	wSize = decode_2bytes(c_pData);
	c_pData += 2;
	sys_log(0, "RELOAD: ITEM: %d", wSize);

	if (wSize)
	{
		ITEM_MANAGER::instance().Initialize((TItemTable *) c_pData, wSize);
		c_pData += wSize * sizeof(TItemTable);
	}

	/*
	 * MONSTER
	 */
	wSize = decode_2bytes(c_pData);
	c_pData += 2;
	sys_log(0, "RELOAD: MOB: %d", wSize);

	if (wSize)
	{
		CMobManager::instance().Initialize((TMobTable *) c_pData, wSize);
		c_pData += wSize * sizeof(TMobTable);
	}

	CMotionManager::instance().Build();

	CHARACTER_MANAGER::instance().for_each_pc(std::mem_fn(&CHARACTER::ComputePoints));
}

void CInputDB::GuildSkillUsableChange(const char* c_pData)
{
	TPacketGuildSkillUsableChange* p = (TPacketGuildSkillUsableChange*) c_pData;

	CGuild* g = CGuildManager::instance().TouchGuild(p->dwGuild);

	g->SkillUsableChange(p->dwSkillVnum, p->bUsable?true:false);
}

void CInputDB::AuthLogin(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	uint8_t bResult = *(uint8_t *) c_pData;

	TPacketGCAuthSuccess ptoc;

	ptoc.bHeader = HEADER_GC_AUTH_SUCCESS;

	if (bResult)
	{
		ptoc.dwLoginKey = d->GetLoginKey();
	}
	else
	{
		ptoc.dwLoginKey = 0;
	}

	ptoc.bResult = bResult;

	d->Packet(&ptoc, sizeof(TPacketGCAuthSuccess));

	sys_log(0, "AuthLogin result %u key %u", bResult, d->GetLoginKey());
}

void CInputDB::ChangeEmpirePriv(const char* c_pData)
{
	TPacketDGChangeEmpirePriv* p = (TPacketDGChangeEmpirePriv*) c_pData;

	// ADD_EMPIRE_PRIV_TIME
	CPrivManager::instance().GiveEmpirePriv(p->empire, p->type, p->value, p->bLog, p->end_time_sec);
	// END_OF_ADD_EMPIRE_PRIV_TIME
}

/**
 * @version 05/06/08	Bang2ni - ¡ˆº”Ω√∞£ √ﬂ∞°
 */
void CInputDB::ChangeGuildPriv(const char* c_pData)
{
	TPacketDGChangeGuildPriv* p = (TPacketDGChangeGuildPriv*) c_pData;

	// ADD_GUILD_PRIV_TIME
	CPrivManager::instance().GiveGuildPriv(p->guild_id, p->type, p->value, p->bLog, p->end_time_sec);
	// END_OF_ADD_GUILD_PRIV_TIME
}

void CInputDB::ChangeCharacterPriv(const char* c_pData)
{
	TPacketDGChangeCharacterPriv* p = (TPacketDGChangeCharacterPriv*) c_pData;
	CPrivManager::instance().GiveCharacterPriv(p->pid, p->type, p->value, p->bLog);
}

void CInputDB::MoneyLog(const char* c_pData)
{
	TPacketMoneyLog * p = (TPacketMoneyLog *) c_pData;

	if (p->type == 4) // QUEST_MONEY_LOG_SKIP
		return;

	if (g_bAuthServer ==true )
		return;

	LogManager::instance().MoneyLog(p->type, p->vnum, p->gold);
}

void CInputDB::GuildMoneyChange(const char* c_pData)
{
	TPacketDGGuildMoneyChange* p = (TPacketDGGuildMoneyChange*) c_pData;

	CGuild* g = CGuildManager::instance().TouchGuild(p->dwGuild);
	if (g)
	{
		g->RecvMoneyChange(p->iTotalGold);
	}
}

void CInputDB::GuildWithdrawMoney(const char* c_pData)
{
	TPacketDGGuildMoneyWithdraw* p = (TPacketDGGuildMoneyWithdraw*) c_pData;

	CGuild* g = CGuildManager::instance().TouchGuild(p->dwGuild);
	if (g)
	{
		g->RecvWithdrawMoneyGive(p->iChangeGold);
	}
}

void CInputDB::SetEventFlag(const char* c_pData)
{
	TPacketSetEventFlag* p = (TPacketSetEventFlag*) c_pData;
	quest::CQuestManager::instance().SetEventFlag(p->szFlagName, p->lValue);
}

void CInputDB::CreateObject(const char * c_pData)
{
	using namespace building;
	CManager::instance().LoadObject((TObject *) c_pData);
}

void CInputDB::DeleteObject(const char * c_pData)
{
	using namespace building;
	CManager::instance().DeleteObject(*(uint32_t *) c_pData);
}

void CInputDB::UpdateLand(const char * c_pData)
{
	using namespace building;
	CManager::instance().UpdateLand((TLand *) c_pData);
}

////////////////////////////////////////////////////////////////////
// Billing
////////////////////////////////////////////////////////////////////
void CInputDB::BillingRepair(const char * c_pData)
{
	uint32_t dwCount = *(uint32_t *) c_pData;
	c_pData += sizeof(uint32_t);

	TPacketBillingRepair * p = (TPacketBillingRepair *) c_pData;

	for (uint32_t i = 0; i < dwCount; ++i, ++p)
	{
		CLoginData * pkLD = M2_NEW CLoginData;

		pkLD->SetKey(p->dwLoginKey);
		pkLD->SetLogin(p->szLogin);
		pkLD->SetIP(p->szHost);

		sys_log(0, "BILLING: REPAIR %s host %s", p->szLogin, p->szHost);
	}
}

void CInputDB::BillingExpire(const char * c_pData)
{
	TPacketBillingExpire * p = (TPacketBillingExpire *) c_pData;

	LPDESC d = DESC_MANAGER::instance().FindByLoginName(p->szLogin);

	if (!d)
		return;

	LPCHARACTER ch = d->GetCharacter();

	if (p->dwRemainSeconds <= 60)
	{
		int i = MAX(5, p->dwRemainSeconds);
		sys_log(0, "BILLING_EXPIRE: %s %u", p->szLogin, p->dwRemainSeconds);
		d->DelayedDisconnect(i);
	}
	else
	{
		if ((p->dwRemainSeconds - d->GetBillingExpireSecond()) > 60)
		{
			d->SetBillingExpireSecond(p->dwRemainSeconds);
#ifdef TEXTS_IMPROVEMENT
			if (ch) {
				ch->ChatPacketNew(CHAT_TYPE_INFO, 241, "%d", (p->dwRemainSeconds / 60));
			}
#endif
		}
	}
}

void CInputDB::BillingLogin(const char * c_pData)
{
	if (nullptr == c_pData)
		return;

	TPacketBillingLogin * p;

	uint32_t dwCount = *(uint32_t *) c_pData;
	c_pData += sizeof(uint32_t);

	p = (TPacketBillingLogin *) c_pData;

	for (uint32_t i = 0; i < dwCount; ++i, ++p)
	{
		DBManager::instance().SetBilling(p->dwLoginKey, p->bLogin);
	}
}

void CInputDB::BillingCheck(const char * c_pData)
{
	uint32_t size = *(uint32_t *) c_pData;
	c_pData += sizeof(uint32_t);

	for (uint32_t i = 0; i < size; ++i)
	{
		uint32_t dwKey = *(uint32_t *) c_pData;
		c_pData += sizeof(uint32_t);

		sys_log(0, "BILLING: NOT_LOGIN %u", dwKey);
		DBManager::instance().SetBilling(dwKey, 0, true);
	}
}

void CInputDB::Notice(const char * c_pData)
{
	char szBuf[256+1];
	strlcpy(szBuf, c_pData, sizeof(szBuf));

	SendNotice(szBuf);
}

void CInputDB::VCard(const char * c_pData)
{
	TPacketGDVCard * p = (TPacketGDVCard *) c_pData;

	sys_log(0, "VCARD: %u %s %s %s %s", p->dwID, p->szSellCharacter, p->szSellAccount, p->szBuyCharacter, p->szBuyAccount);

	std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery("SELECT sell_account, buy_account, time FROM vcard WHERE id=%u", p->dwID));
	if (pmsg->Get()->uiNumRows != 1)
	{
		sys_log(0, "VCARD_FAIL: no data");
		return;
	}

	MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);

	if (strcmp(row[0], p->szSellAccount))
	{
		sys_log(0, "VCARD_FAIL: sell account differ %s", row[0]);
		return;
	}

	if (!row[1] || *row[1])
	{
		sys_log(0, "VCARD_FAIL: buy account already exist");
		return;
	}

	int time = 0;
	str_to_number(time, row[2]);

	if (!row[2] || time < 0)
	{
		sys_log(0, "VCARD_FAIL: time null");
		return;
	}

	std::unique_ptr<SQLMsg> pmsg1(DBManager::instance().DirectQuery("UPDATE GameTime SET LimitTime=LimitTime+%d WHERE UserID='%s'", time, p->szBuyAccount));

	if (pmsg1->Get()->uiAffectedRows == 0 || pmsg1->Get()->uiAffectedRows == (uint32_t)-1)
	{
		sys_log(0, "VCARD_FAIL: cannot modify GameTime table");
		return;
	}

	std::unique_ptr<SQLMsg> pmsg2(DBManager::instance().DirectQuery("UPDATE vcard,GameTime SET sell_pid='%s', buy_pid='%s', buy_account='%s', sell_time=NOW(), new_time=GameTime.LimitTime WHERE vcard.id=%u AND GameTime.UserID='%s'", p->szSellCharacter, p->szBuyCharacter, p->szBuyAccount, p->dwID, p->szBuyAccount));

	if (pmsg2->Get()->uiAffectedRows == 0 || pmsg2->Get()->uiAffectedRows == (uint32_t)-1)
	{
		sys_log(0, "VCARD_FAIL: cannot modify vcard table");
		return;
	}

	sys_log(0, "VCARD_SUCCESS: %s %s", p->szBuyAccount, p->szBuyCharacter);
}

void CInputDB::GuildWarReserveAdd(TGuildWarReserve * p)
{
	CGuildManager::instance().ReserveWarAdd(p);
}

void CInputDB::GuildWarReserveDelete(uint32_t dwID)
{
	CGuildManager::instance().ReserveWarDelete(dwID);
}

void CInputDB::GuildWarBet(TPacketGDGuildWarBet * p)
{
	CGuildManager::instance().ReserveWarBet(p);
}

void CInputDB::MarriageAdd(TPacketMarriageAdd * p)
{
	sys_log(0, "MarriageAdd %u %u %u %s %s", p->dwPID1, p->dwPID2, (uint32_t)p->tMarryTime, p->szName1, p->szName2);
	marriage::CManager::instance().Add(p->dwPID1, p->dwPID2, p->tMarryTime, p->szName1, p->szName2);
}

void CInputDB::MarriageUpdate(TPacketMarriageUpdate * p)
{
	sys_log(0, "MarriageUpdate %u %u %d %d", p->dwPID1, p->dwPID2, p->iLovePoint, p->byMarried);
	marriage::CManager::instance().Update(p->dwPID1, p->dwPID2, p->iLovePoint, p->byMarried);
}

void CInputDB::MarriageRemove(TPacketMarriageRemove * p)
{
	sys_log(0, "MarriageRemove %u %u", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().Remove(p->dwPID1, p->dwPID2);
}

void CInputDB::WeddingRequest(TPacketWeddingRequest* p)
{
	marriage::WeddingManager::instance().Request(p->dwPID1, p->dwPID2);
}

void CInputDB::WeddingReady(TPacketWeddingReady* p)
{
	sys_log(0, "WeddingReady %u %u %u", p->dwPID1, p->dwPID2, p->dwMapIndex);
	marriage::CManager::instance().WeddingReady(p->dwPID1, p->dwPID2, p->dwMapIndex);
}

void CInputDB::WeddingStart(TPacketWeddingStart* p)
{
	sys_log(0, "WeddingStart %u %u", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().WeddingStart(p->dwPID1, p->dwPID2);
}

void CInputDB::WeddingEnd(TPacketWeddingEnd* p)
{
	sys_log(0, "WeddingEnd %u %u", p->dwPID1, p->dwPID2);
	marriage::CManager::instance().WeddingEnd(p->dwPID1, p->dwPID2);
}

// MYSHOP_PRICE_LIST
void CInputDB::MyshopPricelistRes(LPDESC d, const TPacketMyshopPricelistHeader* p )
{
	LPCHARACTER ch;

	if (!d || !(ch = d->GetCharacter()) )
		return;

	sys_log(0, "RecvMyshopPricelistRes name[%s]", ch->GetName());
	ch->UseSilkBotaryReal(p );

}
// END_OF_MYSHOP_PRICE_LIST


//RELOAD_ADMIN
void CInputDB::ReloadAdmin(const char * c_pData )
{
	gm_new_clear();
	int ChunkSize = decode_2bytes(c_pData );
	c_pData += 2;
	int HostSize = decode_2bytes(c_pData );
	c_pData += 2;

	for (int n = 0; n < HostSize; ++n )
	{
		gm_new_host_inert(c_pData );
		c_pData += ChunkSize;
	}


	c_pData += 2;
	int size = 	decode_2bytes(c_pData );
	c_pData += 2;

	for (int n = 0; n < size; ++n )
	{
		tAdminInfo& rAdminInfo = *(tAdminInfo*)c_pData;

		gm_new_insert(rAdminInfo );

		c_pData += sizeof (tAdminInfo );

		LPCHARACTER pChar = CHARACTER_MANAGER::instance().FindPC(rAdminInfo.m_szName );
		if (pChar )
		{
			pChar->SetGMLevel();
		}
	}

}
#ifdef __ENABLE_NEW_OFFLINESHOP__
template <class T>
const char* Decode(T*& pObj, const char* data){
	pObj = (T*) data;
	return data + sizeof(T);
}

void OfflineShopLoadTables(const char* data)
{
	offlineshop::TSubPacketDGLoadTables* pSubPack = nullptr;
	data = Decode(pSubPack, data);
	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	
	OFFSHOP_DEBUG("shop count %u , offer count %u , auction count %u, auction offers %u ",pSubPack->dwShopCount , pSubPack->dwOfferCount, pSubPack->dwAuctionCount , pSubPack->dwAuctionOfferCount);

	for (uint32_t i = 0; i < pSubPack->dwShopCount; i++)
	{
		offlineshop::TShopInfo* pShop = nullptr;
		offlineshop::TItemInfo* pItem = nullptr;

		uint32_t* pdwSoldCount= nullptr;

		data = Decode(pShop, data);
		data = Decode(pdwSoldCount, data);

		OFFSHOP_DEBUG("shop %u %s (solds %u) ",pShop->dwOwnerID , pShop->szName , *pdwSoldCount);

		offlineshop::CShop* pkShop = rManager.PutsNewShop(pShop);

		
		for (uint32_t j = 0; j < pShop->dwCount; j++)
		{
			data = Decode(pItem, data);
			offlineshop::CShopItem kItem(pItem->dwItemID);
			
			kItem.SetOwnerID(pItem->dwOwnerID);
			kItem.SetInfo(pItem->item);
			kItem.SetPrice(pItem->price);
			kItem.SetWindow(NEW_OFFSHOP);

			OFFSHOP_DEBUG("for sale item %u ",pItem->dwItemID);
			pkShop->AddItem(kItem);
		}


		for (uint32_t j = 0; j < *pdwSoldCount; j++)
		{
			data = Decode(pItem, data);
			offlineshop::CShopItem kItem(pItem->dwItemID);

			kItem.SetOwnerID(pItem->dwOwnerID);
			kItem.SetInfo(pItem->item);
			kItem.SetPrice(pItem->price);
			kItem.SetWindow(NEW_OFFSHOP);

			OFFSHOP_DEBUG("sold item %u ",pItem->dwItemID);
			pkShop->AddItemSold(kItem);
		}
	}

	offlineshop::TOfferInfo* pOffer=nullptr;

	for (uint32_t i = 0; i < pSubPack->dwOfferCount; i++)
	{
		data = Decode(pOffer, data);
		OFFSHOP_DEBUG("offer shop : id %u , shopid %u, itemid %u, buyer %u ",pOffer->dwOfferID, pOffer->dwOwnerID, pOffer->dwItemID , pOffer->dwOffererID);
		offlineshop::CShop* pkShop = rManager.GetShopByOwnerID(pOffer->dwOwnerID);

		if (!pkShop)
		{
			sys_err("CANNOT FIND SHOP BY OWNERID (TOfferInfo) %d ",pOffer->dwOwnerID);
			continue;
		}

		pkShop->AddOffer(pOffer);

		//if(!pOffer->bAccepted)
		rManager.PutsNewOffer(pOffer);
	}


	offlineshop::TAuctionInfo*		pTempAuction=nullptr;
	offlineshop::TAuctionOfferInfo* pTempAuctionOffer=nullptr;


	for (uint32_t i = 0; i < pSubPack->dwAuctionCount; i++)
	{
		data = Decode(pTempAuction, data);
		rManager.PutsAuction(*pTempAuction);

		OFFSHOP_DEBUG("auction %u id , %s name , %u minutes ",pTempAuction->dwOwnerID , pTempAuction->szOwnerName, pTempAuction->dwDuration);
	}


	for (uint32_t i = 0; i < pSubPack->dwAuctionOfferCount; i++)
	{
		data = Decode(pTempAuctionOffer, data);
		rManager.PutsAuctionOffer(*pTempAuctionOffer);

		OFFSHOP_DEBUG("offer %u shop , %s buyer ",pTempAuctionOffer->dwOwnerID, pTempAuctionOffer->szBuyerName);
	}
}


void OfflineShopBuyItemPacket(const char* data)
{
	offlineshop::TSubPacketDGBuyItem* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopBuyDBPacket(subpack->dwBuyerID, subpack->dwOwnerID, subpack->dwItemID);
}


void OfflineShopLockedBuyItemPacket(const char* data)
{
	offlineshop::TSubPacketDGLockedBuyItem* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopLockedBuyItemDBPacket(subpack->dwBuyerID, subpack->dwOwnerID, subpack->dwItemID);
}


void OfflineShopEditItemPacket(const char* data)
{
	offlineshop::TSubPacketDGEditItem* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopEditItemDBPacket(subpack->dwOwnerID , subpack->dwItemID, subpack->price);
}


void OfflineShopRemoveItemPacket(const char* data)
{
	offlineshop::TSubPacketDGRemoveItem* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopRemoveItemDBPacket(subpack->dwOwnerID , subpack->dwItemID);
}


void OfflineShopAddItemPacket(const char* data)
{
	offlineshop::TSubPacketDGAddItem* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopAddItemDBPacket(subpack->dwOwnerID, subpack->item);
}


void OfflineShopForceClosePacket(const char* data)
{
	offlineshop::TSubPacketDGShopForceClose* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopForceCloseDBPacket(subpack->dwOwnerID);
}


void OfflineShopShopCreateNewPacket(const char* data)
{
	offlineshop::TSubPacketDGShopCreateNew* subpack;
	data = Decode(subpack, data);

	OFFSHOP_DEBUG("shop %u , dur %u , count %u ",subpack->shop.dwOwnerID , subpack->shop.dwDuration , subpack->shop.dwCount);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();

	std::vector<offlineshop::TItemInfo> vec;
	vec.reserve(subpack->shop.dwCount);

	offlineshop::TItemInfo* pItemInfo=nullptr;

	for (uint32_t i = 0; i < subpack->shop.dwCount; i++)
	{
		data = Decode(pItemInfo, data);
		vec.push_back(*pItemInfo);

		OFFSHOP_DEBUG("item id %u , item vnum %u , item count %u ",pItemInfo->dwItemID , pItemInfo->item.dwVnum , pItemInfo->item.dwCount);
	}


	rManager.RecvShopCreateNewDBPacket(subpack->shop, vec);
}


void OfflineShopShopChangeNamePacket(const char* data)
{
	offlineshop::TSubPacketDGShopChangeName* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopChangeNameDBPacket(subpack->dwOwnerID , subpack->szName);
}


void OfflineShopOfferCreatePacket(const char* data)
{
	offlineshop::TSubPacketDGOfferCreate* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopOfferNewDBPacket(subpack->offer);
}


void OfflineShopOfferNotifiedPacket(const char* data)
{
	offlineshop::TSubPacketDGOfferNotified* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopOfferNotifiedDBPacket(subpack->dwOfferID , subpack->dwOwnerID);
}


void OfflineShopOfferAcceptPacket(const char* data)
{
	offlineshop::TSubPacketDGOfferAccept* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopOfferAcceptDBPacket(subpack->dwOfferID , subpack->dwOwnerID);
}




void OfflineShopOfferCancelPacket(const char* data)
{
	offlineshop::TSubPacketDGOfferCancel* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopOfferCancelDBPacket(subpack->dwOfferID , subpack->dwOwnerID, subpack->IsRemovingItem);//offlineshop-updated 05/08/19
}




void OfflineShopSafeboxAddItemPacket(const char* data)
{
	offlineshop::TSubPacketDGSafeboxAddItem* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopSafeboxAddItemDBPacket(subpack->dwOwnerID , subpack->dwItemID , subpack->item);
}


void OfflineShopSafeboxAddValutesPacket(const char* data)
{
	offlineshop::TSubPacketDGSafeboxAddValutes* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopSafeboxAddValutesDBPacket(subpack->dwOwnerID , subpack->valute);
}



void OfflineShopSafeboxLoad(const char* data)
{
	offlineshop::TSubPacketDGSafeboxLoad* subpack;
	data = Decode(subpack, data);

	std::vector<uint32_t> ids;
	std::vector<offlineshop::TItemInfoEx> items;

	ids.reserve(subpack->dwItemCount);
	items.reserve(subpack->dwItemCount);

	uint32_t* pdwItemID=nullptr;
	offlineshop::TItemInfoEx* temp;

	for (uint32_t i = 0; i < subpack->dwItemCount; i++)
	{
		data = Decode(pdwItemID, data);
		data = Decode(temp, data);

		ids.push_back(*pdwItemID);
		items.push_back(*temp);
	}

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopSafeboxLoadDBPacket(subpack->dwOwnerID , subpack->valute , ids, items);
}


//patch 08-03-2020
void OfflineshopSafeboxExpiredItem(const char* data) {
	offlineshop::TSubPacketDGSafeboxExpiredItem* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopSafeboxExpiredItemDBPacket(subpack->dwOwnerID, subpack->dwItemID);
}


void OfflineShopAuctionCreate(const char* data)
{
	offlineshop::TSubPacketDGAuctionCreate* subpack;
	data = Decode(subpack, data);

	offlineshop::GetManager().RecvAuctionCreateDBPacket(subpack->auction);
}



void OfflineShopAuctionAddOffer(const char* data)
{
	offlineshop::TSubPacketDGAuctionAddOffer* subpack;
	data = Decode(subpack, data);

	offlineshop::GetManager().RecvAuctionAddOfferDBPacket(subpack->offer);
}



void OfflineShopAuctionExpired(const char* data)
{
	offlineshop::TSubPacketDGAuctionExpired* subpack;
	data = Decode(subpack, data);

	offlineshop::GetManager().RecvAuctionExpiredDBPacket(subpack->dwOwnerID);
}






void OfflineshopShopExpired(const char* data)
{
	offlineshop::TSubPacketDGShopExpired* subpack;
	data = Decode(subpack, data);

	offlineshop::CShopManager& rManager = offlineshop::GetManager();
	rManager.RecvShopExpiredDBPacket(subpack->dwOwnerID);
}






void OfflineshopPacket(const char* data)
{
	TPacketDGNewOfflineShop* pPack=nullptr;
	data = Decode(pPack, data);

	OFFSHOP_DEBUG("recv subheader %d",pPack->bSubHeader);

	switch (pPack->bSubHeader)
	{
	case offlineshop::SUBHEADER_DG_LOAD_TABLES:
		OfflineShopLoadTables(data);
		return;

	case offlineshop::SUBHEADER_DG_BUY_ITEM:
		OfflineShopBuyItemPacket(data);
		return;

	case offlineshop::SUBHEADER_DG_LOCKED_BUY_ITEM:
		OfflineShopLockedBuyItemPacket(data);
		return;

	case offlineshop::SUBHEADER_DG_EDIT_ITEM:
		OfflineShopEditItemPacket(data);
		return;
	case offlineshop::SUBHEADER_DG_REMOVE_ITEM:
		OfflineShopRemoveItemPacket(data);
		return;


	case offlineshop::SUBHEADER_DG_ADD_ITEM:
		OfflineShopAddItemPacket(data);
		return;


	case offlineshop::SUBHEADER_DG_SHOP_FORCE_CLOSE:
		OfflineShopForceClosePacket(data);
		return;


	case offlineshop::SUBHEADER_DG_SHOP_CREATE_NEW:
		OfflineShopShopCreateNewPacket(data);
		return;


	case offlineshop::SUBHEADER_DG_SHOP_CHANGE_NAME:
		OfflineShopShopChangeNamePacket(data);
		return;


	case offlineshop::SUBHEADER_DG_SHOP_EXPIRED:
		OfflineshopShopExpired(data);
		break;


	case offlineshop::SUBHEADER_DG_OFFER_CREATE:
		OfflineShopOfferCreatePacket(data);
		return;

	case offlineshop::SUBHEADER_DG_OFFER_NOTIFIED:
		OfflineShopOfferNotifiedPacket(data);
		return;

	case offlineshop::SUBHEADER_DG_OFFER_ACCEPT:
		OfflineShopOfferAcceptPacket(data);
		return;
	
	case offlineshop::SUBHEADER_DG_OFFER_CANCEL:
		OfflineShopOfferCancelPacket(data);
		return;

	

	case offlineshop::SUBHEADER_DG_SAFEBOX_ADD_ITEM:
		OfflineShopSafeboxAddItemPacket(data);
		return;

	case offlineshop::SUBHEADER_DG_SAFEBOX_ADD_VALUTES:
		OfflineShopSafeboxAddValutesPacket(data);
		return;

	case offlineshop::SUBHEADER_DG_SAFEBOX_LOAD:
		OfflineShopSafeboxLoad(data);
		return;

	//patch 08-03-2020
	case offlineshop::SUBHEADER_DG_SAFEBOX_EXPIRED_ITEM:
		OfflineshopSafeboxExpiredItem(data);
		return;


	//AUCTION
	case offlineshop::SUBHEADER_DG_AUCTION_CREATE:
		OfflineShopAuctionCreate(data);
		return;


	case offlineshop::SUBHEADER_DG_AUCTION_ADD_OFFER:
		OfflineShopAuctionAddOffer(data);
		return;


	case offlineshop::SUBHEADER_DG_AUCTION_EXPIRED:
		OfflineShopAuctionExpired(data);
		return;



	default:
		sys_err("UKNOWN SUB HEADER %d ", pPack->bSubHeader);
		return;
	}
}
#endif
//END_RELOAD_ADMIN


#ifdef ENABLE_ITEM_EXTRA_PROTO
void LoadItemExtraProto(const char* data)
{
	TPacketDGLoadItemExtraProto* Pack = (TPacketDGLoadItemExtraProto*)data;
	if (Pack->dwTableSize != sizeof(TItemExtraProto)) {
		sys_err("Invalid TItemExtraProto size %u (known %u) ", Pack->dwTableSize, sizeof(TItemExtraProto));
		return;
	}

	ITEM_MANAGER::instance().InitializeExtraProto((TItemExtraProto*)(data + sizeof(TPacketDGLoadItemExtraProto)), Pack->dwCount);
}
#endif




////////////////////////////////////////////////////////////////////
// Analyze
// @version	05/06/10 Bang2ni - æ∆¿Ã≈€ ∞°∞›¡§∫∏ ∏ÆΩ∫∆Æ ∆–≈∂(HEADER_DG_MYSHOP_PRICELIST_RES) √≥∏Æ∑Á∆æ √ﬂ∞°.
////////////////////////////////////////////////////////////////////
int CInputDB::Analyze(LPDESC d, uint8_t bHeader, const char * c_pData)
{
	switch (bHeader)
	{
	case HEADER_DG_BOOT:
		Boot(c_pData);
		break;

	case HEADER_DG_LOGIN_SUCCESS:
		LoginSuccess(m_dwHandle, c_pData);
		break;

	case HEADER_DG_LOGIN_NOT_EXIST:
		LoginFailure(DESC_MANAGER::instance().FindByHandle(m_dwHandle), "NOID");
		break;

	case HEADER_DG_LOGIN_WRONG_PASSWD:
		LoginFailure(DESC_MANAGER::instance().FindByHandle(m_dwHandle), "WRONGPWD");
		break;

	case HEADER_DG_LOGIN_ALREADY:
		LoginAlready(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_PLAYER_LOAD_SUCCESS:
		PlayerLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_PLAYER_CREATE_SUCCESS:
		PlayerCreateSuccess(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_PLAYER_CREATE_FAILED:
		PlayerCreateFailure(DESC_MANAGER::instance().FindByHandle(m_dwHandle), 0);
		break;

	case HEADER_DG_PLAYER_CREATE_ALREADY:
		PlayerCreateFailure(DESC_MANAGER::instance().FindByHandle(m_dwHandle), 1);
		break;

	case HEADER_DG_PLAYER_DELETE_SUCCESS:
		PlayerDeleteSuccess(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_PLAYER_LOAD_FAILED:
		//sys_log(0, "PLAYER_LOAD_FAILED");
		break;

	case HEADER_DG_PLAYER_DELETE_FAILED:
		//sys_log(0, "PLAYER_DELETE_FAILED");
		PlayerDeleteFail(DESC_MANAGER::instance().FindByHandle(m_dwHandle));
		break;

	case HEADER_DG_ITEM_LOAD:
		ItemLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_QUEST_LOAD:
		QuestLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_AFFECT_LOAD:
		AffectLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

#ifdef ENABLE_BATTLE_PASS
	case HEADER_DG_BATTLE_PASS_LOAD:
		BattlePassLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
		
	case HEADER_DG_BATTLE_PASS_LOAD_RANKING:
		BattlePassLoadRanking(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
#endif

	case HEADER_DG_SAFEBOX_LOAD:
		SafeboxLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_SAFEBOX_CHANGE_SIZE:
		SafeboxChangeSize(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_SAFEBOX_WRONG_PASSWORD:
		SafeboxWrongPassword(DESC_MANAGER::instance().FindByHandle(m_dwHandle));
		break;

	case HEADER_DG_SAFEBOX_CHANGE_PASSWORD_ANSWER:
		SafeboxChangePasswordAnswer(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_MALL_LOAD:
		MallLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_EMPIRE_SELECT:
		EmpireSelect(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_MAP_LOCATIONS:
		MapLocations(c_pData);
		break;

	case HEADER_DG_P2P:
		P2P(c_pData);
		break;

	case HEADER_DG_GUILD_SKILL_UPDATE:
		GuildSkillUpdate(c_pData);
		break;

	case HEADER_DG_GUILD_LOAD:
		GuildLoad(c_pData);
		break;

	case HEADER_DG_GUILD_SKILL_RECHARGE:
		GuildSkillRecharge();
		break;

	case HEADER_DG_GUILD_EXP_UPDATE:
		GuildExpUpdate(c_pData);
		break;

	case HEADER_DG_PARTY_CREATE:
		PartyCreate(c_pData);
		break;

	case HEADER_DG_PARTY_DELETE:
		PartyDelete(c_pData);
		break;

	case HEADER_DG_PARTY_ADD:
		PartyAdd(c_pData);
		break;

	case HEADER_DG_PARTY_REMOVE:
		PartyRemove(c_pData);
		break;

	case HEADER_DG_PARTY_STATE_CHANGE:
		PartyStateChange(c_pData);
		break;

	case HEADER_DG_PARTY_SET_MEMBER_LEVEL:
		PartySetMemberLevel(c_pData);
		break;

	case HEADER_DG_TIME:
		Time(c_pData);
		break;

	case HEADER_DG_GUILD_ADD_MEMBER:
		GuildAddMember(c_pData);
		break;

	case HEADER_DG_GUILD_REMOVE_MEMBER:
		GuildRemoveMember(c_pData);
		break;

	case HEADER_DG_GUILD_CHANGE_GRADE:
		GuildChangeGrade(c_pData);
		break;

	case HEADER_DG_GUILD_CHANGE_MEMBER_DATA:
		GuildChangeMemberData(c_pData);
		break;

	case HEADER_DG_GUILD_DISBAND:
		GuildDisband(c_pData);
		break;

	case HEADER_DG_RELOAD_PROTO:
		ReloadProto(c_pData);
		break;

	case HEADER_DG_GUILD_WAR:
		GuildWar(c_pData);
		break;

	case HEADER_DG_GUILD_WAR_SCORE:
		GuildWarScore(c_pData);
		break;

#ifdef ADVANCED_GUILD_INFO
	case HEADER_DG_GUILD_WAR_RESET:
		GuildResetStats(c_pData);
		break;
#endif

	case HEADER_DG_GUILD_LADDER:
		GuildLadder(c_pData);
		break;

	case HEADER_DG_GUILD_SKILL_USABLE_CHANGE:
		GuildSkillUsableChange(c_pData);
		break;

	case HEADER_DG_CHANGE_NAME:
		ChangeName(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_AUTH_LOGIN:
		AuthLogin(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;

	case HEADER_DG_CHANGE_EMPIRE_PRIV:
		ChangeEmpirePriv(c_pData);
		break;

	case HEADER_DG_CHANGE_GUILD_PRIV:
		ChangeGuildPriv(c_pData);
		break;

	case HEADER_DG_CHANGE_CHARACTER_PRIV:
		ChangeCharacterPriv(c_pData);
		break;

	case HEADER_DG_MONEY_LOG:
		MoneyLog(c_pData);
		break;

	case HEADER_DG_GUILD_WITHDRAW_MONEY_GIVE:
		GuildWithdrawMoney(c_pData);
		break;

	case HEADER_DG_GUILD_MONEY_CHANGE:
		GuildMoneyChange(c_pData);
		break;

	case HEADER_DG_SET_EVENT_FLAG:
		SetEventFlag(c_pData);
		break;

	case HEADER_DG_BILLING_REPAIR:
		BillingRepair(c_pData);
		break;

	case HEADER_DG_BILLING_EXPIRE:
		BillingExpire(c_pData);
		break;

	case HEADER_DG_BILLING_LOGIN:
		BillingLogin(c_pData);
		break;

	case HEADER_DG_BILLING_CHECK:
		BillingCheck(c_pData);
		break;

	case HEADER_DG_VCARD:
		VCard(c_pData);
		break;

	case HEADER_DG_CREATE_OBJECT:
		CreateObject(c_pData);
		break;

	case HEADER_DG_DELETE_OBJECT:
		DeleteObject(c_pData);
		break;

	case HEADER_DG_UPDATE_LAND:
		UpdateLand(c_pData);
		break;

	case HEADER_DG_NOTICE:
		Notice(c_pData);
		break;

	case HEADER_DG_GUILD_WAR_RESERVE_ADD:
		GuildWarReserveAdd((TGuildWarReserve *) c_pData);
		break;

	case HEADER_DG_GUILD_WAR_RESERVE_DEL:
		GuildWarReserveDelete(*(uint32_t *) c_pData);
		break;

	case HEADER_DG_GUILD_WAR_BET:
		GuildWarBet((TPacketGDGuildWarBet *) c_pData);
		break;

	case HEADER_DG_MARRIAGE_ADD:
		MarriageAdd((TPacketMarriageAdd*) c_pData);
		break;

	case HEADER_DG_MARRIAGE_UPDATE:
		MarriageUpdate((TPacketMarriageUpdate*) c_pData);
		break;

	case HEADER_DG_MARRIAGE_REMOVE:
		MarriageRemove((TPacketMarriageRemove*) c_pData);
		break;

	case HEADER_DG_WEDDING_REQUEST:
		WeddingRequest((TPacketWeddingRequest*) c_pData);
		break;

#ifdef ENABLE_ITEMSHOP
	case HEADER_DG_ITEMSHOP:
		ItemShop(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
#endif

	case HEADER_DG_WEDDING_READY:
		WeddingReady((TPacketWeddingReady*) c_pData);
		break;

	case HEADER_DG_WEDDING_START:
		WeddingStart((TPacketWeddingStart*) c_pData);
		break;

	case HEADER_DG_WEDDING_END:
		WeddingEnd((TPacketWeddingEnd*) c_pData);
		break;

		// MYSHOP_PRICE_LIST
	case HEADER_DG_MYSHOP_PRICELIST_RES:
		MyshopPricelistRes(DESC_MANAGER::instance().FindByHandle(m_dwHandle), (TPacketMyshopPricelistHeader*) c_pData );
		break;
		// END_OF_MYSHOP_PRICE_LIST
		//
	// RELOAD_ADMIN
	case HEADER_DG_RELOAD_ADMIN:
		ReloadAdmin(c_pData );
		break;
	//END_RELOAD_ADMIN
#ifdef ENABLE_EVENT_MANAGER
	case HEADER_DG_EVENT_MANAGER:
		EventManager(c_pData);
		break;
#endif
	case HEADER_DG_ACK_CHANGE_GUILD_MASTER :
		this->GuildChangeMaster((TPacketChangeGuildMaster*) c_pData);
		break;
	case HEADER_DG_ACK_SPARE_ITEM_ID_RANGE :
		ITEM_MANAGER::instance().SetMaxSpareItemID(*((TItemIDRangeTable*)c_pData) );
		break;

	case HEADER_DG_UPDATE_HORSE_NAME :
	case HEADER_DG_ACK_HORSE_NAME :
		CHorseNameManager::instance().UpdateHorseName(
				((TPacketUpdateHorseName*)c_pData)->dwPlayerID,
				((TPacketUpdateHorseName*)c_pData)->szHorseName);
		break;

	case HEADER_DG_NEED_LOGIN_LOG:
		DetailLog( (TPacketNeedLoginLogInfo*) c_pData );
		break;
	// µ∂¿œ º±π∞ ±‚¥… ≈◊Ω∫∆Æ
	case HEADER_DG_ITEMAWARD_INFORMER:
		ItemAwardInformer((TPacketItemAwardInfromer*) c_pData);
		break;
#if defined(BL_OFFLINE_MESSAGE)
	case HEADER_DG_RESPOND_OFFLINE_MESSAGES:
		ReadOfflineMessages(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
#endif
	case HEADER_DG_RESPOND_CHANNELSTATUS:
		RespondChannelStatus(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
#ifdef __ENABLE_NEW_OFFLINESHOP__
	case HEADER_DG_NEW_OFFLINESHOP:
		OfflineshopPacket(c_pData);
		break;
#endif
#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
	case HEADER_DG_CHANNEL_RESULT:
		ChangeChannel(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
#endif

#ifdef ENABLE_ITEM_EXTRA_PROTO
	case HEADER_DG_ITEM_EXTRA_PROTO_LOAD:
		LoadItemExtraProto(c_pData);
		break;
#endif

#ifdef __SKILL_COLOR_SYSTEM__
	case HEADER_DG_SKILL_COLOR_LOAD:
		SkillColorLoad(DESC_MANAGER::instance().FindByHandle(m_dwHandle), c_pData);
		break;
#endif
#ifdef ENABLE_HWID
	case HEADER_DG_BLOCKHWID:
		{
			if (g_bAuthServer) {
				const THwidRequest* p = reinterpret_cast<const THwidRequest*>(c_pData);
				CHwidManager::instance().RecvBlockHwid(p->whoname, p->targetname);
			}
			break;
		}
		break;
	case HEADER_DG_UNBLOCKHWID:
		{
			if (g_bAuthServer) {
				const THwidRequest* p = reinterpret_cast<const THwidRequest*>(c_pData);
				CHwidManager::instance().RecvUnblockHwid(p->whoname, p->targetname);
			}
			break;
		}
#endif
	default:
		return (-1);
	}

	return 0;
}

bool CInputDB::Process(LPDESC d, const void * orig, int bytes, int & r_iBytesProceed)
{
	const char *	c_pData = (const char *) orig;
	uint8_t		bHeader, bLastHeader = 0;
	int			iSize;
	int			iLastPacketLen = 0;

	for (m_iBufferLeft = bytes; m_iBufferLeft > 0;)
	{
		if (m_iBufferLeft < 9)
			return true;

		bHeader		= *((uint8_t *) (c_pData));	// 1
		m_dwHandle	= *((uint32_t *) (c_pData + 1));	// 4
		iSize		= *((uint32_t *) (c_pData + 5));	// 4

		sys_log(1, "DBCLIENT: header %d handle %d size %d bytes %d", bHeader, m_dwHandle, iSize, bytes);

		if (m_iBufferLeft - 9 < iSize)
			return true;

		const char * pRealData = (c_pData + 9);

		if (Analyze(d, bHeader, pRealData) < 0)
		{
			sys_err("in InputDB: UNKNOWN HEADER: %d, LAST HEADER: %d(%d), REMAIN BYTES: %d, DESC: %d",
					bHeader, bLastHeader, iLastPacketLen, m_iBufferLeft, d->GetSocket());

			//printdata((uint8_t*) orig, bytes);
			//d->SetPhase(PHASE_CLOSE);
		}

		c_pData		+= 9 + iSize;
		m_iBufferLeft	-= 9 + iSize;
		r_iBytesProceed	+= 9 + iSize;

		iLastPacketLen	= 9 + iSize;
		bLastHeader	= bHeader;
	}

	return true;
}

void CInputDB::GuildChangeMaster(TPacketChangeGuildMaster* p)
{
	CGuildManager::instance().ChangeMaster(p->dwGuildID);
}

void CInputDB::DetailLog(const TPacketNeedLoginLogInfo* info)
{
	LPCHARACTER pChar = CHARACTER_MANAGER::instance().FindByPID( info->dwPlayerID );

	if (nullptr != pChar)
	{
		LogManager::instance().DetailLoginLog(true, pChar);
	}
}

void CInputDB::ItemAwardInformer(TPacketItemAwardInfromer *data)
{
	LPDESC d = DESC_MANAGER::instance().FindByLoginName(data->login);	//login¡§∫∏

	if(d == nullptr)
		return;
	else
	{
		if (d->GetCharacter())
		{
			LPCHARACTER ch = d->GetCharacter();
			ch->SetItemAward_vnum(data->vnum);	// ch ø° ¿”Ω√ ¿˙¿Â«ÿ≥˘¥Ÿ∞° QuestLoad «‘ºˆø°º≠ √≥∏Æ
			ch->SetItemAward_cmd(data->command);

			if(d->IsPhase(PHASE_GAME))			//∞‘¿”∆‰¿Ã¡Ó¿œ∂ß
			{
				quest::CQuestManager::instance().ItemInformer(ch->GetPlayerID(),ch->GetItemAward_vnum());	//questmanager »£√‚
			}
		}
	}
}

void CInputDB::RespondChannelStatus(LPDESC desc, const char* pcData)
{
	if (!desc) {
		return;
	}
	const int nSize = decode_4bytes(pcData);
	pcData += sizeof(nSize);

	uint8_t bHeader = HEADER_GC_RESPOND_CHANNELSTATUS;
	desc->BufferedPacket(&bHeader, sizeof(uint8_t));
	desc->BufferedPacket(&nSize, sizeof(nSize));
	if (0 < nSize) {
		desc->BufferedPacket(pcData, sizeof(TChannelStatus)*nSize);
	}
	uint8_t bSuccess = 1;
	desc->Packet(&bSuccess, sizeof(bSuccess));
	desc->SetChannelStatusRequested(false);
}

#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
void CInputDB::ChangeChannel(LPDESC d, const char* pcData)
{
	if (!d || !d->GetCharacter())
	{
		sys_err("Change channel request with empty or invalid description handle!");
		return;
	}

	TPacketReturnChannel* p = (TPacketReturnChannel*)pcData;
	if (!p->lAddr || !p->port) {
#ifdef TEXTS_IMPROVEMENT
		d->GetCharacter()->ChatPacketNew(CHAT_TYPE_INFO, 636, "");
#endif
		return;
	}

	// Execute
	d->GetCharacter()->StartChannelSwitch(p->lAddr, p->port);
}
#endif

#if defined(BL_OFFLINE_MESSAGE)
#include "buffer_manager.h"
void CInputDB::ReadOfflineMessages(LPDESC desc, const char* pcData)
{
	if (!desc || !desc->GetCharacter())
		return;

	if (desc->GetCharacter()->IsBlockMode(BLOCK_WHISPER))
		return;
	
	auto p = reinterpret_cast<const TPacketDGReadOfflineMessage*>(pcData);

	TPacketGCWhisper pack;
	int len = MIN(CHAT_MAX_LEN, strlen(p->szMessage) + 1);
	pack.bHeader = HEADER_GC_WHISPER;
	pack.wSize = static_cast<uint16_t>(sizeof(TPacketGCWhisper) + len);
	pack.bType = WHISPER_TYPE_OFFLINE;
	strlcpy(pack.szNameFrom, p->szFrom, sizeof(pack.szNameFrom));

	TEMP_BUFFER buf;
	buf.write(&pack, sizeof(TPacketGCWhisper));
	buf.write(p->szMessage, len);
	desc->Packet(buf.read_peek(), buf.size());
}
#endif
#ifdef ENABLE_EVENT_MANAGER
void CInputDB::EventManager(const char* c_pData)
{
	CHARACTER_MANAGER& chrMngr = CHARACTER_MANAGER::Instance();
	const uint8_t subIndex = *(uint8_t*)c_pData;
	c_pData += sizeof(uint8_t);
	if (subIndex == EVENT_MANAGER_LOAD)
	{
		chrMngr.ClearEventData();
		const uint8_t dayCount = *(uint8_t*)c_pData;
		c_pData += sizeof(uint8_t);

		const bool updateFromGameMaster = *(bool*)c_pData;
		c_pData += sizeof(bool);

		for (uint32_t x = 0; x < dayCount; ++x)
		{
			const uint8_t dayIndex = *(uint8_t*)c_pData;
			c_pData += sizeof(uint8_t);

			const uint8_t dayEventCount = *(uint8_t*)c_pData;
			c_pData += sizeof(uint8_t);

			if (dayEventCount > 0)
			{
				std::vector<TEventManagerData> m_vec;
				m_vec.resize(dayEventCount);
				memcpy(&m_vec[0], c_pData, dayEventCount*sizeof(TEventManagerData));
				c_pData += dayEventCount * sizeof(TEventManagerData);
				chrMngr.SetEventData(dayIndex, m_vec);

			}
		}
		if (updateFromGameMaster)
			chrMngr.UpdateAllPlayerEventData();
	}
	else if (EVENT_MANAGER_EVENT_STATUS == subIndex)
	{
		const uint16_t& eventID = *(uint16_t*)c_pData;
		c_pData += sizeof(uint16_t);
		const bool& eventStatus = *(bool*)c_pData;
		c_pData += sizeof(bool);
		const int& endTime = *(int*)c_pData;
		c_pData += sizeof(int);
		char endTimeText[25];
		strlcpy(endTimeText, c_pData, sizeof(endTimeText));
		c_pData += sizeof(endTimeText);
		chrMngr.SetEventStatus(eventID, eventStatus, endTime, endTimeText);
	}
}
#endif

#ifdef ENABLE_ITEMSHOP
void CInputDB::ItemShop(LPDESC d, const char* c_pData)
{
	const uint8_t subIndex = *(uint8_t*)c_pData;
	c_pData += sizeof(uint8_t);

	if (subIndex == ITEMSHOP_LOAD)
		CHARACTER_MANAGER::Instance().LoadItemShopData(c_pData);
	else if (subIndex == ITEMSHOP_LOG)
	{
		if (!d)
			return;
		CHARACTER_MANAGER::Instance().LoadItemShopLogReal(d->GetCharacter(), c_pData);
	}
	else if (subIndex == ITEMSHOP_BUY)
	{
		if (!d)
			return;
		CHARACTER_MANAGER::Instance().LoadItemShopBuyReal(d->GetCharacter(), c_pData);
	}
}
#endif

