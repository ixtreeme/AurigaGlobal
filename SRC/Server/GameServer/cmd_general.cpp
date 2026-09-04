#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/SkillSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include "ecs/AIHelpers.hpp"
#ifdef __FreeBSD__
#include <md5.h>
#else
#include <Core/xmd5.h>
#endif

#include "utils.h"
#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/Registry.hpp"
#include "ecs/EntityFactory.hpp"
#include "motion.h"
#include "packet.h"
#include "affect.h"
#include "pvp.h"
#include "start_position.h"
#include "party.h"
#include "guild_manager.h"
#include "p2p.h"
#include "dungeon.h"
#include "messenger_manager.h"
#include "war_map.h"
#include "questmanager.h"
#include "item_manager.h"
#include "mob_manager.h"
#include "dev_log.h"
#include <Core/Logging.hpp>
#include "item.h"
#include "arena.h"
#include "buffer_manager.h"
#include "unique_item.h"
#include "log.h"
#include <common/VnumHelper.h>
#include "shop.h"
#include "shop_manager.h"
#include <string_view>
#ifdef __NEWPET_SYSTEM__
#include "New_PetSystem.h"
#endif
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
#include "MountSystem.h"
#endif

#ifdef ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93
static std::string MakeDailyRewardHWKey(LPCHARACTER ch)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	if (!ch || !(ecs::PlayerRuntime::IsPC(chEntity)) || !ecs::PlayerRuntime::GetDesc(chEntity))
		return std::string();

	DESC* d = ecs::PlayerRuntime::GetDesc(chEntity);
	const char* hwid = d->GetHwid();
	const char* host = d->GetHostName();

	std::string key;

	// ha van HWID+HOST -> ezt hasznaljuk
	if (hwid && *hwid && host && *host)
	{
		key.reserve(128);
		key += hwid;
		key += "|";
		key += host;
		return key;
	}

	// fallback: account id (hogy ne lehessen ures kulccsal duplazni)
	key = "ACC:";
	key += std::to_string(d->GetAccountTable().id);
	return key;
}

static bool DailyReward_CheckHWIDLimit(LPCHARACTER ch)
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	if (!ch || !(ecs::PlayerRuntime::IsPC(chEntity)) || !ecs::PlayerRuntime::GetDesc(chEntity))
		return false;

	std::string key = MakeDailyRewardHWKey(ch);

	// 1) ma mar claimelte valaki ezen a kulcson?
	std::unique_ptr<SQLMsg> chk(DBManager::instance().DirectQuery(
		"SELECT 1 FROM player.daily_reward_claim_hwid WHERE hwkey='%s' AND claim_day=CURDATE() LIMIT 1",
		key.c_str()
	));

	if (chk && chk->Get()->uiNumRows > 0)
		return false;

	// 2) ha nem, akkor regisztraljuk (mostantol ma mar nem lehet ujra)
	std::unique_ptr<SQLMsg>(DBManager::Instance().DirectQuery(
		"INSERT INTO player.daily_reward_claim_hwid (hwkey, claim_day, pid, account_id) "
		"VALUES('%s', CURDATE(), %u, %u)",
		key.c_str(),
		(ecs::PlayerRuntime::GetPlayerID(chEntity)),
		ecs::PlayerRuntime::GetDesc(chEntity)->GetAccountTable().id
	));

	return true;
}
#endif

ACMD(do_user_horse_ride)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ecs::PlayerRuntime::IsObserverMode(character))
		return;

	if (CombatSystem::IsDead(character) || CombatSystem::IsStun(character))
		return;

	if (ch->IsHorseRiding() == false)
	{
		if (MountSystem::GetMountVnum(character)) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 532, "");
#endif
			return;
		}

		if (ch->GetHorse() == nullptr)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 332, "");
#endif
			return;
		}

		ch->StartRiding();
	}
	else
	{
		ch->StopRiding();
	}
}
ACMD(do_daily_reward_reload){
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch) {
		return;
	}

	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ManagerGiftSystem DeleteRewards|");
	std::string time = "";
	std::string rewards = "";

	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT UNIX_TIMESTAMP(time), reward FROM player.daily_reward_status WHERE pid = %u", (ecs::PlayerRuntime::GetPlayerID(character))));
	if (msg->Get()->uiNumRows > 0) {
		std::unique_ptr<SQLMsg> msg2(DBManager::instance().DirectQuery("SELECT UNIX_TIMESTAMP(time),reward FROM player.daily_reward_status WHERE pid = %u and (time + INTERVAL 1 DAY < NOW()) limit 1;", (ecs::PlayerRuntime::GetPlayerID(character))));
		if (msg2->Get()->uiNumRows > 0) {
			std::unique_ptr<SQLMsg>(DBManager::Instance().DirectQuery("DELETE FROM player.daily_reward_status WHERE pid = %u", (ecs::PlayerRuntime::GetPlayerID(character))));
			std::unique_ptr<SQLMsg>(DBManager::Instance().DirectQuery("INSERT INTO player.daily_reward_status (pid, time, reward, total_rewards) VALUES(%u, NOW(), 0, 0)", (ecs::PlayerRuntime::GetPlayerID(character))));
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 721, "");
#endif
			std::unique_ptr<SQLMsg> msg3(DBManager::instance().DirectQuery("SELECT UNIX_TIMESTAMP(time), reward FROM player.daily_reward_status WHERE pid = %u", (ecs::PlayerRuntime::GetPlayerID(character))));
			if (msg3->Get()->uiNumRows > 0) {
				MYSQL_ROW row;
				while ((row = mysql_fetch_row(msg3->Get()->pSQLResult)) != nullptr) {
					time = row[0];
					rewards = row[1];
				}
			}
		} else {
			std::unique_ptr<SQLMsg> msg3(DBManager::instance().DirectQuery("SELECT UNIX_TIMESTAMP(time), reward FROM player.daily_reward_status WHERE pid = %u", (ecs::PlayerRuntime::GetPlayerID(character))));
			if (msg3->Get()->uiNumRows > 0) {
				MYSQL_ROW row;
				while ((row = mysql_fetch_row(msg3->Get()->pSQLResult)) != nullptr) {
					time = row[0];
					rewards = row[1];
				}
			}
		}
	} else {
		std::unique_ptr<SQLMsg>(DBManager::Instance().DirectQuery("INSERT INTO player.daily_reward_status (pid, time, reward, total_rewards) VALUES(%u, NOW(), 0, 0)", (ecs::PlayerRuntime::GetPlayerID(character))));

		std::unique_ptr<SQLMsg> msg2(DBManager::instance().DirectQuery("SELECT UNIX_TIMESTAMP(time), reward FROM player.daily_reward_status WHERE pid = %u", (ecs::PlayerRuntime::GetPlayerID(character))));
		if (msg2->Get()->uiNumRows > 0) {
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(msg2->Get()->pSQLResult)) != nullptr) {
				time = row[0];
				rewards = row[1];
			}
		}
	}

	std::unique_ptr<SQLMsg> msgend(DBManager::instance().DirectQuery("SELECT items, count FROM player.daily_reward_items WHERE reward = '%s'", rewards.c_str()));
	if (msgend->Get()->uiNumRows > 0) {
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(msgend->Get()->pSQLResult)) != nullptr) {
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ManagerGiftSystem SetReward|%s|%s", row[0], row[1]);
		}
	}

	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ManagerGiftSystem SetTime|%s", time.c_str());
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ManagerGiftSystem SetDailyReward|%s", rewards.c_str());
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ManagerGiftSystem SetRewardDone|");
}

ACMD(do_daily_reward_get_reward){
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch)
		return;

	std::string items = "";
	bool reward = false;
	std::string rewards = "";
	// and (NOW() - interval 30 minute > time)

	std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT reward from player.daily_reward_status where (NOW() > time) and pid = %u", (ecs::PlayerRuntime::GetPlayerID(character))));
	if (msg->Get()->uiNumRows > 0) {
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(msg->Get()->pSQLResult)) != nullptr) {
			rewards = row[0];
		}

		reward = true;
	}

	if (reward) {
#ifdef ENABLE_DAILY_REWARD_HWID_LIMIT_RAZOR93
		// HWID limit: 1 gep / nap
		if (!DailyReward_CheckHWIDLimit(ch))
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Napi 1 jutalom jar. // You have already collected the reward.");
			return;
		}
#endif
		std::string counts = "";

		std::unique_ptr<SQLMsg> msg2(DBManager::instance().DirectQuery("SELECT items, count from player.daily_reward_items where reward = '%s' ORDER BY RAND() limit 1", rewards.c_str()));
		if (msg2->Get()->uiNumRows > 0) {
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(msg2->Get()->pSQLResult)) != nullptr) {
				items = row[0];
				counts = row[1];
			}
		}

		uint32_t item = 0;
		uint32_t count = 0;

		str_to_number(item, items.c_str());
		str_to_number(count, counts.c_str());
		ItemSystem::AutoGiveItemEcs(character, item, count);
		std::unique_ptr<SQLMsg>(DBManager::Instance().DirectQuery("UPDATE daily_reward_status SET reward = CASE WHEN reward = 0 THEN '1' WHEN reward = 1 THEN '2' WHEN reward = 2 THEN '3' WHEN reward = 3 THEN '4' WHEN reward = 4 THEN '5' WHEN reward = 5 THEN '6' WHEN reward = 6 THEN '0' END, total_rewards = total_rewards +1, time = (NOW() + interval 1 day) WHERE pid = %u", (ecs::PlayerRuntime::GetPlayerID(character))));
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 715, "");
	}
#endif
}
ACMD(do_user_horse_back)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch)
		return;

	CMountSystem* mountSystem = ch->GetMountSystem();
	if (mountSystem) {
		if ((mountSystem->CountSummoned() > 0) || MountSystem::GetMountVnum(character)) {
			const entt::entity owner = character;
			const entt::entity item = ItemSystem::GetWearItem(owner, WEAR_COSTUME_MOUNT);
			if (item != entt::null) {
				ItemSystem::UnequipItemEcs(owner, item);
				return;
			}
		}
	}

	if (ch->GetHorse() != nullptr)
	{
		ch->HorseSummon(false);
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 331, "");
#endif
	}
	else if (ch->IsHorseRiding() == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 330, "");
#endif
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 332, "");
#endif
	}
}

ACMD(do_user_horse_feed)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	// λ  ¿  ̸   .
	if (ch->GetMyShop())
		return;

	if (ch->GetHorse() == nullptr)
	{
#ifdef TEXTS_IMPROVEMENT
		if (ch->IsHorseRiding() == false) {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 332, "");
		}
		else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 336, "");
		}
#endif
		return;
	}

	uint32_t dwFood = ch->GetHorseGrade() + 50054 - 1;


	if (ch->CountSpecifyItem(dwFood) > 0)
	{
		ch->RemoveSpecifyItem(dwFood, 1);
		ch->FeedHorse();
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 112, "%s",
#ifdef ENABLE_MULTI_NAMES
		ITEM_MANAGER::instance().GetTable(dwFood)->szLocaleName[ecs::PlayerRuntime::GetDesc(character)->GetLanguage()]
#else
		ITEM_MANAGER::instance().GetTable(dwFood)->szLocaleName
#endif
		);
#endif
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 111, "%s",
#ifdef ENABLE_MULTI_NAMES
		ITEM_MANAGER::instance().GetTable(dwFood)->szLocaleName[ecs::PlayerRuntime::GetDesc(character)->GetLanguage()]
#else
		ITEM_MANAGER::instance().GetTable(dwFood)->szLocaleName
#endif
		);
#endif
	}

}

#define MAX_REASON_LEN		128

EVENTINFO(TimedEventInfo)
{
	entt::entity ch { entt::null };
	int		subcmd;
	int         	left_second;
	char		szReason[MAX_REASON_LEN];

	TimedEventInfo()
	: ch()
	, subcmd( 0 )
	, left_second( 0 )
	{
		::memset( szReason, 0, MAX_REASON_LEN );
	}
};

struct SendDisconnectFunc
{
	void operator () (LPDESC d)
	{
		if (d->GetCharacter())
		{
			if ((ecs::PlayerRuntime::GetGMLevel(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null))) == GM_PLAYER)
				ecs::ChatSystem::Send(((d->GetCharacter()) ? (d->GetCharacter())->GetEntityHandle() : entt::null), CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
		}
	}
};

struct DisconnectFunc
{
	void operator () (LPDESC d)
	{
		if (d->GetType() == DESC_TYPE_CONNECTOR)
			return;

		if (d->IsPhase(PHASE_P2P))
			return;

		if (d->GetCharacter())
			d->GetCharacter()->Disconnect("Shutdown(DisconnectFunc)");

		d->SetPhase(PHASE_CLOSE);
	}
};

EVENTINFO(shutdown_event_data)
{
	int seconds;

	shutdown_event_data()
	: seconds( 0 )
	{
	}
};

EVENTFUNC(shutdown_event)
{
	shutdown_event_data* info = dynamic_cast<shutdown_event_data*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("shutdown_event> <Factor> Null pointer");
		return 0;
	}

	int * pSec = & (info->seconds);

	if (*pSec < 0)
	{
		LOG_INFO("shutdown_event sec {}", *pSec);

		if (--*pSec == -10)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), DisconnectFunc());
			return passes_per_sec;
		}
		else if (*pSec < -10)
			return 0;

		return passes_per_sec;
	}
	else if (*pSec == 0)
	{
		const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
		std::for_each(c_set_desc.begin(), c_set_desc.end(), SendDisconnectFunc());
		g_bNoMoreClient = true;
		--*pSec;
		return passes_per_sec;
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		SendNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 577, "%d", *pSec);
#endif
		--*pSec;
		return passes_per_sec;
	}
}

void Shutdown(int iSec)
{
	if (g_bNoMoreClient)
	{
		thecore_shutdown();
		return;
	}

	CWarMapManager::instance().OnShutdown();
#ifdef TEXTS_IMPROVEMENT
	SendNoticeNew(CHAT_TYPE_NOTICE, 0, 0, 578, "%d", iSec);
#endif
	shutdown_event_data* info = AllocEventInfo<shutdown_event_data>();
	info->seconds = iSec;

	event_create(shutdown_event, info, 1);
}

