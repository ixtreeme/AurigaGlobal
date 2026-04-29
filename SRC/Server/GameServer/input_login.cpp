#include "stdafx.h"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "constants.h"

#include "config.h"
#include "utils.h"
#include "input.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "item.h"
#include "char_manager.h"
#include "cmd.h"
#include "buffer_manager.h"
#include "protocol.h"
#include "pvp.h"
#include "start_position.h"
#include "messenger_manager.h"
#include "guild_manager.h"
#include "party.h"
#include "dungeon.h"
#include "war_map.h"
#include "questmanager.h"
#include "building.h"
#include "wedding.h"
#include "affect.h"
#include "arena.h"
#include "OXEvent.h"
#include "priv_manager.h"
#include "dev_log.h"
#include "log.h"
#include "horsename_manager.h"
#include "MarkManager.h"
#include <common/CommonDefines.h>
#ifdef __ENABLE_NEW_OFFLINESHOP__
#include "new_offlineshop.h"
#include "new_offlineshop_manager.h"
#endif
#ifdef ENABLE_SWITCHBOT
#include "new_switchbot.h"
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
#include "MountSystem.h"
#endif
#ifdef ENABLE_CPP_DUNGEON_RAZOR93
#include "OrcsDungeon.h"
#include "TritonTempleDungeon.h"
#include "ValentineDungeon.h"
#include "RuneDungeon.h"
#include "PyramidDungeonRazor93.h"
#include "NightmareDungeonRazor93.h"
//#include "LostCastleDungeon.h"
#include "Halloween2022Dungeon.h"
#include "VikingDungeon.h"
#include "EasterDungeon.h"
#endif
#ifdef ENABLE_WOLFMAN_CHARACTER

// #define USE_LYCAN_CREATE_POSITION
#ifdef USE_LYCAN_CREATE_POSITION

uint32_t g_lycan_create_position[4][2] =
{
	{		0,		0 },
	{ 768000 + 38300, 896000 + 35500 },
	{ 819200 + 38300, 896000 + 35500 },
	{ 870400 + 38300, 896000 + 35500 },
};

inline uint32_t LYCAN_CREATE_START_X(uint8_t e, uint8_t job)
{
	if (1 <= e && e <= 3)
		return (job == JOB_WOLFMAN) ? g_lycan_create_position[e][0] : g_create_position[e][0];
	return 0;
}

inline uint32_t LYCAN_CREATE_START_Y(uint8_t e, uint8_t job)
{
	if (1 <= e && e <= 3)
		return (job == JOB_WOLFMAN) ? g_lycan_create_position[e][1] : g_create_position[e][1];
	return 0;
}

#endif


#endif

static void _send_bonus_info(LPCHARACTER ch)
{
	int	item_drop_bonus = 0;
	int gold_drop_bonus = 0;
	int gold10_drop_bonus = 0;
	int exp_bonus = 0;

	item_drop_bonus = CPrivManager::instance().GetPriv(ch, PRIV_ITEM_DROP);
	gold_drop_bonus = CPrivManager::instance().GetPriv(ch, PRIV_GOLD_DROP);
	gold10_drop_bonus = CPrivManager::instance().GetPriv(ch, PRIV_GOLD10_DROP);
	exp_bonus = CPrivManager::instance().GetPriv(ch, PRIV_EXP_PCT);
#ifdef TEXTS_IMPROVEMENT
	if (item_drop_bonus) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 243, "%d", item_drop_bonus);
	}
	if (gold_drop_bonus) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 244, "%d", item_drop_bonus);
	}
	if (gold10_drop_bonus) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 245, "%d", item_drop_bonus);
	}
	if (exp_bonus) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 246, "%d", item_drop_bonus);
	}
#endif
}

static bool FN_is_battle_zone(LPCHARACTER ch)
{
	switch (ch->GetMapIndex())
	{
	case 1:         // ¿ 1 
	case 2:         // ¿ 2 
	case 21:        // o 1 
	case 23:        // o 2 
	case 41:        //  1 
	case 43:        //  2 
	case 113:       // OX 
		return false;
	}

	return true;
}

void CInputLogin::Login(LPDESC d, const char* data)
{
	TPacketCGLogin* pinfo = (TPacketCGLogin*)data;

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(pinfo->login, login, sizeof(login));

	sys_log(0, "InputLogin::Login : %s", login);

	TPacketGCLoginFailure failurePacket;

	if (!test_server)
	{
		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "VERSION", sizeof(failurePacket.szStatus));
		d->Packet(&failurePacket, sizeof(TPacketGCLoginFailure));
		return;
	}

	if (g_bNoMoreClient)
	{
		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "SHUTDOWN", sizeof(failurePacket.szStatus));
		d->Packet(&failurePacket, sizeof(TPacketGCLoginFailure));
		return;
	}

	if (g_iUserLimit > 0)
	{
		int iTotal;
		int* paiEmpireUserCount;
		int iLocal;

		DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);

		if (g_iUserLimit <= iTotal)
		{
			failurePacket.header = HEADER_GC_LOGIN_FAILURE;
			strlcpy(failurePacket.szStatus, "FULL", sizeof(failurePacket.szStatus));
			d->Packet(&failurePacket, sizeof(TPacketGCLoginFailure));
			return;
		}
	}

	TLoginPacket login_packet;

	strlcpy(login_packet.login, login, sizeof(login_packet.login));
	strlcpy(login_packet.passwd, pinfo->passwd, sizeof(login_packet.passwd));

	db_clientdesc->DBPacket(HEADER_GD_LOGIN, d->GetHandle(), &login_packet, sizeof(TLoginPacket));
}

void CInputLogin::LoginByKey(LPDESC d, const char* data)
{
	TPacketCGLogin2* pinfo = (TPacketCGLogin2*)data;

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(pinfo->login, login, sizeof(login));

	if (g_bNoMoreClient)
	{
		TPacketGCLoginFailure failurePacket;

		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "SHUTDOWN", sizeof(failurePacket.szStatus));
		d->Packet(&failurePacket, sizeof(TPacketGCLoginFailure));
		return;
	}

	if (g_iUserLimit > 0)
	{
		int iTotal;
		int* paiEmpireUserCount;
		int iLocal;

		DESC_MANAGER::instance().GetUserCount(iTotal, &paiEmpireUserCount, iLocal);

		if (g_iUserLimit <= iTotal)
		{
			TPacketGCLoginFailure failurePacket;

			failurePacket.header = HEADER_GC_LOGIN_FAILURE;
			strlcpy(failurePacket.szStatus, "FULL", sizeof(failurePacket.szStatus));

			d->Packet(&failurePacket, sizeof(TPacketGCLoginFailure));
			return;
		}
	}

	sys_log(0, "LOGIN_BY_KEY: %s key %u", login, pinfo->dwLoginKey);

	d->SetLoginKey(pinfo->dwLoginKey);
#ifndef _IMPROVED_PACKET_ENCRYPTION_
	d->SetSecurityKey(pinfo->adwClientKey);
#endif

	TPacketGDLoginByKey ptod;

	strlcpy(ptod.szLogin, login, sizeof(ptod.szLogin));
	ptod.dwLoginKey = pinfo->dwLoginKey;
	memcpy(ptod.adwClientKey, pinfo->adwClientKey, sizeof(uint32_t) * 4);
	strlcpy(ptod.szIP, d->GetHostName(), sizeof(ptod.szIP));

	db_clientdesc->DBPacket(HEADER_GD_LOGIN_BY_KEY, d->GetHandle(), &ptod, sizeof(TPacketGDLoginByKey));
}

void CInputLogin::ChangeName(LPDESC d, const char* data)
{
	TPacketCGChangeName* p = (TPacketCGChangeName*)data;
	const TAccountTable& c_r = d->GetAccountTable();

	if (!c_r.id)
	{
		sys_err("no account table");
		return;
	}

	if (!c_r.players[p->index].bChangeName)
		return;

	if (!check_name(p->name))
	{
		TPacketGCCreateFailure pack;
		pack.header = HEADER_GC_CHARACTER_CREATE_FAILURE;
		pack.bType = 0;
		d->Packet(&pack, sizeof(pack));
		return;
	}

	TPacketGDChangeName pdb;

	pdb.pid = c_r.players[p->index].dwID;
	strlcpy(pdb.name, p->name, sizeof(pdb.name));
	db_clientdesc->DBPacket(HEADER_GD_CHANGE_NAME, d->GetHandle(), &pdb, sizeof(TPacketGDChangeName));
}

void CInputLogin::CharacterSelect(LPDESC d, const char* data)
{
	struct command_player_select* pinfo = (struct command_player_select*)data;
	const TAccountTable& c_r = d->GetAccountTable();

	sys_log(0, "player_select: login: %s index: %d", c_r.login, pinfo->index);

	if (!c_r.id)
	{
		sys_err("no account table");
		return;
	}

	if (pinfo->index >= PLAYER_PER_ACCOUNT)
	{
		sys_err("index overflow %d, login: %s", pinfo->index, c_r.login);
		return;
	}

	if (c_r.players[pinfo->index].bChangeName)
	{
		sys_err("name must be changed idx %d, login %s, name %s",
			pinfo->index, c_r.login, c_r.players[pinfo->index].szName);
		return;
	}

	TPlayerLoadPacket player_load_packet;

	player_load_packet.account_id = c_r.id;
	player_load_packet.player_id = c_r.players[pinfo->index].dwID;
	player_load_packet.account_index = pinfo->index;

	if (player_load_packet.player_id == 0)//--db expolit fix
	{
		sys_err("invalid player_id from account %d\n", c_r.id);
		d->DelayedDisconnect(0);
		return;
	}//--
	db_clientdesc->DBPacket(HEADER_GD_PLAYER_LOAD, d->GetHandle(), &player_load_packet, sizeof(TPlayerLoadPacket));
}

bool NewPlayerTable(TPlayerTable* table,
	const char* name,
	uint8_t job,
	uint8_t shape,
	uint8_t bEmpire,
	uint8_t bCon,
	uint8_t bInt,
	uint8_t bStr,
	uint8_t bDex)
{
	if (job >= JOB_MAX_NUM)
		return false;

	memset(table, 0, sizeof(TPlayerTable));

	strlcpy(table->name, name, sizeof(table->name));

	table->level = 1;
	table->job = job;
	table->voice = 0;
	table->part_base = shape;

	table->st = JobInitialPoints[job].st;
	table->dx = JobInitialPoints[job].dx;
	table->ht = JobInitialPoints[job].ht;
	table->iq = JobInitialPoints[job].iq;

	table->hp = JobInitialPoints[job].max_hp + table->ht * JobInitialPoints[job].hp_per_ht;
	table->sp = JobInitialPoints[job].max_sp + table->iq * JobInitialPoints[job].sp_per_iq;
	table->stamina = JobInitialPoints[job].max_stamina;

#if defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_LYCAN_CREATE_POSITION)
	table->x = LYCAN_CREATE_START_X(bEmpire, job) + number(-300, 300);
	table->y = LYCAN_CREATE_START_Y(bEmpire, job) + number(-300, 300);
#else
	table->x = CREATE_START_X(bEmpire) + number(-300, 300);
	table->y = CREATE_START_Y(bEmpire) + number(-300, 300);
#endif
	table->z = 0;
	table->dir = 0;
	table->playtime = 0;
	table->gold =
#ifdef ENABLE_REWARD_AT_START
		500000
#else
		0
#endif
		;
#ifdef ENABLE_GAYA_SYSTEM
	table->gaya = 0;
#endif

	table->skill_group = 0;

	return true;
}