ACMD(do_shutdown)
{
	if (!ecs::PlayerRuntime::IsValid(character))
	{
		LOG_ERROR("Accept shutdown command from {}.", ecs::PlayerRuntime::GetName(character).data());
	}
	TPacketGGShutdown p;
	p.bHeader = HEADER_GG_SHUTDOWN;
	P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGShutdown));

	Shutdown(10);
}


#ifdef ENABLE_CHANNEL_SWITCH_SYSTEM
ACMD(do_change_channel)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch)
		return;

	if (ch->IsWarping())
	{
		return;
	}

	if (!ecs::PlayerRuntime::CanWarp(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 234, "10");
#endif
		return;
	}

	if (ch->GetTimedEvent())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 482, "");
#endif
		event_cancel(&ch->GetTimedEventRef());
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 716, "");
#endif
		return;
	}

	if (g_bChannel == 99)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 719, "");
#endif
		return;
	}

	int32_t channel;
	str_to_number(channel, arg1);

	if (channel < 1 || channel > 6)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 717, "");
#endif
		return;
	}

	if (channel == g_bChannel)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 718, "%d", g_bChannel);
#endif
		return;
	}

	if (ch->GetDungeon())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 720, "");
#endif
		return;
	}

	TPacketChangeChannel p;
	p.channel = channel;
	p.lMapIndex = ecs::PlayerRuntime::GetMapIndex(character);

	db_clientdesc->DBPacket(HEADER_GD_FIND_CHANNEL, ecs::PlayerRuntime::GetDesc(character)->GetHandle(), &p, sizeof(p));
}
#endif

#ifdef ENABLE_SORT_INVEN
ACMD(do_item_check)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->EditMyInven();
}


ACMD(do_sort_extra_inventory)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->EditMyExtraInven();
}

#endif

EVENTFUNC(timed_event)
{
	TimedEventInfo * info = dynamic_cast<TimedEventInfo *>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("timed_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER	ch = ecs::LegacyCharOf(info->ch);
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;

	if (ch == nullptr) { // <Factor>
		return 0;
	}
	LPDESC d = ecs::PlayerRuntime::GetDesc(chEntity);

	if (info->left_second <= 0)
	{
		ch->GetTimedEventRef() = nullptr;

		switch (info->subcmd)
		{
			case SCMD_LOGOUT:
			case SCMD_QUIT:
			case SCMD_PHASE_SELECT:
				{
					TPacketNeedLoginLogInfo acc_info;
					acc_info.dwPlayerID = ecs::PlayerRuntime::GetDesc(chEntity)->GetAccountTable().id;

					db_clientdesc->DBPacket( HEADER_GD_VALID_LOGOUT, 0, &acc_info, sizeof(acc_info) );

					LogManager::instance().DetailLoginLog( false, ch );
				}
				break;
		}

		switch (info->subcmd)
		{
			case SCMD_LOGOUT:
				if (d)
					d->SetPhase(PHASE_CLOSE);
				break;

			case SCMD_QUIT:
				ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "quit");
				break;

			case SCMD_PHASE_SELECT:
				{
					ch->Disconnect("timed_event - SCMD_PHASE_SELECT");

					if (d)
					{
						d->SetPhase(PHASE_SELECT);
					}
				}
				break;
		}

		return 0;
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 103, "%d", info->left_second);
#endif
		--info->left_second;
	}

	return PASSES_PER_SEC(1);
}

ACMD(do_cmd)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ch->GetTimedEvent())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 482, "");
#endif
		event_cancel(&ch->GetTimedEventRef());
		return;
	}

#ifdef TEXTS_IMPROVEMENT
	switch (subcmd)
	{
		case SCMD_LOGOUT:
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 326, "");
			break;
		case SCMD_QUIT:
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 240, "");
			break;
		case SCMD_PHASE_SELECT:
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 483, "");
#endif
			break;
	}
#endif

	int nExitLimitTime = 10;

	if (ch->IsHack(false, true, nExitLimitTime) && (!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG)) {
		return;
	}

	switch (subcmd)
	{
		case SCMD_LOGOUT:
		case SCMD_QUIT:
		case SCMD_PHASE_SELECT:
			{
				TimedEventInfo* info = AllocEventInfo<TimedEventInfo>();

				{
					if (ch->IsPosition(POS_FIGHTING))
						info->left_second = 10;
					else
						info->left_second = 3;
				}

				info->ch		= ch->GetEntityHandle();
				info->subcmd		= subcmd;
				strlcpy(info->szReason, argument, sizeof(info->szReason));

				ch->GetTimedEventRef()	= event_create(timed_event, info, 1);
			}
			break;
	}
}

ACMD(do_fishing)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	ch->SetRotation(atof(arg1));
	ch->fishing();
}

ACMD(do_console)
{
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ConsoleEnable");
}

ACMD(do_restart)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!(ecs::PlayerRuntime::IsPC(character)) || ch->GetPosition() != POS_DEAD)
	{
		return;
	}

	if (ch->IsHack())
	{
		if (subcmd == SCMD_RESTART_TOWN)
		{
			return;
		}
		else if (subcmd != SCMD_RESTART_TOWN && (!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG))
		{
			return;
		}
	}

	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "CloseRestartWindow");

	ecs::PlayerRuntime::GetDesc(character)->SetPhase(PHASE_GAME);
	ch->SetPosition(POS_STANDING);
	ch->StartRecoveryEvent();


	int32_t mapidx = ecs::PlayerRuntime::GetMapIndex(character);

	if (ch->GetWarMap() && !ecs::PlayerRuntime::IsObserverMode(character))
	{
		CWarMap * pMap = ch->GetWarMap();
		uint32_t dwGuildOpponent = pMap ? pMap->GetGuildOpponent(ch) : 0;
		if (dwGuildOpponent)
		{
			switch (subcmd)
			{
				case SCMD_RESTART_TOWN:
					{
						LOG_INFO("do_restart: restart town");

						PIXEL_POSITION pos;
						if (CWarMapManager::instance().GetStartPosition(mapidx, ecs::SocialSystem::GetGuild(character)->GetID() < dwGuildOpponent ? 0 : 1, pos))
						{
							ecs::MovementSystem::Show(character, mapidx, pos.x, pos.y);
						}
						else
						{
							ecs::MovementSystem::ExitToSavedLocation(character);
						}

						ecs::PointSystem::Change(character, POINT_HP, ecs::PointSystem::GetMaxHP(character) - ch->GetHP());
						ecs::PointSystem::Change(character, POINT_SP, ecs::PointSystem::GetMaxSP(character) - ch->GetSP());
						ch->ReviveInvisible(5);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
						ch->CheckMount();
#endif
					}
					break;

				case SCMD_RESTART_HERE:
					{
						LOG_INFO("do_restart: restart here");
						ch->RestartAtSamePos();
						ecs::PointSystem::Change(character, POINT_HP, ecs::PointSystem::GetMaxHP(character) - ch->GetHP());
						ecs::PointSystem::Change(character, POINT_SP, ecs::PointSystem::GetMaxSP(character) - ch->GetSP());
						ch->ReviveInvisible(5);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
						ch->CheckMount();
#endif
#ifdef ENABLE_SKILLS_BUFF_ALTERNATIVE
						ch->LoadAffectSkills();
#endif
					}
					break;
				default:
					{
						LOG_ERROR("do_restart: unknown method for {}", ecs::PlayerRuntime::GetName(character).data());
					}
					break;
			}

			return;
		}
	}

	switch (subcmd)
	{
		case SCMD_RESTART_TOWN:
			{
				LOG_INFO("do_restart: restart town");

				bool wasDungeon = false;
				bool showed = false;
				if (mapidx >= 10000)
				{
					LPDUNGEON dungeon = CDungeonManager::instance().FindByMapIndex(mapidx);
					if (dungeon)
					{
						if (mapidx >= 2160000 && mapidx < 2170000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							switch (floor)
							{
								case 1:
									{
										ecs::MovementSystem::Show(character, mapidx, 445000, 1228200);
										showed = true;
									}
									break;
								case 2:
									{
										ecs::MovementSystem::Show(character, mapidx, 391700, 1293200);
										showed = true;
									}
									break;
								case 3:
									{
										ecs::MovementSystem::Show(character, mapidx, 443400, 1269800);
										showed = true;
									}
									break;
								case 4:
									{
										ecs::MovementSystem::Show(character, mapidx, 314700, 1318700);
										showed = true;
									}
									break;
								default:
									{
										ecs::MovementSystem::ExitToSavedLocation(character);
									}
									break;
							}
						}
						else if (mapidx >= 2080000 && mapidx < 2090000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							if (floor != 0)
							{
								ecs::MovementSystem::Show(character, mapidx, 843500, 1066800);
								showed = true;
							}
							else
							{
								ecs::MovementSystem::ExitToSavedLocation(character);
							}
						}
						else if (mapidx >= 2170000 && mapidx < 2180000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							if (floor != 0)
							{
								ecs::MovementSystem::Show(character, mapidx, 87900, 614700);
								showed = true;
							}
							else
							{
								ecs::MovementSystem::ExitToSavedLocation(character);
							}
						}
						else if (mapidx >= 3550000 && mapidx < 3560000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							switch (floor)
							{
								case 1:
									{
										ecs::MovementSystem::Show(character, mapidx, 216600, 266700);
										showed = true;
									}
									break;
								case 2:
									{
										ecs::MovementSystem::Show(character, mapidx, 218600, 348900);
										showed = true;
									}
									break;
								default:
									{
										ecs::MovementSystem::ExitToSavedLocation(character);
									}
									break;
							}
						}
						else if (mapidx >= 3530000 && mapidx < 3540000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							if (floor != 0)
							{
								ecs::MovementSystem::Show(character, mapidx, 166500, 522100);
								showed = true;
							}
							else
							{
								ecs::MovementSystem::ExitToSavedLocation(character);
							}
						}
						else if (mapidx >= 3520000 && mapidx < 3530000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							switch (floor)
							{
								case 1:
								case 2:
									{
										ecs::MovementSystem::Show(character, mapidx, 588100, 180400);
										showed = true;
									}
									break;
								case 3:
									{
										ecs::MovementSystem::Show(character, mapidx, 554000, 207000);
										showed = true;
									}
									break;
								case 4:
								case 5:
									{
										ecs::MovementSystem::Show(character, mapidx, 569100, 223000);
										showed = true;
									}
									break;
								case 6:
									{
										ecs::MovementSystem::Show(character, mapidx, 586600, 206800);
										showed = true;
									}
									break;
								case 7:
								case 8:
									{
										ecs::MovementSystem::Show(character, mapidx, 596900, 222500);
										showed = true;
									}
									break;
								case 9:
									{
										ecs::MovementSystem::Show(character, mapidx, 604700, 192600);
										showed = true;
									}
									break;
								default:
									{
										ecs::MovementSystem::ExitToSavedLocation(character);
									}
									break;
							}
						}
						else if (mapidx >= 3570000 && mapidx < 3580000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							switch (floor)
							{
								case 1:
									{
										ecs::MovementSystem::Show(character, mapidx, 905100, 2261700);
										showed = true;
									}
									break;
								case 2:
								case 3:
									{
										ecs::MovementSystem::Show(character, mapidx, 926600, 2262100);
										showed = true;
									}
									break;
								case 4:
								case 5:
									{
										ecs::MovementSystem::Show(character, mapidx, 953600, 2260800);
										showed = true;
									}
									break;
								case 6:
									{
										ecs::MovementSystem::Show(character, mapidx, 913700, 2355800);
										showed = true;
									}
									break;
								case 7:
									{
										ecs::MovementSystem::Show(character, mapidx, 975900, 2365500);
										showed = true;
									}
									break;
								default:
									{
										ecs::MovementSystem::ExitToSavedLocation(character);
									}
									break;
							}
						}
						else if (mapidx >= 3510000 && mapidx < 3520000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							switch (floor)
							{
								case 1:
								case 2:
								case 3:
								case 4:
								case 5:
								case 6:
								case 7:
									{
										ecs::MovementSystem::Show(character, mapidx, 776600, 671900);
										showed = true;
									}
									break;
								case 8:
									{
										ecs::MovementSystem::Show(character, mapidx, 810900, 686700);
										showed = true;
									}
									break;
								default:
									{
										ecs::MovementSystem::ExitToSavedLocation(character);
									}
									break;
							}
						}
						else if (mapidx >= 2180000 && mapidx < 2190000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							switch (floor)
							{
								case 1:
									{
										ecs::MovementSystem::Show(character, mapidx, 624500, 1415200);
										showed = true;
									}
									break;
								case 2:
									{
										ecs::MovementSystem::Show(character, mapidx, 627300, 1446800);
										showed = true;
									}
									break;
								case 3:
									{
										ecs::MovementSystem::Show(character, mapidx, 673200, 1444000);
										showed = true;
									}
									break;
								case 4:
									{
										ecs::MovementSystem::Show(character, mapidx, 655400, 1421200);
										showed = true;
									}
									break;
								case 5:
									{
										ecs::MovementSystem::Show(character, mapidx, 695500, 1421300);
										showed = true;
									}
									break;
								default:
									{
										ecs::MovementSystem::ExitToSavedLocation(character);
									}
									break;
							}
						}
						else if (mapidx >= 270000 && mapidx < 280000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							if (floor != 0)
							{
								ecs::MovementSystem::Show(character, mapidx, 942000, 127700);
								showed = true;
							}
							else
							{
								ecs::MovementSystem::ExitToSavedLocation(character);
							}
						}
						else if (mapidx >= 2090000 && mapidx < 2100000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							if (floor != 0)
							{
								ecs::MovementSystem::Show(character, mapidx, 853700, 1416400);
								showed = true;
							}
							else
							{
								ecs::MovementSystem::ExitToSavedLocation(character);
							}
						}
						else if (mapidx >= 2100000 && mapidx < 2110000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							if (floor != 0)
							{
								ecs::MovementSystem::Show(character, mapidx, 782400, 1502100);
								showed = true;
							}
							else
							{
								ecs::MovementSystem::ExitToSavedLocation(character);
							}
						}
						else if (mapidx >= 660000 && mapidx < 670000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							switch (floor)
							{
								case 1:
									{
										ecs::MovementSystem::Show(character, mapidx, 377400, 2704000);
										showed = true;
									}
									break;
								case 2:
									{
										ecs::MovementSystem::Show(character, mapidx, 378200, 2680300);
										showed = true;
									}
									break;
								case 3:
								case 4:
									{
										ecs::MovementSystem::Show(character, mapidx, 401700, 2728500);
										showed = true;
									}
									break;
								case 5:
									{
										ecs::MovementSystem::Show(character, mapidx, 401700, 2705700);
										showed = true;
									}
									break;
								case 6:
								case 7:
									{
										ecs::MovementSystem::Show(character, mapidx, 402200, 2682300);
										showed = true;
									}
									break;
								case 8:
								case 9:
								case 10:
									{
										ecs::MovementSystem::Show(character, mapidx, 423800, 2729400);
										showed = true;
									}
									break;
								case 11:
								case 12:
									{
										ecs::MovementSystem::Show(character, mapidx, 423800, 2705900);
										showed = true;
									}
									break;
								case 13:
									{
										ecs::MovementSystem::Show(character, mapidx, 423800, 2681100);
										showed = true;
									}
									break;
								default:
									{
										ecs::MovementSystem::ExitToSavedLocation(character);
									}
									break;
							}
						}
						else if (mapidx >= 670000 && mapidx < 680000)//under_water_zone
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							switch (floor)
							{
								case 1:
									{
										ecs::MovementSystem::Show(character, mapidx, 486400, 25600);
										showed = true;
									}
									break;
								case 2:
									{
										ecs::MovementSystem::Show(character, mapidx, 502500, 70300);
										showed = true;
									}
									break;
								case 3:
								case 4:
									{
										ecs::MovementSystem::Show(character, mapidx, 524300, 43500);
										showed = true;
									}
									break;
								case 5:
									{
										ecs::MovementSystem::Show(character, mapidx, 523500, 67000);
										showed = true;
									}
									break;
								case 6:
								case 7:
									{
										ecs::MovementSystem::Show(character, mapidx, 546900, 68700);
										showed = true;
									}
									break;
								case 8:
								case 9:
								case 10:
									{
										ecs::MovementSystem::Show(character, mapidx, 549400, 45600);
										showed = true;
									}
									break;
								case 11:
								case 12:
									{
										ecs::MovementSystem::Show(character, mapidx, 528900, 46200);
										showed = true;
									}
									break;
								case 13:
									{
										ecs::MovementSystem::Show(character, mapidx, 506400, 45600);
										showed = true;
									}
									break;
								default:
									{
										ecs::MovementSystem::ExitToSavedLocation(character);
									}
									break;
							}
						}
						else if (mapidx >= 3560000 && mapidx < 3570000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							if (floor != 0)
							{
								ecs::MovementSystem::Show(character, mapidx, 1528200, 2318700);
								showed = true;
							}
							else
							{
								ecs::MovementSystem::ExitToSavedLocation(character);
							}
						}
						else if (mapidx >= 2120000 && mapidx < 2130000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							if (floor != 0)
							{
								ecs::MovementSystem::Show(character, mapidx, 320000, 1529000);
								showed = true;
							}
							else
							{
								ecs::MovementSystem::ExitToSavedLocation(character);
							}
						}
						else if (mapidx >= 260000 && mapidx < 270000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							switch (floor)
							{
								case 1:
									{
										ecs::MovementSystem::Show(character, mapidx, 54500, 2268000);
										showed = true;
									}
									break;
								case 2:
									{
										ecs::MovementSystem::Show(character, mapidx, 19400, 2306600);
										showed = true;
									}
									break;
								case 3:
									{
										ecs::MovementSystem::Show(character, mapidx, 110500, 2295900);
										showed = true;
									}
									break;
								default:
									{
										ecs::MovementSystem::ExitToSavedLocation(character);
									}
									break;
							}
						}
						else if (mapidx >= 250000 && mapidx < 260000)
						{
							wasDungeon = true;
							int32_t floor = dungeon->GetFlag("floor");
							switch (floor)
							{
								case 1:
									{
										ecs::MovementSystem::Show(character, mapidx, 2320600, 3077800);
										showed = true;
									}
									break;
								case 2:
									{
										ecs::MovementSystem::Show(character, mapidx, 2351500, 3141500);
										showed = true;
									}
									break;
								default:
									{
										ecs::MovementSystem::ExitToSavedLocation(character);
									}
									break;
							}
						}
					}
				}

				if (!wasDungeon)
				{
					PIXEL_POSITION pos;
					if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(mapidx, (ecs::PlayerRuntime::GetEmpire(character)), pos))
					{
						ecs::MovementSystem::WarpSet(character, pos.x, pos.y);
					}
					else
					{
						ecs::MovementSystem::WarpSet(character, EMPIRE_START_X((ecs::PlayerRuntime::GetEmpire(character))), EMPIRE_START_Y((ecs::PlayerRuntime::GetEmpire(character))));
					}
				}

				ecs::PointSystem::Change(character, POINT_HP, ecs::PointSystem::GetMaxHP(character) - ch->GetHP());
				ecs::PointSystem::Change(character, POINT_SP, ecs::PointSystem::GetMaxSP(character) - ch->GetSP());
				ch->DeathPenalty(1);
				if (showed)
				{
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
					ch->CheckMount();
#endif
#ifdef ENABLE_SKILLS_BUFF_ALTERNATIVE
					ch->LoadAffectSkills();
#endif
				}
			}
			break;
		case SCMD_RESTART_HERE:
			{
				LOG_INFO("do_restart: restart here");

				ch->RestartAtSamePos();
#ifdef ENABLE_REVIVE_WITH_HALF_HP_IF_MONSTER_KILLED_YOU
				ecs::PointSystem::Change(character, POINT_HP, ch->GetDeadByMonster() ? (ecs::PointSystem::GetMaxHP(character) - ch->GetHP()) / 2 : 50 - ch->GetHP());
#else
				ecs::PointSystem::Change(character, POINT_HP, 50 - ch->GetHP());
#endif
				ecs::PointSystem::Change(character, POINT_SP, ecs::PointSystem::GetMaxSP(character) - ch->GetSP());
				ch->DeathPenalty(0);
				ch->ReviveInvisible(5);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
				ch->CheckMount();
#endif
#ifdef ENABLE_SKILLS_BUFF_ALTERNATIVE
				ch->LoadAffectSkills();
#endif
			}
			break;
		default:
			{
				LOG_ERROR("do_restart: unknown method for {}", ecs::PlayerRuntime::GetName(character).data());
			}
			break;
	}
}