bool RaceToJob(unsigned race, unsigned* ret_job)
{
	*ret_job = 0;

	if (race >= MAIN_RACE_MAX_NUM)
		return false;

	switch (race)
	{
	case MAIN_RACE_WARRIOR_M:
		*ret_job = JOB_WARRIOR;
		break;

	case MAIN_RACE_WARRIOR_W:
		*ret_job = JOB_WARRIOR;
		break;

	case MAIN_RACE_ASSASSIN_M:
		*ret_job = JOB_ASSASSIN;
		break;

	case MAIN_RACE_ASSASSIN_W:
		*ret_job = JOB_ASSASSIN;
		break;

	case MAIN_RACE_SURA_M:
		*ret_job = JOB_SURA;
		break;

	case MAIN_RACE_SURA_W:
		*ret_job = JOB_SURA;
		break;

	case MAIN_RACE_SHAMAN_M:
		*ret_job = JOB_SHAMAN;
		break;

	case MAIN_RACE_SHAMAN_W:
		*ret_job = JOB_SHAMAN;
		break;
#ifdef ENABLE_WOLFMAN_CHARACTER
	case MAIN_RACE_WOLFMAN_M:
		*ret_job = JOB_WOLFMAN;
		break;
#endif
	default:
		return false;
		break;
	}
	return true;
}

// û ? 
bool NewPlayerTable2(TPlayerTable* table, const char* name, uint8_t race, uint8_t shape, uint8_t bEmpire)
{
	if (race >= MAIN_RACE_MAX_NUM)
	{
		sys_err("NewPlayerTable2.OUT_OF_RACE_RANGE(%d >= max(%d))\n", race, MAIN_RACE_MAX_NUM);
		return false;
	}

	unsigned job;

	if (!RaceToJob(race, &job))
	{
		sys_err("NewPlayerTable2.RACE_TO_JOB_ERROR(%d)\n", race);
		return false;
	}

	sys_log(0, "NewPlayerTable2(name=%s, race=%d, job=%d)", name, race, job);

	memset(table, 0, sizeof(TPlayerTable));

	strlcpy(table->name, name, sizeof(table->name));

	table->level = 1;
	table->job = race; //   ?´
	table->voice = 0;
	table->part_base = shape;

	table->st = JobInitialPoints[job].st;
	table->dx = JobInitialPoints[job].dx;
	table->ht = JobInitialPoints[job].ht;
	table->iq = JobInitialPoints[job].iq;

	table->hp = JobInitialPoints[job].max_hp + table->ht * JobInitialPoints[job].hp_per_ht;
	table->sp = JobInitialPoints[job].max_sp + table->iq * JobInitialPoints[job].sp_per_iq;
	table->stamina = JobInitialPoints[job].max_stamina;

#if defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_LYCAN_CREATE_POSITION)
	table->x = LYCAN_CREATE_START_X(bEmpire, job) + number(-300, 300);
	table->y = LYCAN_CREATE_START_Y(bEmpire, job) + number(-300, 300);
#else
	table->x = CREATE_START_X(bEmpire) + number(-300, 300);
	table->y = CREATE_START_Y(bEmpire) + number(-300, 300);
#endif
	table->z = 0;
	table->dir = 0;
	table->playtime = 0;
	table->gold =
#ifdef ENABLE_REWARD_AT_START
		0
#else
		0
#endif
		;
#ifdef ENABLE_GAYA_SYSTEM
	table->gaya = 0;
#endif
	table->stat_point = 0;
	table->skill_group = 0;

	return true;
}

void CInputLogin::CharacterCreate(LPDESC d, const char* data)
{
	struct command_player_create* pinfo = (struct command_player_create*)data;
	TPlayerCreatePacket player_create_packet;

	sys_log(0, "PlayerCreate: name %s pos %d job %d shape %d",
		pinfo->name,
		pinfo->index,
		pinfo->job,
		pinfo->shape);

	TPacketGCLoginFailure packFailure;
	memset(&packFailure, 0, sizeof(packFailure));
	packFailure.header = HEADER_GC_CHARACTER_CREATE_FAILURE;

	if (true == g_BlockCharCreation)
	{
		d->Packet(&packFailure, sizeof(packFailure));
		return;
	}

#ifdef ENABLE_BUG_FIXES
	if (strlen(pinfo->name) > 12) {
		d->Packet(&packFailure, sizeof(packFailure));
		return;
	}
#endif

	//    ??u, ? ?  
	if (!check_name(pinfo->name) || pinfo->shape > 1)
	{
		d->Packet(&packFailure, sizeof(packFailure));
		return;
	}

	const TAccountTable& c_rAccountTable = d->GetAccountTable();

	if (0 == strcmp(c_rAccountTable.login, pinfo->name))
	{
		TPacketGCCreateFailure pack;
		pack.header = HEADER_GC_CHARACTER_CREATE_FAILURE;
		pack.bType = 1;

		d->Packet(&pack, sizeof(pack));
		return;
	}

	memset(&player_create_packet, 0, sizeof(TPlayerCreatePacket));

	if (!NewPlayerTable2(&player_create_packet.player_table, pinfo->name, pinfo->job, pinfo->shape, d->GetEmpire()))
	{
		sys_err("player_prototype error: job %d face %d ", pinfo->job);
		d->Packet(&packFailure, sizeof(packFailure));
		return;
	}

	trim_and_lower(c_rAccountTable.login, player_create_packet.login, sizeof(player_create_packet.login));
	strlcpy(player_create_packet.passwd, c_rAccountTable.passwd, sizeof(player_create_packet.passwd));

	player_create_packet.account_id = c_rAccountTable.id;
	player_create_packet.account_index = pinfo->index;

	sys_log(0, "PlayerCreate: name %s account_id %d, TPlayerCreatePacketSize(%d), Packet->Gold %d",
		pinfo->name,
		pinfo->index,
		sizeof(TPlayerCreatePacket),
		player_create_packet.player_table.gold);

	db_clientdesc->DBPacket(HEADER_GD_PLAYER_CREATE, d->GetHandle(), &player_create_packet, sizeof(TPlayerCreatePacket));
}