#define MAX_STAT g_iStatusPointSetMaxValue

ACMD(do_stat_reset)
{
	ecs::PointSystem::Change(character, POINT_STAT_RESET_COUNT, 12 - ecs::PointSystem::Get(character, POINT_STAT_RESET_COUNT));
}

ACMD(do_stat_minus)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (ch->IsPolymorphed())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 312, "");
#endif
		return;
	}

	if (ecs::PointSystem::Get(character, POINT_STAT_RESET_COUNT) <= 0)
		return;

	if (!strcmp(arg1, "st"))
	{
		if (ecs::PointSystem::GetReal(character, POINT_ST) <= JobInitialPoints[ch->GetJob()].st)
			return;

		ch->SetRealPoint(POINT_ST, ecs::PointSystem::GetReal(character, POINT_ST) - 1);
		ch->SetPoint(POINT_ST, ecs::PointSystem::Get(character, POINT_ST) - 1);
		ch->ComputePoints();
		ecs::PointSystem::Change(character, POINT_ST, 0);
	}
	else if (!strcmp(arg1, "dx"))
	{
		if (ecs::PointSystem::GetReal(character, POINT_DX) <= JobInitialPoints[ch->GetJob()].dx)
			return;

		ch->SetRealPoint(POINT_DX, ecs::PointSystem::GetReal(character, POINT_DX) - 1);
		ch->SetPoint(POINT_DX, ecs::PointSystem::Get(character, POINT_DX) - 1);
		ch->ComputePoints();
		ecs::PointSystem::Change(character, POINT_DX, 0);
	}
	else if (!strcmp(arg1, "ht"))
	{
		if (ecs::PointSystem::GetReal(character, POINT_HT) <= JobInitialPoints[ch->GetJob()].ht)
			return;

		ch->SetRealPoint(POINT_HT, ecs::PointSystem::GetReal(character, POINT_HT) - 1);
		ch->SetPoint(POINT_HT, ecs::PointSystem::Get(character, POINT_HT) - 1);
		ch->ComputePoints();
		ecs::PointSystem::Change(character, POINT_HT, 0);
		ecs::PointSystem::Change(character, POINT_MAX_HP, 0);
	}
	else if (!strcmp(arg1, "iq"))
	{
		if (ecs::PointSystem::GetReal(character, POINT_IQ) <= JobInitialPoints[ch->GetJob()].iq)
			return;

		ch->SetRealPoint(POINT_IQ, ecs::PointSystem::GetReal(character, POINT_IQ) - 1);
		ch->SetPoint(POINT_IQ, ecs::PointSystem::Get(character, POINT_IQ) - 1);
		ch->ComputePoints();
		ecs::PointSystem::Change(character, POINT_IQ, 0);
		ecs::PointSystem::Change(character, POINT_MAX_SP, 0);
	}
	else
		return;

	ecs::PointSystem::Change(character, POINT_STAT, +1);
	ecs::PointSystem::Change(character, POINT_STAT_RESET_COUNT, -1);
	ch->ComputePoints();
}

ACMD(do_stat)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (ch->IsPolymorphed())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 312, "");
#endif
		return;
	}

	if (ecs::PointSystem::Get(character, POINT_STAT) <= 0)
		return;

	uint8_t idx = 0;

	if (!strcmp(arg1, "st"))
		idx = POINT_ST;
	else if (!strcmp(arg1, "dx"))
		idx = POINT_DX;
	else if (!strcmp(arg1, "ht"))
		idx = POINT_HT;
	else if (!strcmp(arg1, "iq"))
		idx = POINT_IQ;
	else
		return;

	if (ecs::PointSystem::GetReal(character, idx) >= MAX_STAT)
		return;

	ch->SetRealPoint(idx, ecs::PointSystem::GetReal(character, idx) + 1);
	ch->SetPoint(idx, ecs::PointSystem::Get(character, idx) + 1);
	ch->ComputePoints();
	ecs::PointSystem::Change(character, idx, 0);

	if (idx == POINT_IQ)
	{
		ecs::PointSystem::Change(character, POINT_MAX_HP, 0);
	}
	else if (idx == POINT_HT)
	{
		ecs::PointSystem::Change(character, POINT_MAX_SP, 0);
	}

	ecs::PointSystem::Change(character, POINT_STAT, -1);
	ch->ComputePoints();
}

#ifdef ENABLE_PVP_ADVANCED
#include <string>
#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
const char* szTableStaticPvP[] = {BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT};

ACMD(do_pvp)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);

	if (!ch)
		return;

	if (ch->GetArena() != nullptr || CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(character)) == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	char arg1[256], arg2[256], arg3[256], arg4[256], arg5[256], arg6[256], arg7[256], arg8[256], arg9[256], arg10[256];

	pvp_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2), arg3, sizeof(arg3), arg4, sizeof(arg4), arg5, sizeof(arg5), arg6, sizeof(arg6), arg7, sizeof(arg7), arg8, sizeof(arg8), arg9, sizeof(arg9), arg10, sizeof(arg10));

	uint32_t vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(vid);
	const entt::entity victim = pkVictim ? pkVictim->GetEntityHandle() : entt::null;

	//// Fake PC / non-real target => ignore
	//if (pkVictim->IsFakePlayer() || !ecs::PlayerRuntime::GetDesc(((pkVictim) ? (pkVictim)->GetEntityHandle() : entt::null)))
	//{
	//	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Nem lehet PVP-t kezelni klónnal.");
	//	return;
	//}
	if (!pkVictim)
		return;

	if (ecs::PlayerRuntime::IsNPC(victim))
		return;

	if (pkVictim->GetArena() != nullptr) {
		return;
	}

	int mytime = ecs::QuestSystem::GetFlag(victim, "pvp.timed");
	int itime = mytime <= 0 ? 0 : mytime - get_global_time();
	if (itime > 0) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_DIALOG, 888, "%d", itime);
		ecs::ChatSystem::SendNew(victim, CHAT_TYPE_DIALOG, 889, "%s", ecs::PlayerRuntime::GetName(character).data());
#endif
		return;
	}
	else {
		ecs::QuestSystem::SetFlag(victim, "pvp.timed", get_global_time() + 30);
	}

	if (ecs::SocialSystem::GetExchange(character) || ecs::SocialSystem::GetExchange(victim))
	{
		CPVPManager::instance().Decline(character, victim);
		CPVPManager::instance().Decline(victim, character);
		return;
	}

	if (*arg2 && !strcmp(arg2, "accept"))
	{
		int64_t chA_nBetMoney = ecs::QuestSystem::GetFlag(character, szTableStaticPvP[8]);
		int64_t  chB_nBetMoney = ecs::QuestSystem::GetFlag(victim, szTableStaticPvP[8]);
		int64_t  limit = 2000000000;


		if ((ecs::PointSystem::GetGold(character) < chA_nBetMoney) || (ecs::PointSystem::GetGold(victim) < chB_nBetMoney ) || (chA_nBetMoney > limit) || (chB_nBetMoney > limit)) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 722, "");
			ecs::ChatSystem::SendNew(victim, CHAT_TYPE_INFO, 722, "");