void CInputLogin::CharacterDelete(LPDESC d, const char* data)
{
	struct command_player_delete* pinfo = (struct command_player_delete*)data;
	const TAccountTable& c_rAccountTable = d->GetAccountTable();

	if (!c_rAccountTable.id)
	{
		sys_err("PlayerDelete: no login data");
		return;
	}

	sys_log(0, "PlayerDelete: login: %s index: %d, social_id %s", c_rAccountTable.login, pinfo->index, pinfo->private_code);

	if (pinfo->index >= PLAYER_PER_ACCOUNT)
	{
		sys_err("PlayerDelete: index overflow %d, login: %s", pinfo->index, c_rAccountTable.login);
		return;
	}

	if (!c_rAccountTable.players[pinfo->index].dwID)
	{
		sys_err("PlayerDelete: Wrong Social ID index %d, login: %s", pinfo->index, c_rAccountTable.login);
		d->Packet(encode_byte(HEADER_GC_CHARACTER_DELETE_WRONG_SOCIAL_ID), 1);
		return;
	}

	// @fixme143 BEGIN
	static char __private_code[8 * 2 + 1];
	DBManager::instance().EscapeString(__private_code, sizeof(__private_code), pinfo->private_code, strnlen(pinfo->private_code, sizeof(pinfo->private_code)));
	if (strncmp(__private_code, pinfo->private_code, strnlen(pinfo->private_code, sizeof(pinfo->private_code))))
	{
		return;
	}
	// @fixme143 END

	TPlayerDeletePacket	player_delete_packet;
	trim_and_lower(c_rAccountTable.login, player_delete_packet.login, sizeof(player_delete_packet.login));
	player_delete_packet.player_id = c_rAccountTable.players[pinfo->index].dwID;
	player_delete_packet.account_index = pinfo->index;
	strlcpy(player_delete_packet.private_code, __private_code, sizeof(player_delete_packet.private_code));
	db_clientdesc->DBPacket(HEADER_GD_PLAYER_DELETE, d->GetHandle(), &player_delete_packet, sizeof(TPlayerDeletePacket));
}

void CInputLogin::Entergame(LPDESC d, const char* data)
{
	LPCHARACTER ch;

	if (!(ch = d->GetCharacter()))
	{
		d->SetPhase(PHASE_CLOSE);
		return;
	}

	PIXEL_POSITION pos = ch->GetXYZ();

	if (!SECTREE_MANAGER::instance().GetMovablePosition(ch->GetMapIndex(), pos.x, pos.y, pos))
	{
		PIXEL_POSITION pos2;
		SECTREE_MANAGER::instance().GetRecallPositionByEmpire(ch->GetMapIndex(), ch->GetEmpire(), pos2);

		sys_err("!GetMovablePosition (name %s %dx%d map %d changed to %dx%d)",
			ch->GetName(),
			pos.x, pos.y,
			ch->GetMapIndex(),
			pos2.x, pos2.y);
		pos = pos2;
	}

	CGuildManager::instance().LoginMember(ch);

	// ?? ? ?
	ch->Show(ch->GetMapIndex(), pos.x, pos.y, pos.z);
	ch->ReviveInvisible(5);
	d->SetPhase(PHASE_GAME);
	SECTREE_MANAGER::instance().SendNPCPosition(ch);
#ifdef ENABLE_ATLAS_BOSS
	SECTREE_MANAGER::instance().SendBossPosition(ch);
#endif
#ifdef ENABLE_CPP_DUNGEON_RAZOR93

	COrcsDungeon::instance().OnPlayerLogin(ch);
	CTritonTempleDungeon::instance().OnPlayerLogin(ch);
	CValentineDungeon::instance().OnPlayerLogin(ch);
	CRuneDungeon::instance().OnPlayerLogin(ch);
	CPyramidDungeonRazor93::instance().OnPlayerLogin(ch);
	CNightmareDungeonRazor93::instance().OnPlayerLogin(ch);
	//CLostCastleDungeon::instance().OnPlayerLogin(ch);
	CHalloween2022Dungeon::instance().OnPlayerLogin(ch);
	CVikingDungeon::instance().OnPlayerLogin(ch);
	CEasterDungeon::instance().OnPlayerLogin(ch);
#endif

#ifdef __HIDE_COSTUME_SYSTEM__
	if (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "costume_option.hide_body") != 0)
		ch->SetBodyCostumeHidden(true);
	else
		ch->SetBodyCostumeHidden(false);

	if (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "costume_option.hide_hair") != 0)
		ch->SetHairCostumeHidden(true);
	else
		ch->SetHairCostumeHidden(false);

#ifdef ENABLE_ACCE_SYSTEM
	if (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "costume_option.hide_acce") != 0)
		ch->SetAcceCostumeHidden(true);
	else
		ch->SetAcceCostumeHidden(false);
#endif

#ifdef __WEAPON_COSTUME_SYSTEM__
	if (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "costume_option.hide_weapon") != 0)
		ch->SetWeaponCostumeHidden(true);
	else
		ch->SetWeaponCostumeHidden(false);
#endif
#endif


	if (ch->GetItemAward_cmd())																		// ?
		quest::CQuestManager::instance().ItemInformer(ch->GetPlayerID(), ch->GetItemAward_vnum());	//questmanager ?

	sys_log(0, "ENTERGAME: %s %dx%dx%d %s map_index %d",
		ch->GetName(), ch->GetX(), ch->GetY(), ch->GetZ(), d->GetHostName(), ch->GetMapIndex());

	if (ch->GetHorseLevel() > 0)
	{
		ch->EnterHorse();
	}

	// ÷?? ? 
	ch->ResetPlayTime();

	// ?  ?T ?
	ch->StartSaveEvent();
	ch->StartRecoveryEvent();

	CPVPManager::instance().Connect(ch);
	CPVPManager::instance().SendList(d);

	MessengerManager::instance().Login(ch->GetName());

	CPartyManager::instance().SetParty(ch);
	CGuildManager::instance().SendGuildWar(ch);

	building::CManager::instance().SendLandList(d, ch->GetMapIndex());

	marriage::CManager::instance().Login(ch);
#ifdef ENABLE_EVENT_MANAGER
	CHARACTER_MANAGER::Instance().SendDataPlayer(ch);
#endif

	TPacketGCTime p;
	p.bHeader = HEADER_GC_TIME;
	p.time = get_global_time();
	d->Packet(&p, sizeof(p));

	TPacketGCChannel p2;
	p2.header = HEADER_GC_CHANNEL;
	p2.channel = g_bChannel;
	d->Packet(&p2, sizeof(p2));
	ch->SendGreetMessage();
#ifdef ENABLE_ITEM_ON_TITLE_RAZOR93
	ch->UpdateItemOnTitleName(true);
#endif
#ifdef ENABLE_PVP_ADVANCED // If something is wrong and server is crashed or stopping when you was in duel.
	int isDuel = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), CHECK_IS_FIGHT);
	if (isDuel)
		ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), CHECK_IS_FIGHT, 0);
#endif

	_send_bonus_info(ch);
#if defined(BL_OFFLINE_MESSAGE)
	ch->ReadOfflineMessages();
#endif

	for (int i = 0; i <= PREMIUM_MAX_NUM; ++i)
	{
		int remain = ch->GetPremiumRemainSeconds(i);

		if (remain <= 0)
			continue;

		ch->AddAffect(AFFECT_PREMIUM_START + i, POINT_NONE, 0, 0, remain, 0, true);
		sys_log(0, "PREMIUM: %s type %d %dmin", ch->GetName(), i, remain);
	}

	if (g_bCheckClientVersion)
	{
		sys_log(0, "VERSION CHECK %s %s", g_stClientVersion.c_str(), d->GetClientVersion());

		if (!d->GetClientVersion()) {
			d->DelayedDisconnect(10);
		}
		else {
			if (0 != g_stClientVersion.compare(d->GetClientVersion())) {
				d->DelayedDisconnect(0);
				LogManager::instance().HackLog("VERSION_CONFLICT", ch);
			}
		}
	}
	else
	{
		sys_log(0, "VERSION : NO CHECK");
	}

	if (ch->IsGM() == true)
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "ConsoleEnable");

	if (ch->GetMapIndex() >= 10000)
	{
		if (CWarMapManager::instance().IsWarMap(ch->GetMapIndex()))
			ch->SetWarMap(CWarMapManager::instance().Find(ch->GetMapIndex()));
		else if (marriage::WeddingManager::instance().IsWeddingMap(ch->GetMapIndex()))
			ch->SetWeddingMap(marriage::WeddingManager::instance().Find(ch->GetMapIndex()));
		else {
			ch->SetDungeon(CDungeonManager::instance().FindByMapIndex(ch->GetMapIndex()));
		}
	}
	else if (CArenaManager::instance().IsArenaMap(ch->GetMapIndex()) == true)
	{
		int memberFlag = CArenaManager::instance().IsMember(ch->GetMapIndex(), ch->GetPlayerID());
		if (memberFlag == MEMBER_OBSERVER)
		{
			ch->SetObserverMode(true);
			ch->SetArenaObserverMode(true);
			if (CArenaManager::instance().RegisterObserverPtr(ch, ch->GetMapIndex(), ch->GetX() / 100, ch->GetY() / 100))
			{
				sys_log(0, "ARENA : Observer add failed");
			}

			if (ch->IsHorseRiding() == true)
			{
				ch->StopRiding();
				ch->HorseSummon(false);
			}
		}
		else if (memberFlag == MEMBER_DUELIST)
		{
			TPacketGCDuelStart duelStart;
			duelStart.header = HEADER_GC_DUEL_START;
			duelStart.wSize = sizeof(TPacketGCDuelStart);

			ch->GetDesc()->Packet(&duelStart, sizeof(TPacketGCDuelStart));

			if (ch->IsHorseRiding() == true)
			{
				ch->StopRiding();
				ch->HorseSummon(false);
			}

			LPPARTY pParty = ch->GetParty();
			if (pParty != nullptr)
			{
				if (pParty->GetMemberCount() == 2)
				{
					CPartyManager::instance().DeleteParty(pParty);
				}
				else
				{
					pParty->Quit(ch->GetPlayerID());
				}
			}
		}
		else if (memberFlag == MEMBER_NO)
		{
			if (ch->GetGMLevel() == GM_PLAYER)
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
		}
		else
		{
			// wtf
		}
	}
	else if (ch->GetMapIndex() == 113)
	{
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (ch->IsHorseRiding()) {
			ch->StopRiding();
			ch->HorseSummon(false);
		}

		CMountSystem* mountSystem = ch->GetMountSystem();
		LPITEM mount = ch->GetWear(WEAR_COSTUME_MOUNT);
		if (mountSystem && ch->GetMountVnum() && mount) {
			mountSystem->Unmount(mount->GetValue(1));
		}
		else {
			ch->RemoveAffect(AFFECT_MOUNT);
			ch->RemoveAffect(AFFECT_MOUNT_BONUS);
		}
#endif
		// ox ?T 
		if (COXEventManager::instance().Enter(ch) == false)
		{
			// ox   ?  . ÷?  
			if (ch->GetGMLevel() == GM_PLAYER)
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
		}
	}
	else
	{
		if (CWarMapManager::instance().IsWarMap(ch->GetMapIndex()) ||
			marriage::WeddingManager::instance().IsWeddingMap(ch->GetMapIndex()))
		{
			if (!test_server)
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
		}
	}

	if (ch->GetHorseLevel() > 0)
	{
		uint32_t pid = ch->GetPlayerID();
		if (pid != 0 && CHorseNameManager::instance().GetHorseName(pid) == nullptr)
			db_clientdesc->DBPacket(HEADER_GD_REQ_HORSE_NAME, 0, &pid, sizeof(uint32_t));

#ifdef ENABLE_BUG_FIXES
		ch->SetHorseLevel(ch->GetHorseLevel());
		ch->SkillLevelPacket();
#endif
	}