#endif
			CPVPManager::instance().Decline(character, victim);
			CPVPManager::instance().Decline(victim, character);
			return;
		}

		ch->SetDuel("IsFight", 1);
		pkVictim->SetDuel("IsFight", 1);

		if (chA_nBetMoney > 0 && chA_nBetMoney > 0)
		{
			ecs::PointSystem::Change(character, POINT_GOLD, - chA_nBetMoney, true);
			ecs::PointSystem::Change(victim, POINT_GOLD, - chB_nBetMoney, true);
		}

		CPVPManager::instance().Insert(character, victim);
		return;
	}

	int m_BlockChangeItem = 0, m_BlockBuff = 0, m_BlockPotion = 0, m_BlockRide = 0, m_BlockPet = 0, m_BlockPoly = 0, m_BlockParty = 0, m_BlockExchange = 0, m_BetMoney = 0;

	str_to_number(m_BlockChangeItem, arg2);
	str_to_number(m_BlockBuff, arg3);
	str_to_number(m_BlockPotion, arg4);
	str_to_number(m_BlockRide, arg5);
	str_to_number(m_BlockPet, arg6);
	str_to_number(m_BlockPoly, arg7);
	str_to_number(m_BlockParty, arg8);
	str_to_number(m_BlockExchange, arg9);
	str_to_number(m_BetMoney, arg10);

	if (!isdigit(*arg2) && !isdigit(*arg3) && !isdigit(*arg4) && !isdigit(*arg5) && !isdigit(*arg6) && !isdigit(*arg7) && !isdigit(*arg8) && !isdigit(*arg9) && !isdigit(*arg10))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_DIALOG, 874, "");
#endif
		return;
	}

	if (m_BetMoney < 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_DIALOG, 875, "");
#endif
		return;
	}

	if (m_BetMoney >= GOLD_MAX)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_DIALOG, 876, "");
#endif
		return;
	}

	if (ecs::PointSystem::GetGold(character) < m_BetMoney)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_DIALOG, 877, "");
#endif
		return;
	}

	if ((ecs::PointSystem::GetGold(character) + m_BetMoney) > GOLD_MAX)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_DIALOG, 878, "");
#endif
		return;
	}

	if ((ecs::PointSystem::GetGold(victim) + m_BetMoney) > GOLD_MAX)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_DIALOG, 878, "");
#endif
		return;
	}

	if (ecs::PointSystem::GetGold(victim) < m_BetMoney)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_DIALOG, 879, "");
#endif
		return;
	}

	if (*arg1 && *arg2 && *arg3 && *arg4 && *arg5 && *arg6 && *arg7 && *arg8 && *arg9 && *arg10)
	{
		ch->SetDuel("BlockChangeItem", m_BlockChangeItem);			ch->SetDuel("BlockBuff", m_BlockBuff);
		ch->SetDuel("BlockPotion", m_BlockPotion);					ch->SetDuel("BlockRide", m_BlockRide);
		ch->SetDuel("BlockPet", m_BlockPet);						ch->SetDuel("BlockPoly", m_BlockPoly);
		ch->SetDuel("BlockParty", m_BlockParty);					ch->SetDuel("BlockExchange", m_BlockExchange);
		ch->SetDuel("BetMoney", m_BetMoney);

		pkVictim->SetDuel("BlockChangeItem", m_BlockChangeItem);	pkVictim->SetDuel("BlockBuff", m_BlockBuff);
		pkVictim->SetDuel("BlockPotion", m_BlockPotion);			pkVictim->SetDuel("BlockRide", m_BlockRide);
		pkVictim->SetDuel("BlockPet", m_BlockPet);					pkVictim->SetDuel("BlockPoly", m_BlockPoly);
		pkVictim->SetDuel("BlockParty", m_BlockParty);				pkVictim->SetDuel("BlockExchange", m_BlockExchange);
		pkVictim->SetDuel("BetMoney", m_BetMoney);

		CPVPManager::instance().Insert(character, victim);
	}
}

ACMD(do_pvp_advanced)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch)
		return;

	if (ch->GetArena() != nullptr || CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(character)) == true)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	uint32_t vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(vid);
	const entt::entity victim = pkVictim ? pkVictim->GetEntityHandle() : entt::null;

	// Fake PC / non-real target => ignore
	//if (pkVictim->IsFakePlayer() || !ecs::PlayerRuntime::GetDesc(((pkVictim) ? (pkVictim)->GetEntityHandle() : entt::null)))
	//{
	//	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Nem lehet PVP-t kezelni klónnal.");
	//	return;
	//}
	if (!pkVictim)
		return;

	if (ecs::PlayerRuntime::IsNPC(victim))
		return;

	if (pkVictim->GetArena() != nullptr) {
		return;
	}

	if (ecs::QuestSystem::GetFlag(character, szTableStaticPvP[9]) > 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_DIALOG, 882, "");
#endif
		return;
	}

	if (ecs::QuestSystem::GetFlag(victim, szTableStaticPvP[9]) > 0)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_DIALOG, 882, "");
#endif
		return;
	}

	int statusEq = ecs::QuestSystem::GetFlag(victim, BLOCK_EQUIPMENT_);

	CGuild * g = ecs::SocialSystem::GetGuild(victim);

	const char* m_Name = ecs::PlayerRuntime::GetName(victim).data();
	const char* m_GuildName = "-";

	int m_Vid = ecs::PlayerRuntime::GetPacketVID(victim);
	int m_Level = ecs::PointSystem::GetLevel(victim);
	int m_PlayTime = ecs::PointSystem::GetReal(victim, POINT_PLAYTIME);
	int m_MaxHP = ecs::PointSystem::GetMaxHP(victim);
	int m_MaxSP = ecs::PointSystem::GetMaxSP(victim);

	uint32_t m_Race = ecs::PlayerRuntime::GetRaceNum(victim);

	if (g)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "BINARY_Duel_GetInfo %d %s %s %d %d %d %d %d", m_Vid, m_Name, g->GetName(), m_Level, m_Race, m_PlayTime, m_MaxHP, m_MaxSP);

		if (statusEq < 1)
			NetworkSyncSystem::SendEquipmentToViewer(g_registry, victim, character);
	}
	else {
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "BINARY_Duel_GetInfo %d %s %s %d %d %d %d %d", m_Vid, m_Name, m_GuildName, m_Level, m_Race, m_PlayTime, m_MaxHP, m_MaxSP);

		if (statusEq < 1)
			NetworkSyncSystem::SendEquipmentToViewer(g_registry, victim, character);
	}
}

ACMD(do_decline_pvp)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t vid = 0;
	str_to_number(vid, arg1);

	const entt::entity pkVictim = CHARACTER_MANAGER::instance().FindEntity(vid);

	if (pkVictim == entt::null)
		return;

	if (ecs::PlayerRuntime::IsNPC(pkVictim))
		return;

	CPVPManager::instance().Decline(character, pkVictim);
	ecs::QuestSystem::SetFlag(character, "pvp.timed", 0);
	ecs::QuestSystem::SetFlag(pkVictim, "pvp.timed", 0);
}
ACMD(do_block_equipment)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch)
		return;

	char arg1[256];
	one_argument (argument, arg1, sizeof(arg1));

	if (!(ecs::PlayerRuntime::IsPC(character)) || nullptr == ch)
		return;

	int statusEq = ecs::QuestSystem::GetFlag(character, BLOCK_EQUIPMENT_);

	if (!strcmp(arg1, "BLOCK"))
	{
		if (statusEq > 0)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 11, "");
#endif
		}
		else {
			ecs::QuestSystem::SetFlag(character, BLOCK_EQUIPMENT_, 1);
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "equipview 1");
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 12, "");
#endif
		}
	}
	else if (!strcmp(arg1, "UNBLOCK"))
	{
		if (statusEq != 0) {
			ecs::QuestSystem::SetFlag(character, BLOCK_EQUIPMENT_, 0);
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "equipview 0");
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 14, "");
#endif
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 13, "");
		}
#endif
	}
}
#endif

ACMD(do_guildskillup)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (!ecs::SocialSystem::GetGuild(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 138, "");
#endif
		return;
	}

	CGuild* g = ecs::SocialSystem::GetGuild(character);
	TGuildMember* gm = g->GetMember((ecs::PlayerRuntime::GetPlayerID(character)));
	if (gm->grade == GUILD_LEADER_GRADE)
	{
		uint32_t vnum = 0;
		str_to_number(vnum, arg1);
		g->SkillLevelUp(vnum);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 890, "");
	}
#endif
}

ACMD(do_skillup)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t vnum = 0;
	str_to_number(vnum, arg1);

	if (true == SkillSystem::CanUseSkill(character, vnum))
	{
		ch->SkillLevelUp(vnum);
	}
	else
	{
		switch(vnum)
		{
			case SKILL_HORSE_WILDATTACK:
			case SKILL_HORSE_CHARGE:
			case SKILL_HORSE_ESCAPE:
			case SKILL_HORSE_WILDATTACK_RANGE:

			case SKILL_7_A_ANTI_TANHWAN:
			case SKILL_7_B_ANTI_AMSEOP:
			case SKILL_7_C_ANTI_SWAERYUNG:
			case SKILL_7_D_ANTI_YONGBI:

			case SKILL_8_A_ANTI_GIGONGCHAM:
			case SKILL_8_B_ANTI_YEONSA:
			case SKILL_8_C_ANTI_MAHWAN:
			case SKILL_8_D_ANTI_BYEURAK:

			case SKILL_ADD_HP:
			case SKILL_RESIST_PENETRATE:
#ifdef ENABLE_NEW_SECONDARY_SKILLS
			case NEW_SUPPORT_SKILL_ATTACK:
			case NEW_SUPPORT_SKILL_YANG:
			case NEW_SUPPORT_SKILL_MONSTERS:
			case NEW_SUPPORT_SKILL_HP:
#endif

#ifdef ENABLE_NEW_PASSIVE_SKILLS
			case SKILL_ANTI_PALBANG:
			case SKILL_ANTI_AMSEOP:
			case SKILL_ANTI_SWAERYUNG:
			case SKILL_ANTI_YONGBI:
			case SKILL_ANTI_GIGONGCHAM:
			case SKILL_ANTI_HWAJO:
			case SKILL_ANTI_MARYUNG:
			case SKILL_ANTI_BYEURAK:
			case SKILL_HELP_PALBANG:
			case SKILL_HELP_AMSEOP:
			case SKILL_HELP_SWAERYUNG:
			case SKILL_HELP_YONGBI:
			case SKILL_HELP_GIGONGCHAM:
			case SKILL_HELP_HWAJO:
			case SKILL_HELP_MARYUNG:
			case SKILL_HELP_BYEURAK:
#endif

				ch->SkillLevelUp(vnum);
				break;
		}
	}
}

//
// @version	05/06/20 Bang2ni - Ŀǵ ó Delegate to CHARACTER class
//
ACMD(do_safebox_close)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->CloseSafebox();
}

//
// @version	05/06/20 Bang2ni - Ŀǵ ó Delegate to CHARACTER class
//
ACMD(do_safebox_password)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	ch->ReqSafeboxLoad(arg1);
}

ACMD(do_safebox_change_password)
{
	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || strlen(arg1)>6)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 188, "");
#endif
		return;
	}

	if (!*arg2 || strlen(arg2)>6)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 188, "");
#endif
		return;
	}

	TSafeboxChangePasswordPacket p;

	p.dwID = ecs::PlayerRuntime::GetDesc(character)->GetAccountTable().id;
	strlcpy(p.szOldPassword, arg1, sizeof(p.szOldPassword));
	strlcpy(p.szNewPassword, arg2, sizeof(p.szNewPassword));

	db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_CHANGE_PASSWORD, ecs::PlayerRuntime::GetDesc(character)->GetHandle(), &p, sizeof(p));
}

ACMD(do_mall_password)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1 || strlen(arg1) > 6)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 188, "");
#endif
		return;
	}

	int iPulse = thecore_pulse();

	if (ch->GetMall())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 189, "");
#endif
		return;
	}

	if (iPulse - ch->GetMallLoadTime() < passes_per_sec * 10) // 10ʿ ѹ û
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 190, "");
#endif
		return;
	}

	ch->SetMallLoadTime(iPulse);

	TSafeboxLoadPacket p;
	p.dwID = ecs::PlayerRuntime::GetDesc(character)->GetAccountTable().id;
	strlcpy(p.szLogin, ecs::PlayerRuntime::GetDesc(character)->GetAccountTable().login, sizeof(p.szLogin));
	strlcpy(p.szPassword, arg1, sizeof(p.szPassword));

	db_clientdesc->DBPacket(HEADER_GD_MALL_LOAD, ecs::PlayerRuntime::GetDesc(character)->GetHandle(), &p, sizeof(p));
}

ACMD(do_mall_close)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ch->GetMall())
	{
		ch->SetMallLoadTime(thecore_pulse());
		ch->CloseMall();
		ch->Save();
	}
}

ACMD(do_ungroup)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ecs::SocialSystem::GetParty(character))
		return;

	if (!CPartyManager::instance().IsEnablePCParty())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 208, "");
#endif
		return;
	}

	if (ch->GetDungeon())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 202, "");
#endif
		return;
	}

	LPPARTY pParty = ecs::SocialSystem::GetParty(character);

	if (pParty->GetMemberCount() == 2)
	{
		// party disband
		CPartyManager::instance().DeleteParty(pParty);
	}
	else
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 215, "");
#endif
		//pParty->SendPartyRemoveOneToAll(ch);
		pParty->Quit((ecs::PlayerRuntime::GetPlayerID(character)));
		//pParty->SendPartyRemoveAllToOne(ch);
	}
}

ACMD(do_close_shop)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ch->GetMyShop())
	{
		ch->CloseMyShop();
		return;
	}
}

ACMD(do_set_walk_mode)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->SetNowWalking(true);
	ch->SetWalking(true);
}

ACMD(do_set_run_mode)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->SetNowWalking(false);
	ch->SetWalking(false);
}

ACMD(do_war)
{
	//
	CGuild * g = ecs::SocialSystem::GetGuild(character);

	if (!g)
		return;

	// üũѹ!
	if (g->UnderAnyWar())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 167, "");
#endif
		return;
	}

	//Ķ͸ ι
	char arg1[256], arg2[256];
	uint32_t type = GUILD_WAR_TYPE_FIELD; //fixme102 base int modded uint
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
		return;

	if (*arg2)
	{
		str_to_number(type, arg2);
#ifdef ENABLE_BUG_FIXES
		if (type < 0) {
			return;
		}
#endif

		if (type >= GUILD_WAR_TYPE_MAX_NUM)
			type = GUILD_WAR_TYPE_FIELD;
	}

	//  ̵ µ
	uint32_t gm_pid = g->GetMasterPID();

	// üũ( 常 )
	if (gm_pid != (ecs::PlayerRuntime::GetPlayerID(character)))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 144, "");
#endif
		return;
	}

	// 带
	CGuild * opp_g = CGuildManager::instance().FindGuildByName(arg1);

	if (!opp_g)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 130, "");
#endif
		return;
	}

	//  üũ
	switch (g->GetGuildWarState(opp_g->GetID()))
	{
		case GUILD_WAR_NONE:
			{
				if (opp_g->UnderAnyWar())
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 157, "");
#endif
					return;
				}

				int iWarPrice = KOR_aGuildWarInfo[type].iWarPrice;

				if (g->GetGuildMoney() < iWarPrice)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 172, "");
#endif
					return;
				}

				if (opp_g->GetGuildMoney() < iWarPrice)
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 160, "");
#endif
					return;
				}
			}
			break;

		case GUILD_WAR_SEND_DECLARE:
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 438, "");
#endif
				return;
			}
			break;

		case GUILD_WAR_RECV_DECLARE:
			{
				if (opp_g->UnderAnyWar())
				{
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 157, "");
#endif
					g->RequestRefuseWar(opp_g->GetID());
					return;
				}
			}
			break;

		case GUILD_WAR_RESERVE:
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 169, "");
#endif
				return;
			}
			break;

		case GUILD_WAR_END:
			return;

		default:
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 168, "");
#endif
			g->RequestRefuseWar(opp_g->GetID());
			return;
	}

	if (!g->CanStartWar(type))
	{
		//    ִ  ʴ´.
		if (g->GetLadderPoint() == 0)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 159, "");
#endif
			LOG_INFO("GuildWar.StartError.NEED_LADDER_POINT");
		}
		else if (g->GetMemberCount() < GUILD_WAR_MIN_MEMBER_COUNT)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 145, "%d", GUILD_WAR_MIN_MEMBER_COUNT);
#endif
			LOG_INFO("GuildWar.StartError.NEED_MINIMUM_MEMBER[{}]", GUILD_WAR_MIN_MEMBER_COUNT);
		}
		else
		{
			LOG_INFO("GuildWar.StartError.UNKNOWN_ERROR");
		}
		return;
	}

	// ʵ üũ ϰ  üũ  ³Ҷ Ѵ.
	if (!opp_g->CanStartWar(GUILD_WAR_TYPE_FIELD))
	{
#ifdef TEXTS_IMPROVEMENT
		if (opp_g->GetLadderPoint() == 0) {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 153, "%s", opp_g->GetName());
		} else if (opp_g->GetMemberCount() < GUILD_WAR_MIN_MEMBER_COUNT) {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 158, "");
		}
#endif
		return;
	}

	do
	{
		if (g->GetMasterCharacter() != nullptr)
			break;

		CCI *pCCI = P2P_MANAGER::instance().FindByPID(g->GetMasterPID());

		if (pCCI != nullptr)
			break;

#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 507, "");
#endif
		g->RequestRefuseWar(opp_g->GetID());
		return;

	} while (false);

	do
	{
		if (opp_g->GetMasterCharacter() != nullptr)
			break;

		CCI *pCCI = P2P_MANAGER::instance().FindByPID(opp_g->GetMasterPID());

		if (pCCI != nullptr)
			break;

#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 507, "");
#endif
		g->RequestRefuseWar(opp_g->GetID());
		return;

	} while (false);

	g->RequestDeclareWar(opp_g->GetID(), type);
}

ACMD(do_nowar)
{
	CGuild* g = ecs::SocialSystem::GetGuild(character);
	if (!g)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t gm_pid = g->GetMasterPID();

	if (gm_pid != (ecs::PlayerRuntime::GetPlayerID(character)))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 144, "");
#endif
		return;
	}

	CGuild* opp_g = CGuildManager::instance().FindGuildByName(arg1);

	if (!opp_g)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 130, "");
#endif
		return;
	}

	g->RequestRefuseWar(opp_g->GetID());
}

ACMD(do_detaillog)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->DetailLog();
}

ACMD(do_monsterlog)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->ToggleMonsterLog();
}

ACMD(do_pkmode)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint8_t mode = 0;
	str_to_number(mode, arg1);

	if (mode == PK_MODE_PROTECT)
		return;

	if ((ecs::PointSystem::GetLevel(character)) < PK_PROTECT_LEVEL && mode != 0)
		return;

	ch->SetPKMode(mode);
}

ACMD(do_messenger_auth)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ch->GetArena())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;

	char answer = LOWER(*arg1);
	// @fixme130 AuthToAdd void -> bool
	bool bIsDenied = answer != 'y';
	bool bIsAdded = MessengerManager::instance().AuthToAdd(ecs::PlayerRuntime::GetName(character).data(), arg2, bIsDenied); // DENY
	if (bIsAdded && bIsDenied)
	{
		const entt::entity tch = CHARACTER_MANAGER::instance().FindPCEntity(arg2);
#ifdef TEXTS_IMPROVEMENT
		if (tch != entt::null) {
			ecs::ChatSystem::SendNew(tch, CHAT_TYPE_INFO, 107, "%s", ecs::PlayerRuntime::GetName(character).data());
		}
#endif
	}
}

ACMD(do_setblockmode)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		uint8_t flag = 0;
		str_to_number(flag, arg1);
		ch->SetBlockMode(flag);
	}
}

ACMD(do_unmount)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	const entt::entity owner = character;
	const entt::entity mount = ItemSystem::GetWearItem(owner, WEAR_COSTUME_MOUNT);
	if (ItemSystem::IsValidItem(mount))
	{
		CMountSystem* mountSystem = ch->GetMountSystem();
		uint32_t mobVnum = 0;

		if (!mountSystem)
			return;

#ifdef __CHANGELOOK_SYSTEM__
		if (const uint32_t transmutation = ItemSystem::GetItemTransmutationVnum(mount))
		{
			const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(transmutation);

			if (itemTable)
				mobVnum = itemTable->alValues[1];
			else
				mobVnum = ItemSystem::GetItemValue(mount, 1);
		}
		else
			mobVnum = ItemSystem::GetItemValue(mount, 1);
#else
		if (ItemSystem::GetItemValue(mount, 1) != 0)
			mobVnum = ItemSystem::GetItemValue(mount, 1);
#endif

		if (MountSystem::GetMountVnum(character))
		{
			mountSystem->Unmount(mobVnum);
		}
		return;
	}
#endif

	if (true == ch->UnEquipSpecialRideUniqueItem())
	{
		AffectSystem::RemoveAffect(character, AFFECT_MOUNT);
		AffectSystem::RemoveAffect(character, AFFECT_MOUNT_BONUS);

		if (ch->IsHorseRiding())
		{
			ch->StopRiding();
		}
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 366, "");
	}
#endif
}

ACMD(do_observer_exit)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ecs::PlayerRuntime::IsObserverMode(character))
	{
		if (ch->GetWarMap())
			ch->SetWarMap(nullptr);

		if (ch->GetArena() != nullptr || ch->GetArenaObserverMode() == true)
		{
			ch->SetArenaObserverMode(false);

			if (ch->GetArena() != nullptr)
				ch->GetArena()->RemoveObserver((ecs::PlayerRuntime::GetPlayerID(character)));

			ch->SetArena(nullptr);
			ecs::MovementSystem::WarpSet(character, ARENA_RETURN_POINT_X((ecs::PlayerRuntime::GetEmpire(character))), ARENA_RETURN_POINT_Y((ecs::PlayerRuntime::GetEmpire(character))));
		}
		else
		{
			ecs::MovementSystem::ExitToSavedLocation(character);
		}
		ch->SetObserverMode(false);
	}
}

ACMD(do_view_equip)
{
	if ((ecs::PlayerRuntime::GetGMLevel(character)) <= GM_PLAYER)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		uint32_t vid = 0;
		str_to_number(vid, arg1);
		const entt::entity tch = CHARACTER_MANAGER::instance().FindEntity(vid);

		if (tch == entt::null)
			return;

		if (!ecs::PlayerRuntime::IsPC(tch))
			return;

		NetworkSyncSystem::SendEquipmentToViewer(g_registry, tch, character);
	}
}

ACMD(do_party_request)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ch->GetArena())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	if (ecs::SocialSystem::GetParty(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 441, "");
#endif
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t vid = 0;
	str_to_number(vid, arg1);
	auto* tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		if (!ch->RequestToParty((tch ? tch->GetEntityHandle() : entt::null)))
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "PartyRequestDenied");
}

ACMD(do_party_request_accept)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t vid = 0;
	str_to_number(vid, arg1);
	auto* tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		ch->AcceptToParty((tch ? tch->GetEntityHandle() : entt::null));
}

ACMD(do_party_request_deny)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	uint32_t vid = 0;
	str_to_number(vid, arg1);
	auto* tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		ch->DenyToParty((tch ? tch->GetEntityHandle() : entt::null));
}

// LUA_ADD_GOTO_INFO
struct GotoInfo
{
	std::string 	st_name;

	uint8_t 	empire;
	int 	mapIndex;
	uint32_t 	x, y;

	GotoInfo()
	{
		st_name 	= "";
		empire 		= 0;
		mapIndex 	= 0;

		x = 0;
		y = 0;
	}

	GotoInfo(const GotoInfo& c_src)
	{
		__copy__(c_src);
	}

	void operator = (const GotoInfo& c_src)
	{
		__copy__(c_src);
	}

	void __copy__(const GotoInfo& c_src)
	{
		st_name 	= c_src.st_name;
		empire 		= c_src.empire;
		mapIndex 	= c_src.mapIndex;

		x = c_src.x;
		y = c_src.y;
	}
};

#ifdef __ATTR_TRANSFER_SYSTEM__
ACMD(do_attr_transfer)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch->CanDoAttrTransfer())
		return;

	LOG_INFO("{} has used an Attr Transfer command: {}.", ecs::PlayerRuntime::GetName(character).data(), argument);

	int w_index = 0, i_index = 0;
	const char *line;
	char arg1[256], arg2[256], arg3[256];
	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));
	if (0 == arg1[0]) {
		return;
	}

	std::string_view strArg1(arg1);
	if (strArg1 == "open")
	{
		AttrTransfer_open(ch);
		return;
	}
	else if (strArg1 == "close")
	{
		AttrTransfer_close(ch);
		return;
	}
	else if (strArg1 == "make")
	{
		AttrTransfer_make(ch);
		return;
	}
	else if (strArg1 == "add")
	{
		if (0 == arg2[0] || !isdigit(*arg2) || 0 == arg3[0] || !isdigit(*arg3))
			return;

		str_to_number(w_index, arg2);
		str_to_number(i_index, arg3);
		AttrTransfer_add_item(ch, w_index, i_index);
		return;
	}
	else if (strArg1 == "delete")
	{
		if (0 == arg2[0] || !isdigit(*arg2))
			return;

		str_to_number(w_index, arg2);
		AttrTransfer_delete_item(ch, w_index);
		return;
	}

	switch (LOWER(arg1[0]))
	{
		case 'o':
			AttrTransfer_open(ch);
			break;
		case 'c':
			AttrTransfer_close(ch);
			break;
		case 'm':
			AttrTransfer_make(ch);
			break;
		case 'a':
			{
				if (0 == arg2[0] || !isdigit(*arg2) || 0 == arg3[0] || !isdigit(*arg3))
					return;

				str_to_number(w_index, arg2);
				str_to_number(i_index, arg3);
				AttrTransfer_add_item(ch, w_index, i_index);
			}
			break;
		case 'd':
			{
				if (0 == arg2[0] || !isdigit(*arg2))
					return;

				str_to_number(w_index, arg2);
				AttrTransfer_delete_item(ch, w_index);
			}
			break;
		default:
			return;
	}
}
#endif

ACMD(do_inventory)
{
	int	index = 0;
	int	count		= 1;

	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1) {
		return;
	}

	if (!*arg2)
	{
		index = 0;
		str_to_number(count, arg1);
	}
	else
	{
		str_to_number(index, arg1); index = MIN(index, INVENTORY_MAX_NUM);
		str_to_number(count, arg2); count = MIN(count, INVENTORY_MAX_NUM);
	}

	for (int i = 0; i < count; ++i)
	{
		if (index >= INVENTORY_MAX_NUM)
			break;

		const entt::entity item = ItemSystem::GetInventoryItem(
			character, index);
#ifdef TEXTS_IMPROVEMENT
		if (ItemSystem::IsValidItem(item)) {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 727,
				"%d#%s", index, ItemSystem::GetItemName(item));
		} else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 728, "%d", index);
		}
#endif
		++index;
	}
}

//gift notify quest command
ACMD(do_gift)
{
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "gift");
}

#ifdef __NEWPET_SYSTEM__
ACMD(do_CubePetAdd) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);

	int pos = 0;
	int invpos = 0;

	const char *line;
	char arg1[256], arg2[256], arg3[256];

	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));

	if (0 == arg1[0])
		return;
	std::string_view strArg1(arg1);
	switch (LOWER(arg1[0]))
	{
	case 'a':	// add cue_index inven_index
	{
		if (0 == arg2[0] || !isdigit(*arg2) ||
			0 == arg3[0] || !isdigit(*arg3))
			return;

		str_to_number(pos, arg2);
		str_to_number(invpos, arg3);

	}
	break;

	default:
		return;
	}

	if (ch->GetNewPetSystem()->IsActivePet())
		ch->GetNewPetSystem()->SetItemCube(pos, invpos);
	else
		return;

}

ACMD(do_PetSkill) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;

	uint32_t skillslot = 0;
	str_to_number(skillslot, arg1);
	if (skillslot > 3 || skillslot < 0)
		return;

	if (ch->GetNewPetSystem()->IsActivePet()) {
		ch->GetNewPetSystem()->DoPetSkill(skillslot);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 729, "");
	}