#ifdef TEXTS_IMPROVEMENT
	if (g_noticeBattleZone) {
		if (FN_is_battle_zone(ch)) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 637, "");
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 638, "");
		}
	}
#endif

#ifdef __ENABLE_NEW_OFFLINESHOP__
	if (ch->IsPC())
	{
		offlineshop::CShop* pkShop = offlineshop::GetManager().GetShopByOwnerID(ch->GetPlayerID());
		if (pkShop)
			ch->SetOfflineShop(pkShop);

		offlineshop::CAuction* auction = offlineshop::GetManager().GetAuctionByOwnerID(ch->GetPlayerID());
		if (auction)
			ch->SetAuction(auction);
	}
#endif

#ifdef ENABLE_SWITCHBOT
	CSwitchbotManager::Instance().EnterGame(ch);
#endif


	//#ifdef ENABLE_BATTLE_PASS_STAY_ONLINE
	//	ch->LoadStayActiveBattlePass();
	//#endif
#ifdef __ENABLE_BLOCK_EXP__
	int expret = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "exp.stat");
	ch->Block_Exp = expret == 1 ? true : false;
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "manage_exp_status %d", ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "exp.stat"));
#endif

#ifdef ENABLE_MULTI_LANGUAGE
	TPacketChangeLanguage packet;
	packet.bHeader = HEADER_GC_REQUEST_CHANGE_LANGUAGE;
	packet.bLanguage = ch->GetDesc()->GetLanguage();
	ch->GetDesc()->Packet(&packet, sizeof(TPacketChangeLanguage));
#endif
#ifdef ENABLE_RUNE_SYSTEM
	ch->SetPart(PART_RUNE, ch->GetRuneEffect());
	ch->UpdatePacket();
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "rune_affect %d", ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "rune.hide_effect"));
#endif
#ifdef ENABLE_PVP_ADVANCED
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "equipview %d", ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), BLOCK_EQUIPMENT_));
#endif
#ifdef BLOCK_RIDING_INSIDE_WAR
	if (ch->GetWarMap()) {
		if (ch->IsHorseRiding()) {
			ch->StopRiding();
			ch->HorseSummon(false);
		}

		CMountSystem* mountSystem = ch->GetMountSystem();
		LPITEM mount = ch->GetWear(WEAR_COSTUME_MOUNT);
		if (mountSystem && ch->GetMountVnum() && mount) {
			mountSystem->Unmount(mount->GetValue(1));
		}
		else {
			ch->RemoveAffect(AFFECT_MOUNT);
			ch->RemoveAffect(AFFECT_MOUNT_BONUS);
		}
	}
#endif

#ifdef ENABLE_BIOLOGIST_UI
	if (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "biologist.stat") <= 15)
	{
		int biologisttime = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "biologist.time");
		biologisttime = biologisttime > 0 ? biologisttime : 1;
		ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "biologist_time %d", biologisttime);
	}
#endif

#ifdef __HIDE_COSTUME_SYSTEM__
	ch->SetBodyCostumeHidden(ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "costume_option.hide_body") == 1 ? true : false, true);
	ch->SetHairCostumeHidden(ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "costume_option.hide_hair") == 1 ? true : false, true);
#ifdef ENABLE_ACCE_SYSTEM
	ch->SetAcceCostumeHidden(ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "costume_option.hide_acce") == 1 ? true : false, true);
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	ch->SetWeaponCostumeHidden(ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "costume_option.hide_weapon") == 1 ? true : false, true);
#endif
#endif
#ifdef ENABLE_LOCKED_EXTRA_INVENTORY
	ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_EXTRA_INVENTORY1, ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "lock_extra.cat1"));
	ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_EXTRA_INVENTORY2, ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "lock_extra.cat2"));
	ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_EXTRA_INVENTORY3, ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "lock_extra.cat3"));
	ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_EXTRA_INVENTORY4, ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "lock_extra.cat4"));
	ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_EXTRA_INVENTORY5, ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "lock_extra.cat5"));
	ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_EXTRA_INVENTORY6, ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(ch), "lock_extra.cat6"));
	ecs::ChatSystem::Send(AIHelpers::EcsOf(ch), CHAT_TYPE_COMMAND, "RefreshExpandInventory");
#endif
#ifdef ENABLE_ANTICHEAT
	ch->ClearCheatChecks();
#endif
}