#endif
}

#ifdef ENABLE_NEW_PET_EDITS
ACMD(do_PetIncreaseSkill) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if ((!*arg1) || (!*arg2))
		return;

	int iSlot = atoi(arg1), iType = atoi(arg2);
	if (!ch->GetNewPetSystem())
		return;

	if (ch->GetNewPetSystem()->IsActivePet())
		ch->GetNewPetSystem()->IncreasePetSkill(iSlot, iType);
}
#endif

ACMD(do_FeedCubePet) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;

	uint32_t feedtype = 0;
	str_to_number(feedtype, arg1);
	if (ch->GetNewPetSystem()->IsActivePet()) {
		ch->GetNewPetSystem()->ItemCubeFeed(feedtype);
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 729, "");
	}
#endif
}

ACMD(do_PetEvo) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);

	if (ecs::SocialSystem::GetExchange(character) || ch->GetMyShop() || ch->GetShopOwner() || ch->IsOpenSafebox() || ch->IsCubeOpen()) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 730, "");
#endif
		return;
	}
	if (ch->GetNewPetSystem()->IsActivePet()) {
		int tmpevo = ch->GetNewPetSystem()->GetEvolution();
		if (((tmpevo == 0) && (ch->GetNewPetSystem()->GetLevel() >= 40)) || ((tmpevo == 1) && (ch->GetNewPetSystem()->GetLevel() >= 60)) || ((tmpevo == 2) && (ch->GetNewPetSystem()->GetLevel() >= 80))) {
#ifdef ENABLE_NEW_PET_EDITS
			if (ch->GetNewPetSystem()->GetExp() < ch->GetNewPetSystem()->GetNextExpFromMob()) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 59, "");
#endif
				return;
			}
#endif

			bool bRet = false;
			uint32_t dwItemVnum1 = 55003 + tmpevo;
			if (ch->CountSpecifyItem(dwItemVnum1) < 10) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 60, "%d#%s", 10,
#ifdef ENABLE_MULTI_NAMES
				ITEM_MANAGER::instance().GetTable(dwItemVnum1)->szLocaleName[ecs::PlayerRuntime::GetDesc(character)->GetLanguage()]
#else
				ITEM_MANAGER::instance().GetTable(dwItemVnum1)->szLocaleName
#endif
				);
#endif
				bRet = true;
			}

			uint32_t dwItemVnum2 = 27992 + tmpevo;
			if (!bRet && ch->CountSpecifyItem(dwItemVnum2) < 10) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 60, "%d#%s", 10,
#ifdef ENABLE_MULTI_NAMES
				ITEM_MANAGER::instance().GetTable(dwItemVnum2)->szLocaleName[ecs::PlayerRuntime::GetDesc(character)->GetLanguage()]
#else
				ITEM_MANAGER::instance().GetTable(dwItemVnum2)->szLocaleName
#endif
				);
#endif
				bRet = true;
			}

			uint32_t dwItemVnum3 = 86056 + tmpevo;
			if (!bRet && ch->CountSpecifyItem(dwItemVnum3) < 3) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 60, "%d#%s", 3,
#ifdef ENABLE_MULTI_NAMES
				ITEM_MANAGER::instance().GetTable(dwItemVnum3)->szLocaleName[ecs::PlayerRuntime::GetDesc(character)->GetLanguage()]
#else
				ITEM_MANAGER::instance().GetTable(dwItemVnum3)->szLocaleName
#endif
				);
#endif
				bRet = true;
			}

			if (bRet)
				return;

			ch->RemoveSpecifyItem(dwItemVnum1, 10);
			ch->RemoveSpecifyItem(dwItemVnum2, 10);
			ch->RemoveSpecifyItem(dwItemVnum3, 3);
			ch->GetNewPetSystem()->IncreasePetEvolution();
		}
		else {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 730, "");
#endif
			return;
		}
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 729, "");
	}
#endif

}

#endif

#ifdef ENABLE_NEW_CRAFT_SYSTEM_RAZOR93
namespace
{
	const uint32_t STONE_CRAFT_NPC_VNUM = 9005;
	const uint32_t STONE_CRAFT_REWARD_VNUM = 39066;
	const int STONE_CRAFT_NEED_COUNT = 5;
	const int STONE_CRAFT_MAX_DISTANCE = 1000;

	static bool IsStoneCraftMaterialVnum(uint32_t vnum)
	{
		if (vnum >= 28030 && vnum <= 28043)
			return true;

		if (vnum >= 28130 && vnum <= 28143)
			return true;

		if (vnum >= 28230 && vnum <= 28243)
			return true;

		if (vnum >= 28330 && vnum <= 28343)
			return true;

		if (vnum >= 28430 && vnum <= 28443)
			return true;

		switch (vnum)
		{
		case 28100:
		case 28104:
		case 28108:
		case 28112:

		case 28200:
		case 28204:
		case 28208:
		case 28212:

		case 28300:
		case 28304:
		case 28308:
		case 28312:

		case 28400:
		case 28404:
		case 28408:
		case 28412:
			return true;
		}

		return false;
	}

	static entt::entity GetStoneCraftNpc(entt::entity ch)
	{
		if (ch == entt::null)
			return entt::null;

		entt::entity npc = ecs::PlayerRuntime::GetQuestNPC(ch);
		if (npc != entt::null && ecs::PlayerRuntime::GetRaceNum(npc) == STONE_CRAFT_NPC_VNUM)
			return npc;

		npc = CombatSystem::GetSelectedTarget(ch);
		if (npc != entt::null && ecs::PlayerRuntime::GetRaceNum(npc) == STONE_CRAFT_NPC_VNUM)
			return npc;

		return entt::null;
	}

	static bool CanUseStoneCraft(entt::entity chEntity, entt::entity npcEntity)
	{
		if (chEntity == entt::null || npcEntity == entt::null)
			return false;

		if (ecs::PlayerRuntime::GetRaceNum(npcEntity) != STONE_CRAFT_NPC_VNUM)
			return false;

	if (CombatSystem::IsDead(chEntity) || CombatSystem::IsStun(chEntity) || ecs::PlayerRuntime::IsObserverMode(chEntity))
			return false;

		// IsOpenSafebox and IsCubeOpen have no entity form; one resolve for the
		// pair rather than two, and only on the branch that needs them.
		LPCHARACTER ch = ecs::LegacyCharOf(chEntity);
		if (ecs::SocialSystem::GetExchange(chEntity)
			|| ecs::SocialSystem::GetMyShop(chEntity)
			|| ecs::SocialSystem::GetShopOwner(chEntity) != entt::null
			|| (ch && (ch->IsOpenSafebox() || ch->IsCubeOpen())))
			return false;

		const int32_t distance = DISTANCE_APPROX(ecs::PlayerRuntime::GetX(chEntity) - ecs::PlayerRuntime::GetX(npcEntity), ecs::PlayerRuntime::GetY(chEntity) - ecs::PlayerRuntime::GetY(npcEntity));
		if (distance >= STONE_CRAFT_MAX_DISTANCE)
			return false;

		return true;
	}
}

ACMD(do_stonecraft)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
		return;

	if (!str_cmp(arg1, "open"))
	{
		const entt::entity npc = GetStoneCraftNpc(character);
		if (!CanUseStoneCraft(character, npc))
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You need to be closer to npc.");
			return;
		}

		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "stone_craft_open");
		return;
	}

	if (!str_cmp(arg1, "make"))
	{
		if (!*arg2)
			return;

		uint32_t materialVnum = static_cast<uint32_t>(atoi(arg2));

		if (!IsStoneCraftMaterialVnum(materialVnum))
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Unknow stone.");
			return;
		}

		const entt::entity npc = GetStoneCraftNpc(character);
		if (!CanUseStoneCraft(character, npc))
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You are too far from npc.");
			return;
		}

		const int materialCount = ch->CountSpecifyItemRenewal(materialVnum);
		if (materialCount < STONE_CRAFT_NEED_COUNT)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Not enough stone.");
			return;
		}

		const entt::entity reward = ItemSystem::AutoGiveItemEcs(character, STONE_CRAFT_REWARD_VNUM, 1);
		if (reward == entt::null)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Not enough space in inventory.");
			return;
		}

		ch->RemoveSpecifyItem(materialVnum, STONE_CRAFT_NEED_COUNT, true);
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Craft successful.");
		return;
	}
	if (!str_cmp(arg1, "makeall"))
	{
		const entt::entity npc = GetStoneCraftNpc(character);
		if (!CanUseStoneCraft(character, npc))
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "You are too far from npc");
			return;
		}

		static const uint32_t stoneVnums[] =
		{
			28030, 28031, 28032, 28033, 28034, 28035, 28036, 28037, 28038, 28039, 28040, 28041, 28042, 28043,
			28100, 28104, 28108, 28112,
			28130, 28131, 28132, 28133, 28134, 28135, 28136, 28137, 28138, 28139, 28140, 28141, 28142, 28143,
			28200, 28204, 28208, 28212,
			28230, 28231, 28232, 28233, 28234, 28235, 28236, 28237, 28238, 28239, 28240, 28241, 28242, 28243,
			28300, 28304, 28308, 28312,
			28330, 28331, 28332, 28333, 28334, 28335, 28336, 28337, 28338, 28339, 28340, 28341, 28342, 28343,
			28400, 28404, 28408, 28412,
			28430, 28431, 28432, 28433, 28434, 28435, 28436, 28437, 28438, 28439, 28440, 28441, 28442, 28443
		};

		int totalCrafted = 0;

		for (size_t i = 0; i < sizeof(stoneVnums) / sizeof(stoneVnums[0]); ++i)
		{
			const uint32_t materialVnum = stoneVnums[i];
			const int materialCount = ch->CountSpecifyItemRenewal(materialVnum);
			const int craftCount = materialCount / STONE_CRAFT_NEED_COUNT;

			if (craftCount > 0)
				totalCrafted += craftCount;
		}

		if (totalCrafted <= 0)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Not enough stone.");
			return;
		}


		const entt::entity reward = ItemSystem::AutoGiveItemEcs(character, STONE_CRAFT_REWARD_VNUM, totalCrafted, -1, false);
		if (reward == entt::null)
		{
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Not enough space in inventory.");
			return;
		}

		// Csak sikeres itemadas utan vonjuk le az anyagokat
		for (size_t i = 0; i < sizeof(stoneVnums) / sizeof(stoneVnums[0]); ++i)
		{
			const uint32_t materialVnum = stoneVnums[i];
			const int materialCount = ch->CountSpecifyItemRenewal(materialVnum);
			const int craftCount = materialCount / STONE_CRAFT_NEED_COUNT;

			if (craftCount > 0)
				ch->RemoveSpecifyItem(materialVnum, craftCount * STONE_CRAFT_NEED_COUNT, true);
		}

		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Craft successful: You got %d items.", totalCrafted);
		return;
	}
}
#endif


#ifdef ENABLE_CUBE_RENEWAL_WORLDARD
ACMD(do_cube)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);

	const char *line;
	char arg1[256], arg2[256], arg3[256];
	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));

	if (0 == arg1[0])
	{
		return;
	}

	switch (LOWER(arg1[0]))
	{
		case 'o':	// open
			Cube_open(ch);
			break;

		default:
			return;
	}
}
#else
ACMD(do_cube)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch->CanDoCube())
		return;

	dev_log(LOG_DEB0, "CUBE COMMAND <%s>: %s", ecs::PlayerRuntime::GetName(character).data(), argument);
	int cube_index = 0, inven_index = 0;
#ifdef ENABLE_EXTRA_INVENTORY
	uint8_t window_type = 0;
	char arg1[256], arg2[256], arg3[256], arg4[256];

	two_arguments(two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2)), arg3, sizeof(arg3), arg4, sizeof(arg4));
#else
	const char *line;
	char arg1[256], arg2[256], arg3[256];

	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));
#endif


	if (0 == arg1[0]) {
		return;
	}

	std::string_view strArg1(arg1);

	// r_info (request information)
	// /cube r_info     ==> (Client -> Server)  NPC   ִ  û
	//					    (Server -> Client) /cube r_list npcVNUM resultCOUNT 123,1/125,1/128,1/130,5
	//
	// /cube r_info 3   ==> (Client -> Server)  NPC  ִ   3°    ʿ  û
	// /cube r_info 3 5 ==> (Client -> Server)  NPC  ִ   3° ۺ  5    ʿ   û
	//					   (Server -> Client) /cube m_info startIndex count 125,1|126,2|127,2|123,5&555,5&555,4/120000@125,1|126,2|127,2|123,5&555,5&555,4/120000
	//
	if (strArg1 == "r_info")
	{
		if (0 == arg2[0])
			Cube_request_result_list(ch);
		else
		{
			if (isdigit(*arg2))
			{
				int listIndex = 0, requestCount = 1;
				str_to_number(listIndex, arg2);

				if (0 != arg3[0] && isdigit(*arg3))
					str_to_number(requestCount, arg3);

				Cube_request_material_info(ch, listIndex, requestCount);
			}
		}

		return;
	}

	switch (LOWER(arg1[0]))
	{
		case 'o':	// open
			Cube_open(ch);
			break;

		case 'c':	// close
			Cube_close(ch);
			break;

		case 'l':	// list
			Cube_show_list(ch);
			break;

		case 'a':	// add cue_index inven_index
			{
#ifdef ENABLE_EXTRA_INVENTORY
				if (arg2[0] == 0 || !isdigit(*arg2) || arg3[0] == 0 || !isdigit(*arg3) || arg4[0] == 0 || !isdigit(*arg4))
#else
				if (0 == arg2[0] || !isdigit(*arg2) ||
					0 == arg3[0] || !isdigit(*arg3))
#endif
					return;

				str_to_number(cube_index, arg2);
				str_to_number(inven_index, arg3);
#ifdef ENABLE_EXTRA_INVENTORY
				str_to_number(window_type, arg4);
				Cube_add_item(ch, cube_index, inven_index, window_type);
#else
				Cube_add_item (ch, cube_index, inven_index);
#endif
			}
			break;

		case 'd':	// delete
			{
				if (0 == arg2[0] || !isdigit(*arg2))
					return;

				str_to_number(cube_index, arg2);
				Cube_delete_item (ch, cube_index);
			}
			break;

		case 'm':	// make
			if (0 != arg2[0])
			{
				while (true == Cube_make(ch))
					dev_log (LOG_DEB0, "cube make success");
			}
			else
				Cube_make(ch);
			break;

		default:
			return;
	}
}
#endif


void open_in_game_mall(entt::entity character)
{
	if (character == entt::null || !g_registry.valid(character))
		return;

	char buf[512+1];
	char sas[33];
	MD5_CTX ctx;
	const char sas_key[] = "raLgNC5jCu7LrA";

	char language[3];
	strcpy(language, "en");//If you have multilanguage, update this

	snprintf(buf, sizeof(buf), "%u%u%s", ecs::PlayerRuntime::GetPlayerID(character),
		ecs::PlayerRuntime::GetAccountID(character), sas_key);

	MD5Init(&ctx);
	MD5Update(&ctx, (const unsigned char *) buf, strlen(buf));
#ifdef __FreeBSD__
	MD5End(&ctx, sas);
#else
	static const char hex[] = "0123456789abcdef";
	unsigned char digest[16];
	MD5Final(digest, &ctx);
	int i;
	for (i = 0; i < 16; ++i) {
		sas[i+i] = hex[digest[i] >> 4];
		sas[i+i+1] = hex[digest[i] & 0x0f];
	}
	sas[i+i] = '\0';
#endif

	// snprintf(buf, sizeof(buf), "mall https://www.%s/shop?pid=%u&lang=%s&sid=%d&sas=%s",
			// g_strWebMallURL.c_str(), (ecs::PlayerRuntime::GetPlayerID(((ch) ? (ch)->GetEntityHandle() : entt::null))), language, g_server_id, sas);
	snprintf(buf, sizeof(buf), "mall https://bwmt2-global.eu/shop/",
			ecs::PlayerRuntime::GetPlayerID(character), language, sas);
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, buf);

//	char buf[512+1];
//	char sas[33];
//	MD5_CTX ctx;
//	const char secretKey[] = "base64:vKqgEB0ho5Swmzh+bQTAmBoWpOk8z2yIFaxsIJMOzvE=";
//	const char websiteUrl[] = "https://bwmt2-global.eu";
//	snprintf(buf, sizeof(buf), "%u%s", ch->GetAID(), secretKey);
//	MD5Init(&ctx);
//	MD5Update(&ctx, (const unsigned char *) buf, strlen(buf));
//#ifdef __FreeBSD__
//	MD5End(&ctx, sas);
//#else
//	static const char hex[] = "0123456789abcdef";
//	unsigned char digest[16];
//	MD5Final(digest, &ctx);
//	int i;
//	for (i = 0; i < 16; ++i) {
//		sas[i+i] = hex[digest[i] >> 4];
//		sas[i+i+1] = hex[digest[i] & 0x0f];
//	}
//
//	sas[i+i] = '\0';
//#endif
//
//#ifdef ENABLE_MULTI_LANGUAGE
//	uint8_t lang = ecs::PlayerRuntime::GetDesc(((ch) ? (ch)->GetEntityHandle() : entt::null)) ? ecs::PlayerRuntime::GetDesc(((ch) ? (ch)->GetEntityHandle() : entt::null))->GetLanguage() : 0;
//	std::string str_lang;
//	switch (lang) {
//		case LANGUAGE_RO: {
//			str_lang = "ro";
//			break;
//		}
//		case LANGUAGE_IT: {
//			str_lang = "it";
//			break;
//		}
//		case LANGUAGE_TR: {
//			str_lang = "tr";
//			break;
//		}
//		case LANGUAGE_DE: {
//			str_lang = "de";
//			break;
//		}
//		case LANGUAGE_PL: {
//			str_lang = "pl";
//			break;
//		}
//		case LANGUAGE_PT: {
//			str_lang = "pt";
//			break;
//		}
//		case LANGUAGE_ES: {
//			str_lang = "es";
//			break;
//		}
//		case LANGUAGE_CZ: {
//			str_lang = "cz";
//			break;
//		}
//		case LANGUAGE_HU: {
//			str_lang = "hu";
//			break;
//		}
//		default: {
//			str_lang = "en";
//			break;
//		}
//	}
//
//	snprintf(buf, sizeof(buf), "mall %s/in-game-shop?aid=%u&secret=%s&lang=%s", websiteUrl, ch->GetAID(), sas, str_lang.c_str());
//#else
//	snprintf(buf, sizeof(buf), "mall %s/in-game-shop?aid=%u&secret=%s", websiteUrl, ch->GetAID(), sas);
//#endif
//	ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_COMMAND, buf);
}

ACMD(do_in_game_mall)
{
	open_in_game_mall(character);
}

// ֻ
ACMD(do_dice)
{
#ifdef TEXTS_IMPROVEMENT
	char arg1[256], arg2[256];
	int start = 1, end = 100;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (*arg1 && *arg2)
	{
		start = atoi(arg1);
		end = atoi(arg2);
	}
	else if (*arg1 && !*arg2)
	{
		start = 1;
		end = atoi(arg1);
	}

	end = MAX(start, end);
	start = MIN(start, end);

	int n = number(start, end);
	if (ecs::SocialSystem::GetParty(character)) {
		ecs::SocialSystem::GetParty(character)->ChatPacketToAllMemberNew(
#ifdef ENABLE_DICE_SYSTEM
		CHAT_TYPE_DICE_INFO
#else
		CHAT_TYPE_INFO
#endif
		, 544, "%s#%d#%d#%d", ecs::PlayerRuntime::GetName(character).data(), n, start, end);
	} else {
		ecs::ChatSystem::SendNew(character,
#ifdef ENABLE_DICE_SYSTEM
		CHAT_TYPE_DICE_INFO
#else
		CHAT_TYPE_INFO
#endif
		, 545, "%d#%d#%d", n, start, end);
	}
#endif
}

#ifdef ENABLE_NEWSTUFF
ACMD(do_click_safebox)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ch->GetDungeon() || ch->GetWarMap())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 731, "");
#endif
		return;
	}

	ch->SetSafeboxOpenPosition();
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ShowMeSafeboxPassword");
}
ACMD(do_force_logout)
{
	LPDESC pDesc=DESC_MANAGER::instance().FindByCharacterName(ecs::PlayerRuntime::GetName(character).data());
	if (!pDesc)
		return;
	pDesc->DelayedDisconnect(0);
}
#endif

ACMD(do_click_mall)
{
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ShowMeMallPassword");
}

ACMD(do_ride)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
#ifdef DISABLE_CORE_PULSE_RAZOR93

	if (!ch->IsNextMountPulse()) {
		ecs::ChatSystem::Send(character,  CHAT_TYPE_INFO, "You can't do this that fast, please calm down a bit...");
		return;
	}
#endif
    dev_log(LOG_DEB0, "[DO_RIDE] start");
	if (CombatSystem::IsDead(character) || CombatSystem::IsStun(character))
		return;

	if (ecs::PlayerRuntime::GetMapIndex(character) == 113)
		return;

	const entt::entity owner = character;
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (ch->IsPolymorphed() == true){
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 732, "");
#endif
		return;
	}
	const entt::entity mount = ItemSystem::GetWearItem(owner, WEAR_COSTUME_MOUNT);
	if (ItemSystem::IsValidItem(mount))
	{
		CMountSystem* mountSystem = ch->GetMountSystem();
		uint32_t mobVnum = 0;

		if (!mountSystem)
			return;

#ifdef __CHANGELOOK_SYSTEM__
		if (const uint32_t transmutation = ItemSystem::GetItemTransmutationVnum(mount))
		{
			const TItemTable* itemTable = ITEM_MANAGER::instance().GetTable(transmutation);

			if (itemTable)
				mobVnum = itemTable->alValues[1];
			else
				mobVnum = ItemSystem::GetItemValue(mount, 1);
		}
		else
			mobVnum = ItemSystem::GetItemValue(mount, 1);
#else
		if (ItemSystem::GetItemValue(mount, 1) != 0)
			mobVnum = ItemSystem::GetItemValue(mount, 1);
#endif

		if (MountSystem::GetMountVnum(character))
		{
			mountSystem->Unmount(mobVnum);
		}
		else
		{
			if(mountSystem->CountSummoned() == 1)
			{
				mountSystem->Mount(mobVnum, mount);
			}
		}

		return;
	}
#endif

	if (ch->IsHorseRiding())
	{
		ch->StopRiding();
		return;
	}

	if (ch->GetHorse() != nullptr)
	{
	    ch->StartRiding();
	    return;
	}

	for (UINT i=0; i< INVENTORY_MAX_NUM; ++i) //INVENTORY_MAX_NUM
	{
		const entt::entity item = ItemSystem::GetInventoryItem(owner, i);
		if (!ItemSystem::IsValidItem(item))
			continue;

		if (ItemSystem::GetItemType(item) == ITEM_COSTUME &&
			ItemSystem::GetItemSubType(item) == COSTUME_MOUNT) {
			ItemSystem::UseItemEcs(owner, item);
			return;
		}
	}
//// belt inventory kereses
//	for (uint8_t i = BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END; ++i)
//	{
//		const entt::entity item = ItemSystem::GetInventoryItem(character, i);
//		if (!item)
//			continue;
//
//		if (ItemSystem::GetItemType((item ? item->GetEntityHandle() : entt::null)) == ITEM_COSTUME && ItemSystem::GetItemSubType((item ? item->GetEntityHandle() : entt::null)) == COSTUME_MOUNT)
//		{
//			ch->UseItem(TItemPos(INVENTORY, i)); // belt inventory is INVENTORY window_type
//			return;
//		}
	//}

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 5, "");
#endif
}

#ifdef ENABLE_GAYA_SYSTEM
ACMD(do_gaya_system)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (quest::CQuestManager::instance().GetEventFlag("gaya_disable") == 1)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 734, "");
#endif
		return;
	}

	char arg1[255];
	char arg2[255];
	char arg3[256];
	three_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2), arg3, sizeof(arg3));

	if (!*arg1)
		return;

	if (0 == arg1[0])
		return;

	std::string_view strArg1(arg1);
	if (strArg1 == "craft")
	{
		if (0 == arg2[0])
			return;

		int slot = atoi(arg2);
		ch->CraftGayaItems(slot);
	}
	else if (strArg1 == "market")
	{
		if (0 == arg2[0])
			return;

		int slot = atoi(arg2);
		ch->MarketGayaItems(slot);
	}
	else if (strArg1 == "refresh")
	{
		ch->RefreshGayaItems();
	}
}
#endif

#ifdef __ENABLE_RANGE_ALCHEMY__
ACMD(do_extend_range_npc)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;


	uint32_t vnum = 0;
	str_to_number(vnum, arg1);

	if (CombatSystem::IsDead(character))
		return;

	if (CombatSystem::IsDead(character) || ecs::SocialSystem::GetExchange(character) || ch->GetMyShop() || ch->IsOpenSafebox() || ch->IsCubeOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 735, "");
#endif
		return;
	}

	LPSHOP shop = CShopManager::instance().Get(vnum);

	if(!shop)
		return;

	ch->SetShopOwner(character);
	shop->AddGuest(ch, 0, false);

}
#endif



#ifdef __ENABLE_REFINE_ALCHEMY__
ACMD(do_refine_window_alchemy) {
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	ch->DragonSoul_RefineWindow_Open(ch);
}
#endif



#ifdef __HIDE_COSTUME_SYSTEM__
ACMD(do_hide_costume)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
		return;

	bool hidden = true;
	uint8_t bPartPos = 0;
	uint8_t bHidden = 0;

	str_to_number(bPartPos, arg1);

	if (*arg2)
	{
		str_to_number(bHidden, arg2);

		if (bHidden == 0)
			hidden = false;
	}

	if (bPartPos == 1)
		ch->SetBodyCostumeHidden(hidden);
	else if (bPartPos == 2)
		ch->SetHairCostumeHidden(hidden);
	else if (bPartPos == 3)
		ch->SetAcceCostumeHidden(hidden);
	else if (bPartPos == 4)
		ch->SetWeaponCostumeHidden(hidden);
	else
		return;

	NetworkSyncSystem::UpdatePacket(character);
}
#endif

#ifdef ENABLE_RUNE_SYSTEM
#include "shop.h"
#include "shop_manager.h"
#include <common/rune_length.h>

ACMD(do_rune)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;
	const entt::entity owner = character;

	char arg1[512];
	const char* rest = one_argument(argument, arg1, sizeof(arg1));
	switch (arg1[0])
	{
		case 'a':
			{
				one_argument(rest, arg1, sizeof(arg1));
				int slot;
				if (str_to_number(slot, arg1) == false)
					return;

				if (slot == WEAR_RUNE7)
					return;

				const entt::entity item = ItemSystem::GetWearItem(owner, slot);
				if (ItemSystem::IsValidItem(item))
					ItemSystem::ActivateRuneLegacyBoundary(item);
			}
			break;
		case 'd':
			{
				one_argument(rest, arg1, sizeof(arg1));
				int slot;
				if (str_to_number(slot, arg1) == false)
					return;

				if (slot == WEAR_RUNE7)
					return;

				const entt::entity item = ItemSystem::GetWearItem(owner, slot);
				if (ItemSystem::IsValidItem(item))
					ItemSystem::DeactivateRuneLegacyBoundary(item);
			}
			break;
		case 'l':
			{
				one_argument(rest, arg1, sizeof(arg1));
				int w;
				if (str_to_number(w, arg1) == false)
					return;

				int iMaxSubTypes = RUNE_SUBTYPES - 1;
				for (int i = 0; i < iMaxSubTypes; i++) {
					const entt::entity item = ItemSystem::GetWearItem(
						owner, WEAR_RUNE1 + i);
					if (ItemSystem::IsValidItem(item)) {
						if (w == 0)
							ItemSystem::DeactivateRuneLegacyBoundary(item);
						else
							ItemSystem::ActivateRuneLegacyBoundary(item);
					}
				}
			}
			break;
	}
}