void CInputLogin::Empire(LPDESC d, const char* c_pData)
{
	auto p = (TPacketCGEmpire*)c_pData;

	if (EMPIRE_MAX_NUM <= p->bEmpire)
	{
		d->SetPhase(PHASE_CLOSE);
		return;
	}

	const TAccountTable& r = d->GetAccountTable();

	if (r.bEmpire != 0)
	{
		for (const auto& player : r.players)
		{
			if (0 != player.dwID)
			{
				sys_err("EmpireSelectFailed %d", player.dwID);
				return;
			}
		}
	}

	TEmpireSelectPacket pd;

	pd.dwAccountID = r.id;
	pd.bEmpire = p->bEmpire;

	db_clientdesc->DBPacket(HEADER_GD_EMPIRE_SELECT, d->GetHandle(), &pd, sizeof(pd));
}

int CInputLogin::GuildSymbolUpload(LPDESC d, const char* c_pData, uint64_t uiBytes)
{
	if (uiBytes < sizeof(TPacketCGGuildSymbolUpload))
		return -1;

	const auto* p = reinterpret_cast<const TPacketCGGuildSymbolUpload*>(c_pData);

	if (uiBytes < p->size)
		return -1;

	const int iSymbolSize = p->size - sizeof(TPacketCGGuildSymbolUpload);

	if (iSymbolSize <= 0 || static_cast<uint32_t>(iSymbolSize) > CGuildMarkManager::MAX_SYMBOL_SIZE)
	{
		sys_err("GuildSymbolUpload: invalid symbol size %d for guild %u", iSymbolSize, p->guild_id);
		d->SetPhase(PHASE_CLOSE);
		return 0;
	}

	if (!test_server)
	{
		if (!building::CManager::instance().FindLandByGuild(p->guild_id))
		{
			sys_err("GuildSymbolUpload: guild %u does not own land", p->guild_id);
			d->SetPhase(PHASE_CLOSE);
			return 0;
		}
	}

	sys_log(0, "GuildSymbolUpload: guild=%u size=%d", p->guild_id, iSymbolSize);

	CGuildMarkManager::instance().UploadSymbol(
		p->guild_id,
		iSymbolSize,
		reinterpret_cast<const uint8_t*>(c_pData + sizeof(*p)));

	CGuildMarkManager::instance().SaveSymbol(GUILD_SYMBOL_FILENAME);
	return iSymbolSize;
}

// ---------------------------------------------------------------------------
// [SERVER] GuildSymbolCRC - client sends its cached symbol CRC,
//          server compares and sends full symbol data if different
// ---------------------------------------------------------------------------
void CInputLogin::GuildSymbolCRC(LPDESC d, const char* c_pData)
{
	const auto& CGPacket = *reinterpret_cast<const TPacketCGSymbolCRC*>(c_pData);

	sys_log(0, "GuildSymbolCRC: guild=%u client_crc=%u client_size=%u",
		CGPacket.guild_id, CGPacket.crc, CGPacket.size);

	const CGuildMarkManager::TGuildSymbol* pkGS =
		CGuildMarkManager::instance().GetGuildSymbol(CGPacket.guild_id);

	if (!pkGS)
		return;

	sys_log(0, "GuildSymbolCRC: server_crc=%u server_size=%zu", pkGS->crc, pkGS->raw.size());

	// Only send if client's data differs
	if (pkGS->raw.size() == CGPacket.size && pkGS->crc == CGPacket.crc)
		return;

	if (pkGS->raw.empty())
		return;

	TPacketGCGuildSymbolData GCPacket;
	GCPacket.header = HEADER_GC_SYMBOL_DATA;
	GCPacket.size = sizeof(GCPacket) + pkGS->raw.size();
	GCPacket.guild_id = CGPacket.guild_id;

	d->BufferedPacket(&GCPacket, sizeof(GCPacket));
	d->Packet(pkGS->raw.data(), pkGS->raw.size());

	sys_log(0, "GuildSymbolCRC: sent symbol data for guild %u (%zu bytes)",
		CGPacket.guild_id, pkGS->raw.size());
}

// ---------------------------------------------------------------------------
// [SERVER] GuildMarkUpload - client sends a 16x12 guild mark image
// ---------------------------------------------------------------------------
void CInputLogin::GuildMarkUpload(LPDESC d, const char* c_pData)
{
	auto* p = (TPacketCGMarkUpload*)c_pData;

	CGuild* pkGuild = CGuildManager::instance().FindGuild(p->gid);
	if (!pkGuild)
	{
		sys_err("MARK_SERVER: GuildMarkUpload: guild not found (gid=%u)", p->gid);
		return;
	}

	if (pkGuild->GetLevel() < guild_mark_min_level)
	{
		sys_log(0, "MARK_SERVER: GuildMarkUpload: guild %u level %u < required %u",
			p->gid, pkGuild->GetLevel(), guild_mark_min_level);
		return;
	}

	sys_log(0, "MARK_SERVER: GuildMarkUpload: gid=%u", p->gid);

	// Check if mark image is completely empty (all transparent)
	const auto* pixels = reinterpret_cast<const uint32_t*>(p->image);
	bool isEmpty = true;

	for (uint32_t i = 0; i < SGuildMark::SIZE; ++i)
	{
		if (pixels[i] != 0x00000000)
		{
			isEmpty = false;
			break;
		}
	}

	CGuildMarkManager& rkMarkMgr = CGuildMarkManager::instance();

	if (isEmpty)
		rkMarkMgr.DeleteMark(p->gid);
	else
		rkMarkMgr.SaveMark(p->gid, p->image);
}