ACMD(do_rune_charge)
{
	if (!ecs::PlayerRuntime::IsValid(character))
		return;
	const entt::entity owner = character;

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if ((!*arg1) || (!*arg2))
		return;

	int iArg1 = 0;
	if (str_to_number(iArg1, arg1) == false)
		return;

	int iArg2 = 0;
	if (str_to_number(iArg2, arg2) == false)
		return;

	entt::entity rune = ItemSystem::GetWearItem(owner, iArg1);
	if (!ItemSystem::IsValidItem(rune))
		return;

	if (!ItemSystem::IsRuneItem(rune))
		return;
	else if (ItemSystem::GetItemSubType(rune) == RUNE_SLOT7)
		return;

	entt::entity bottle = ItemSystem::GetInventoryItem(owner, iArg2);
	if (!ItemSystem::IsValidItem(bottle))
		return;

	if (ItemSystem::GetItemType(bottle) != ITEM_USE ||
		ItemSystem::GetItemSubType(bottle) != USE_RUNE_PERC_CHARGE)
		return;

	if (ItemSystem::GetItemCount(bottle) > 1) {
		const int pos = ItemSystem::GetEmptyInventoryPositionEcs(owner, bottle);
		if (pos == -1) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 366, "");
#endif
			return;
		}

		const uint32_t bottleVnum = ItemSystem::GetItemVnum(bottle);
		ItemSystem::ConsumeItemEcs(bottle, 1);
		const entt::entity splitBottle = ItemSystem::CreateItemEcs(bottleVnum, 1);
		if (!ItemSystem::IsValidItem(splitBottle))
			return;
		if (!ItemSystem::PlaceItemEcs(owner, splitBottle, INVENTORY, pos)) {
			ItemSystem::DestroyItemEntityEcs(splitBottle, "RUNE_BOTTLE_SPLIT_FAIL");
			return;
		}
		bottle = splitBottle;
	}

	int32_t lBottlePercent = ItemSystem::GetItemSocket(bottle, 0);
	if (lBottlePercent < 1)
		return;

	int32_t lMaxTime = ItemSystem::GetItemValue(rune, 0);
	int32_t lOnePercent = lMaxTime / 100;
	if (lOnePercent <= 0)
		return;
	int32_t lRemainPercent =
		ItemSystem::GetItemSocket(rune, ITEM_SOCKET_REMAIN_SEC) / lOnePercent;
	if (lRemainPercent > 99) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 33, "%s",
			ItemSystem::GetItemName(rune));
#endif
		return;
	}

	int32_t dif = 100 - lRemainPercent;
	dif = dif > lBottlePercent ? lBottlePercent : dif;
	int32_t add = lOnePercent * dif;
	int32_t lValue = ItemSystem::GetItemSocket(rune, ITEM_SOCKET_REMAIN_SEC) + add;
	ItemSystem::SetItemSocket(rune, ITEM_SOCKET_REMAIN_SEC, lValue);
#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(owner, CHAT_TYPE_INFO, 34, "%s#%d",
		ItemSystem::GetItemName(rune), dif);
#endif
	ItemSystem::SetItemSocket(bottle, 0, lBottlePercent - dif);
	if (ItemSystem::GetItemSocket(bottle, 0) < 1)
		ItemSystem::RemoveItemFromCharacterLegacyBoundary(bottle);

	ItemSystem::ChangeRuneAttributesLegacyBoundary(rune, lValue);
	if (!AffectSystem::FindAffect(owner, AFFECT_RUNE2) &&
		ItemSystem::GetItemSocket(rune, 1) == 1) {
		if (int32_t(lValue / lOnePercent) >= 50) {
			ItemSystem::ActivateRuneBonusLegacyBoundary(rune);
		}
	}
}

ACMD(do_rune_shop)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (!ch)
		return;

	if (ch->IsOpenSafebox() || ecs::SocialSystem::GetExchange(character) || ch->GetMyShop() || ch->IsCubeOpen())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 294, "");
#endif
		return;
	}

	LPSHOP pkShop = CShopManager::instance().Get(RUNE_SHOP);
	if (pkShop) {
		pkShop->AddGuest(ch, 0, false);
		ch->SetShopOwner(entt::null);
	}
}

ACMD(do_rune_effect)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	int iArg1 = 0;
	if (str_to_number(iArg1, arg1) == false)
		return;

	if ((iArg1 != 0) && (iArg1 != 1))
		return;

	if (ecs::QuestSystem::GetFlag(character, "rune.hide_effect") == iArg1)
		return;

	ecs::QuestSystem::SetFlag(character, "rune.hide_effect", iArg1);
	ch->ComputePoints();
	NetworkSyncSystem::UpdatePacket(character);
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "rune_affect %d", iArg1);
}
#endif
#ifdef ENABLE_EVENT_MANAGER
ACMD(do_event_manager)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	std::vector<std::string> vecArgs;
	split_argument(argument, vecArgs);
	if (vecArgs.size() < 2) { return; }
	else if (vecArgs[1] == "info")
	{
		CHARACTER_MANAGER::Instance().SendDataPlayer(character);
	}
	else if (vecArgs[1] == "remove")
	{
		if (!ch->IsGM())
			return;

		if (vecArgs.size() < 3) {

			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "put the event index!!");
			return;
		}

		uint8_t removeIndex;
		str_to_number(removeIndex, vecArgs[2].c_str());

		if(CHARACTER_MANAGER::Instance().CloseEventManuel(removeIndex))
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "successfuly remove!");
		else
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "dont has any event!");
	}
	else if (vecArgs[1] == "update")
	{
		if (!ch->IsGM())
			return;
		const uint8_t subHeader = EVENT_MANAGER_UPDATE;
		//db_clientdesc->DBPacketHeader(HEADER_GD_EVENT_MANAGER, 0, sizeof(uint8_t));
		//db_clientdesc->Packet(&subHeader, sizeof(uint8_t));
		db_clientdesc->DBPacket(HEADER_GD_EVENT_MANAGER, 0, &subHeader, sizeof(uint8_t));

		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "successfully update!");
	}
}
#endif
#ifdef ENABLE_ITEMSHOP
ACMD(do_ishop)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	std::vector<std::string> vecArgs;
	split_argument(argument, vecArgs);
	if (vecArgs.size() < 2) { return; }
	else if (vecArgs[1] == "data")
	{
		if (ch->GetProtectTime("itemshop.load") == 1)
			return;
		ch->SetProtectTime("itemshop.load", 1);
		if (vecArgs.size() < 3) { return; }
		int updateTime;
		str_to_number(updateTime, vecArgs[2].c_str());
		CHARACTER_MANAGER::Instance().LoadItemShopData(character, CHARACTER_MANAGER::Instance().GetItemShopUpdateTime() != updateTime);
	}
	else if (vecArgs[1] == "log")
	{
		if (ch->GetProtectTime("itemshop.log") == 1)
			return;
		ch->SetProtectTime("itemshop.log", 1);

		CHARACTER_MANAGER::Instance().LoadItemShopLog(character);
	}
	else if (vecArgs[1] == "buy")
	{
		if (vecArgs.size() < 4) { return; }
		int itemID;
		str_to_number(itemID, vecArgs[2].c_str());
		int itemCount;
		str_to_number(itemCount, vecArgs[3].c_str());
		CHARACTER_MANAGER::Instance().LoadItemShopBuy(character, itemID, itemCount);
	}
}
#endif

#ifdef ENABLE_GUILD_RENEWAL_BY_RAZOR93_ERES_KOLBASZ

#include <ctime>

static bool ParseDateToEpochEndOfDay(const char* s, time_t& outEpoch)
{
	// Accept epoch seconds
	if (!s || !*s) return false;
	bool allDigit = true;
	for (const char* p = s; *p; ++p)
	{
		if (*p < '0' || *p > '9') { allDigit = false; break; }
	}
	if (allDigit && strlen(s) >= 9)
	{
		outEpoch = (time_t)atoll(s);
		return true;
	}

	// Accept YYYY.MM.DD or YYYY-MM-DD
	int y=0,m=0,d=0;
	if (sscanf(s, "%d.%d.%d", &y, &m, &d) != 3)
	{
		if (sscanf(s, "%d-%d-%d", &y, &m, &d) != 3)
			return false;
	}
	if (y < 1970 || m < 1 || m > 12 || d < 1 || d > 31)
		return false;
	std::tm t{};
	t.tm_year = y - 1900;
	t.tm_mon = m - 1;
	t.tm_mday = d;
	t.tm_hour = 23;
	t.tm_min = 59;
	t.tm_sec = 59;
	outEpoch = mktime(&t);
	return outEpoch > 0;
}

ACMD(do_gr_open)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	CGuild* g = ch ? ecs::SocialSystem::GetGuild(character) : nullptr;
	if (!g)
		return;
	ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "GuildRenewalOpen");
	g->SendRenewalInfoTo(ch);
}

ACMD(do_gr_load)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	CGuild* g = ch ? ecs::SocialSystem::GetGuild(character) : nullptr;
	if (!g)
		return;
	g->RequestRenewalLoad();
	g->SendRenewalInfoTo(ch);
}

ACMD(do_gr_deposit_item)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	CGuild* g = ch ? ecs::SocialSystem::GetGuild(character) : nullptr;
	if (!g)
		return;

	char arg1[256], arg2[256];
	argument = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	const int cell = atoi(arg1);
	uint32_t count = (uint32_t)atoi(arg2);
	if (cell < 0 || count == 0)
		return;

	const entt::entity owner = character;
	const entt::entity item = ItemSystem::GetInventoryItem(owner, cell);
	if (!ItemSystem::IsValidItem(item))
		return;
	if (count > ItemSystem::GetItemCount(item))
		count = ItemSystem::GetItemCount(item);

	const uint32_t vnum = ItemSystem::GetItemVnum(item);

	// Remove from player
	if (count >= ItemSystem::GetItemCount(item))
		ItemSystem::DestroyItemEntityEcs(item, "GUILD_RENEWAL_DEPOSIT");
	else
		ItemSystem::ConsumeItemEcs(item, count);

	g->RenewalDepositItem(ch, vnum, count, false);
	g->SendRenewalInfoTo(ch);
}

ACMD(do_gr_deposit_money)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	CGuild* g = ch ? ecs::SocialSystem::GetGuild(character) : nullptr;
	if (!g)
		return;
	const int64_t amount = atoll(argument);
	if (amount <= 0)
		return;
	if ((int64_t)ecs::PointSystem::GetGold(character) < amount)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Not enough yang.");
		return;
	}
	ecs::PointSystem::Change(character, POINT_GOLD, (long)-amount, true);
	g->RenewalDepositMoney(ch, amount, false);
	g->SendRenewalInfoTo(ch);
}

ACMD(do_gr_set_tax)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	CGuild* g = ch ? ecs::SocialSystem::GetGuild(character) : nullptr;
	if (!g)
		return;
	if (!g->IsGuildMaster((ecs::PlayerRuntime::GetPlayerID(character))))
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Only guild leader can set tax request.");
		return;
	}

	char dateArg[256];
	char moneyArg[256];
	argument = two_arguments(argument, dateArg, sizeof(dateArg), moneyArg, sizeof(moneyArg));
	if (!*dateArg || !*moneyArg)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Usage: gr_set_tax <YYYY.MM.DD> <yang> <vnum1> <count1> ... <vnum5> <count5>");
		return;
	}

	int y=0, mo=0, d=0;
	if (3 != sscanf(dateArg, "%d.%d.%d", &y, &mo, &d))
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Invalid date format. Use YYYY.MM.DD");
		return;
	}
	// Deadline: end of that day
	tm t{};
	t.tm_year = y - 1900;
	t.tm_mon = mo - 1;
	t.tm_mday = d;
	t.tm_hour = 23;
	t.tm_min = 59;
	t.tm_sec = 59;
	t.tm_isdst = -1;
	time_t deadline = mktime(&t);
	if (deadline <= 0)
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Invalid deadline.");
		return;
	}

	TGuildRenewalTaxRequestData tax{};
	tax.bActive = 1;
	tax.dwDeadline = (uint32_t)deadline;
	tax.llMoneyPerMember = atoll(moneyArg);
	if (tax.llMoneyPerMember < 0)
		tax.llMoneyPerMember = 0;

	// Parse up to 5 (vnum,count) pairs
	for (int i = 0; i < GUILD_RENEWAL_REQ_ITEM_COUNT; ++i)
	{
		char vnumArg[256];
		char cntArg[256];
		argument = two_arguments(argument, vnumArg, sizeof(vnumArg), cntArg, sizeof(cntArg));
		if (!*vnumArg || !*cntArg)
			break;
		uint32_t vnum = (uint32_t)strtoul(vnumArg, nullptr, 10);
		uint32_t cnt = (uint32_t)strtoul(cntArg, nullptr, 10);
		tax.adwItemVnum[i] = vnum;
		tax.adwItemCount[i] = cnt;
	}

	if (!g->RenewalSetTax(ch, tax))
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Failed to set tax request.");
		return;
	}

	// Notify everyone online in the guild
	g->SendRenewalInfoToOnlineMembers();
	ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Guild tax request has been set.");
}

ACMD(do_gr_pay_tax)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	CGuild* g = ch ? ecs::SocialSystem::GetGuild(character) : nullptr;
	if (!g)
		return;
	std::string reason;
	if (!g->RenewalPayTax(ch, &reason))
	{
		ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s", reason.c_str());
	}
	g->SendRenewalInfoTo(ch);
}

ACMD(do_gr_levelup)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	CGuild* g = ch ? ecs::SocialSystem::GetGuild(character) : nullptr;
	if (!g)
		return;
	std::string reason;
	if (!g->DoRenewalLevelUp(ch, &reason))
	{
		if (!reason.empty())
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "%s", reason.c_str());
		else
			ecs::ChatSystem::Send(character, CHAT_TYPE_INFO, "Guild cannot be upgraded.");
	}
	g->SendRenewalInfoTo(ch);
}

#endif // ENABLE_GUILD_RENEWAL_BY_RAZOR93