// ---------------------------------------------------------------------------
// [SERVER] GuildMarkIDXList - send full guild_id -> mark_id mapping to client
// ---------------------------------------------------------------------------
void CInputLogin::GuildMarkIDXList(LPDESC d, const char* c_pData)
{
	CGuildMarkManager& rkMarkMgr = CGuildMarkManager::instance();

	const uint32_t markCount = rkMarkMgr.GetMarkCount();
	const size_t bufSize = sizeof(uint16_t) * 2 * markCount;

	TPacketGCMarkIDXList pkt;
	pkt.header = HEADER_GC_MARK_IDXLIST;
	pkt.bufSize = sizeof(pkt) + bufSize;
	pkt.count = markCount;

	if (bufSize > 0)
	{
		// Use vector instead of raw malloc
		std::vector<char> buf(bufSize);
		rkMarkMgr.CopyMarkIdx(buf.data(), bufSize);

		d->BufferedPacket(&pkt, sizeof(pkt));
		d->LargePacket(buf.data(), bufSize);
	}
	else
	{
		d->Packet(&pkt, sizeof(pkt));
	}

	sys_log(0, "MARK_SERVER: GuildMarkIDXList: sent %u entries (%u bytes)", markCount, pkt.bufSize);
}

// ---------------------------------------------------------------------------
// [SERVER] GuildMarkCRCList - client sends block CRCs, server responds
//          with compressed data for any blocks that differ
// ---------------------------------------------------------------------------
void CInputLogin::GuildMarkCRCList(LPDESC d, const char* c_pData)
{
	const auto* pCG = reinterpret_cast<const TPacketCGMarkCRCList*>(c_pData);

	std::map<uint8_t, const SGuildMarkBlock*> mapDiffBlocks;
	CGuildMarkManager::instance().GetDiffBlocks(pCG->imgIdx, pCG->crclist, mapDiffBlocks);

	uint32_t blockCount = 0;
	TEMP_BUFFER buf(1024 * 1024);  // 1 MB scratch buffer

	for (const auto& [posBlock, pBlock] : mapDiffBlocks)
	{
		const SGuildMarkBlock& rkBlock = *pBlock;

		// Validate compressed data before sending
		if (rkBlock.m_sizeCompBuf == 0 || rkBlock.m_sizeCompBuf > SGuildMarkBlock::MAX_COMP_SIZE)
		{
			sys_err("MARK_SERVER: GuildMarkCRCList: invalid compressed size in block %u: %u bytes",
				posBlock, rkBlock.m_sizeCompBuf);
			continue;
		}

		uint8_t pos = posBlock;
		buf.write(&pos, sizeof(uint8_t));
		buf.write(&rkBlock.m_sizeCompBuf, sizeof(uint32_t));
		buf.write(rkBlock.m_abCompBuf, rkBlock.m_sizeCompBuf);

		++blockCount;
	}

	TPacketGCMarkBlock pGC;
	pGC.header = HEADER_GC_MARK_BLOCK;
	pGC.imgIdx = pCG->imgIdx;
	pGC.bufSize = buf.size() + sizeof(TPacketGCMarkBlock);
	pGC.count = blockCount;

	sys_log(0, "MARK_SERVER: GuildMarkCRCList: imgIdx=%u diff=%zu sending=%u size=%u",
		pCG->imgIdx, mapDiffBlocks.size(), blockCount, pGC.bufSize);

	if (buf.size() > 0)
	{
		d->BufferedPacket(&pGC, sizeof(TPacketGCMarkBlock));
		d->LargePacket(buf.read_peek(), buf.size());
	}
	else
	{
		d->Packet(&pGC, sizeof(TPacketGCMarkBlock));
	}
}


int CInputLogin::Analyze(LPDESC d, uint8_t bHeader, const char* c_pData)
{
	int iExtraLen = 0;

	switch (bHeader)
	{
	case HEADER_CG_PONG:
		Pong(d);
		break;

	case HEADER_CG_TIME_SYNC:
		Handshake(d, c_pData);
		break;

	case HEADER_CG_LOGIN:
		Login(d, c_pData);
		break;

	case HEADER_CG_LOGIN2:
		LoginByKey(d, c_pData);
		break;

	case HEADER_CG_CHARACTER_SELECT:
		CharacterSelect(d, c_pData);
		break;

	case HEADER_CG_CHARACTER_CREATE:
		CharacterCreate(d, c_pData);
		break;

	case HEADER_CG_CHARACTER_DELETE:
		CharacterDelete(d, c_pData);
		break;

	case HEADER_CG_ENTERGAME:
		Entergame(d, c_pData);
		break;

	case HEADER_CG_EMPIRE:
		Empire(d, c_pData);
		break;

	case HEADER_CG_MOVE:
		break;

		///////////////////////////////////////
		// Guild Mark
		/////////////////////////////////////
	case HEADER_CG_MARK_CRCLIST:
		GuildMarkCRCList(d, c_pData);
		break;

	case HEADER_CG_MARK_IDXLIST:
		GuildMarkIDXList(d, c_pData);
		break;

	case HEADER_CG_MARK_UPLOAD:
		GuildMarkUpload(d, c_pData);
		break;

		//////////////////////////////////////
		// Guild Symbol
		/////////////////////////////////////
	case HEADER_CG_GUILD_SYMBOL_UPLOAD:
		if ((iExtraLen = GuildSymbolUpload(d, c_pData, m_iBufferLeft)) < 0)
			return -1;
		break;

	case HEADER_CG_SYMBOL_CRC:
		GuildSymbolCRC(d, c_pData);
		break;
		/////////////////////////////////////
		///
	case HEADER_CG_MARK_LOGIN:
		break;

	case HEADER_CG_HACK:
		break;

	case HEADER_CG_CHANGE_NAME:
		ChangeName(d, c_pData);
		break;

	case HEADER_CG_CLIENT_VERSION:
		Version(d->GetCharacter(), c_pData);
		break;

	case HEADER_CG_CLIENT_VERSION2:
		Version(d->GetCharacter(), c_pData);
		break;
		// @fixme120
	case HEADER_CG_ITEM_USE:
	case HEADER_CG_TARGET:
		break;
	default:
		sys_err("login phase does not handle this packet! header %d", bHeader);
		//d->SetPhase(PHASE_CLOSE);
		return (0);
	}

	return (iExtraLen);
}


