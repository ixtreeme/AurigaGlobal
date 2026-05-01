#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include <sstream>
#include "constants.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "log.h"
#include "questmanager.h"
#include "questlua.h"
#include "questevent.h"
#include "config.h"
#include "mining.h"
#include "fishing.h"
#include "priv_manager.h"
#include "utils.h"
#include "p2p.h"
#include "item_manager.h"
#include "mob_manager.h"
#include "start_position.h"
#include "over9refine.h"
#include "OXEvent.h"
#include "regen.h"
#include "cmd.h"
#include "guild.h"
#include "guild_manager.h"
#include "sectree_manager.h"

#include "desc.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"
#include "ecs/systems/ItemSystem.hpp"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif
#ifdef ENABLE_NEWSTUFF
#include "db.h"
#endif
extern ACMD(do_block_chat);

namespace quest
{
	ALUA(_get_locale)
	{
		// migrated from CHARACTER::get_locale
		// DUAL-PATH: legacy only during migration window
		lua_pushstring(L, g_stLocale.c_str());
		return 1;
	}

	ALUA(_number)
	{
		// migrated from CHARACTER::number
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
			lua_pushnumber(L, 0);
		else
			lua_pushnumber(L, number((int)lua_tonumber(L, 1), (int)lua_tonumber(L, 2)));
		return 1;
	}

	ALUA(_time_to_str)
	{
		// migrated from CHARACTER::time_to_str
		// DUAL-PATH: legacy only during migration window
		time_t curTime = (time_t)lua_tonumber(L, -1);
		lua_pushstring(L, asctime(gmtime(&curTime)));
		return 1;
	}

	ALUA(_say)
	{
		// migrated from CHARACTER::say
		// DUAL-PATH: legacy only during migration window
		ostringstream s;
		combine_lua_string(L, s);
		CQuestManager::Instance().AddScript(s.str() + "[ENTER]");
		return 0;
	}

	ALUA(_chat)
	{
		// migrated from CHARACTER::chat
		// DUAL-PATH: legacy only during migration window
		ostringstream s;
		combine_lua_string(L, s);

		ecs::ChatSystem::Send(AIHelpers::EcsOf(CQuestManager::Instance().GetCurrentCharacterPtr()), CHAT_TYPE_TALKING, "%s", s.str().c_str());
		return 0;
	}

	ALUA(_cmdchat)
	{
		// migrated from CHARACTER::cmdchat
		// DUAL-PATH: legacy only during migration window
		ostringstream s;
		combine_lua_string(L, s);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(CQuestManager::Instance().GetCurrentCharacterPtr()), CHAT_TYPE_COMMAND, "%s", s.str().c_str());
		return 0;
	}

	ALUA(_syschat)
	{
		// migrated from CHARACTER::syschat
		// DUAL-PATH: legacy only during migration window
		ostringstream s;
		combine_lua_string(L, s);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(CQuestManager::Instance().GetCurrentCharacterPtr()), CHAT_TYPE_INFO, "%s", s.str().c_str());
		return 0;
	}

	ALUA(_notice)
	{
		// migrated from CHARACTER::notice
		// DUAL-PATH: legacy only during migration window
		ostringstream s;
		combine_lua_string(L, s);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(CQuestManager::Instance().GetCurrentCharacterPtr()), CHAT_TYPE_NOTICE, "%s", s.str().c_str());
		return 0;
	}

	ALUA(_left_image)
	{
		// migrated from CHARACTER::left_image
		// DUAL-PATH: legacy only during migration window
		if (lua_isstring(L, -1))
		{
			string s = lua_tostring(L,-1);
			CQuestManager::Instance().AddScript("[LEFTIMAGE src;"+s+"]");
		}
		return 0;
	}

	ALUA(_top_image)
	{
		// migrated from CHARACTER::top_image
		// DUAL-PATH: legacy only during migration window
		if (lua_isstring(L, -1))
		{
			string s = lua_tostring(L,-1);
			CQuestManager::Instance().AddScript("[TOPIMAGE src;"+s+"]");
		}
		return 0;
	}

	ALUA(_set_skin) // Quest UI style
	{
		// migrated from CHARACTER::set_skin
		// DUAL-PATH: legacy only during migration window
		if (lua_isnumber(L, -1))
		{
			CQuestManager::Instance().SetSkinStyle((int)rint(lua_tonumber(L,-1)));
		}
		else
		{
			sys_err("QUEST wrong skin index");
		}

		return 0;
	}

	ALUA(_set_server_timer)
	{
		// migrated from CHARACTER::set_server_timer
		// DUAL-PATH: legacy only during migration window
		int n = lua_gettop(L);
		if ((n != 2 || !lua_isnumber(L, 2) || !lua_isstring(L, 1)) &&
				(n != 3 || !lua_isstring(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)))
		{
			sys_err("QUEST set_server_timer argument count wrong.");
			return 0;
		}

		const char * name = lua_tostring(L, 1);
		double t = lua_tonumber(L, 2);
		uint32_t arg = 0;

		CQuestManager & q = CQuestManager::instance();

		if (lua_isnumber(L, 3))
			arg = (uint32_t) lua_tonumber(L, 3);

		int timernpc = q.LoadTimerScript(name);

		LPEVENT event = quest_create_server_timer_event(name, t, timernpc, false, arg);
		q.AddServerTimer(name, arg, event);
		return 0;
	}

	ALUA(_set_server_loop_timer)
	{
		// migrated from CHARACTER::set_server_loop_timer
		// DUAL-PATH: legacy only during migration window
		int n = lua_gettop(L);
		if ((n != 2 || !lua_isnumber(L, 2) || !lua_isstring(L, 1)) &&
				(n != 3 || !lua_isstring(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3)))
		{
			sys_err("QUEST set_server_timer argument count wrong.");
			return 0;
		}
		const char * name = lua_tostring(L, 1);
		double t = lua_tonumber(L, 2);
		uint32_t arg = 0;
		CQuestManager & q = CQuestManager::instance();

		if (lua_isnumber(L, 3))
			arg = (uint32_t) lua_tonumber(L, 3);

		int timernpc = q.LoadTimerScript(name);

		LPEVENT event = quest_create_server_timer_event(name, t, timernpc, true, arg);
		q.AddServerTimer(name, arg, event);
		return 0;
	}

	ALUA(_clear_server_timer)
	{
		// migrated from CHARACTER::clear_server_timer
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();
		const char * name = lua_tostring(L, 1);
		uint32_t arg = (uint32_t) lua_tonumber(L, 2);
		q.ClearServerTimer(name, arg);
		return 0;
	}

	ALUA(_set_named_loop_timer)
	{
		// migrated from CHARACTER::set_named_loop_timer
		// DUAL-PATH: legacy only during migration window
		int n = lua_gettop(L);

		if (n != 2 || !lua_isnumber(L, -1) || !lua_isstring(L, -2))
			sys_err("QUEST set_timer argument count wrong.");
		else
		{
			const char * name = lua_tostring(L, -2);
			double t = lua_tonumber(L, -1);

			CQuestManager & q = CQuestManager::instance();
			int timernpc = q.LoadTimerScript(name);
			q.GetCurrentPC()->AddTimer(name, quest_create_timer_event(name, ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(q.GetCurrentCharacterPtr())), t, timernpc, true));
		}

		return 0;
	}

	ALUA(_get_server_timer_arg)
	{
		// migrated from CHARACTER::get_server_timer_arg
		// DUAL-PATH: legacy only during migration window
		lua_pushnumber(L, CQuestManager::instance().GetServerTimerArg());
		return 1;
	}

	ALUA(_set_timer)
	{
		// migrated from CHARACTER::set_timer
		// DUAL-PATH: legacy only during migration window
		if (lua_gettop(L) != 1 || !lua_isnumber(L, -1))
			sys_err("QUEST invalid argument.");
		else
		{
			double t = lua_tonumber(L, -1);

			CQuestManager& q = CQuestManager::instance();
			quest_create_timer_event("", ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(q.GetCurrentCharacterPtr())), t);
		}

		return 0;
	}

	ALUA(_set_named_timer)
	{
		// migrated from CHARACTER::set_named_timer
		// DUAL-PATH: legacy only during migration window
		int n = lua_gettop(L);

		if (n != 2 || !lua_isnumber(L, -1) || !lua_isstring(L, -2))
		{
			sys_err("QUEST set_timer argument count wrong.");
		}
		else
		{
			const char * name = lua_tostring(L,-2);
			double t = lua_tonumber(L, -1);

			CQuestManager & q = CQuestManager::instance();
			int timernpc = q.LoadTimerScript(name);
			q.GetCurrentPC()->AddTimer(name, quest_create_timer_event(name, ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(q.GetCurrentCharacterPtr())), t, timernpc));
		}

		return 0;
	}

	ALUA(_timer)
	{
		// migrated from CHARACTER::timer
		// DUAL-PATH: legacy only during migration window
		if (lua_gettop(L) == 1)
			return _set_timer(L);
		else
			return _set_named_timer(L);
	}

	ALUA(_clear_named_timer)
	{
		// migrated from CHARACTER::clear_named_timer
		// DUAL-PATH: legacy only during migration window
		int n = lua_gettop(L);

		if (n != 1 || !lua_isstring(L, -1))
			sys_err("QUEST set_timer argument count wrong.");
		else
		{
			CQuestManager & q = CQuestManager::instance();
			q.GetCurrentPC()->RemoveTimer(lua_tostring(L, -1));
		}

		return 0;
	}

	ALUA(_getnpcid)
	{
		// migrated from CHARACTER::getnpcid
		// DUAL-PATH: legacy only during migration window
		const char * name = lua_tostring(L, -1);
		CQuestManager & q = CQuestManager::instance();
		lua_pushnumber(L, q.FindNPCIDByName(name));
		return 1;
	}

	ALUA(_is_test_server)
	{
		// migrated from CHARACTER::is_test_server
		// DUAL-PATH: legacy only during migration window
		lua_pushboolean(L, test_server);
		return 1;
	}

	ALUA(_raw_script)
	{
		// migrated from CHARACTER::raw_script
		// DUAL-PATH: legacy only during migration window
		if ( test_server )
			LOG_INFO("_raw_script : {} ", lua_tostring(L,-1));
		if (lua_isstring(L, -1))
			CQuestManager::Instance().AddScript(lua_tostring(L,-1));
		else
			sys_err("QUEST wrong argument: questname: {}", CQuestManager::instance().GetCurrentQuestName().c_str());

		return 0;
	}

	ALUA(_char_log)
	{
		// migrated from CHARACTER::char_log
		// DUAL-PATH: legacy only during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		uint32_t what = 0;
		const char* how = "";
		const char* hint = "";

		if (lua_isnumber(L, 1)) what = (uint32_t)lua_tonumber(L, 1);
		if (lua_isstring(L, 2)) how = lua_tostring(L, 2);
		if (lua_tostring(L, 3)) hint = lua_tostring(L, 3);

		LogManager::instance().CharLog(ch, what, how, hint);
		return 0;
	}

	ALUA(_item_log)
	{
		// migrated from CHARACTER::item_log
		// DUAL-PATH: legacy only during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		uint32_t dwItemID = 0;
		const char* how = "";
		const char* hint = "";

		if ( lua_isnumber(L, 1) ) dwItemID = (uint32_t)lua_tonumber(L, 1);
		if ( lua_isstring(L, 2) ) how = lua_tostring(L, 2);
		if ( lua_tostring(L, 3) ) hint = lua_tostring(L, 3);

		const entt::entity item = ItemSystem::FindItemByID(dwItemID);

		if (item != entt::null)
			LogManager::instance().ItemLogEntity(ch, item, how, hint);

		return 0;
	}

	ALUA(_syslog)
	{
		// migrated from CHARACTER::syslog
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2))
			return 0;

		if (lua_tonumber(L, 1) >= 1)
		{
			if (!test_server)
				return 0;
		}

		PC* pc = CQuestManager::instance().GetCurrentPC();

		if (!pc)
			return 0;

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
			return 0;

		LOG_INFO("QUEST: quest: {} player: {} : {}", pc->GetCurrentQuestName().c_str(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), lua_tostring(L, 2));
		return 0;
	}

	ALUA(_syserr)
	{
		// migrated from CHARACTER::syserr
		// DUAL-PATH: legacy only during migration window
		if (!lua_isstring(L, 1))
			return 0;

		PC* pc = CQuestManager::instance().GetCurrentPC();

		if (!pc)
			return 0;

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
			return 0;

		sys_err("QUEST: quest: {} player: {} : {}", pc->GetCurrentQuestName().c_str(), ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), lua_tostring(L, 1));
		return 0;
	}

	// LUA_ADD_BGM_INFO
	ALUA(_set_bgm_volume_enable)
	{
		// migrated from CHARACTER::set_bgm_volume_enable
		// DUAL-PATH: legacy only during migration window
		CHARACTER_SetBGMVolumeEnable();

		return 0;
	}

	ALUA(_add_bgm_info)
	{
		// migrated from CHARACTER::add_bgm_info
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1) || !lua_isstring(L, 2))
			return 0;

		int mapIndex		= (int)lua_tonumber(L, 1);

		const char*	bgmName	= lua_tostring(L, 2);
		if (!bgmName)
			return 0;

		float bgmVol = lua_isnumber(L, 3) ? lua_tonumber(L, 3) : (1.0f/5.0f)*0.1f;

		CHARACTER_AddBGMInfo(mapIndex, bgmName, bgmVol);

		return 0;
	}
	// END_OF_LUA_ADD_BGM_INFO

	// LUA_ADD_GOTO_INFO
	ALUA(_add_goto_info)
	{
		// migrated from CHARACTER::add_goto_info
		// DUAL-PATH: legacy only during migration window
		const char* name = lua_tostring(L, 1);

		int empire 	= (int)lua_tonumber(L, 2);
		int mapIndex 	= (int)lua_tonumber(L, 3);
		int x 		= (int)lua_tonumber(L, 4);
		int y 		= (int)lua_tonumber(L, 5);

		if (!name)
			return 0;

		CHARACTER_AddGotoInfo(name, empire, mapIndex, x, y);
		return 0;
	}
	// END_OF_LUA_ADD_GOTO_INFO

	// REFINE_PICK
	ALUA(_refine_pick)
	{
		// migrated from CHARACTER::refine_pick
		// DUAL-PATH: legacy only during migration window
		uint8_t bCell = (uint8_t) lua_tonumber(L,-1);

		CQuestManager& q = CQuestManager::instance();

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		LPITEM item = ch->GetInventoryItem(bCell);

		int ret = mining::RealRefinePick(ch, EntityFactory::CreateItemEntity(g_registry, item));
		lua_pushnumber(L, ret);
		return 1;
	}
	// END_OF_REFINE_PICK

	ALUA(_fish_real_refine_rod)
	{
		// migrated from CHARACTER::fish_real_refine_rod
		// DUAL-PATH: legacy only during migration window
		uint8_t bCell = (uint8_t) lua_tonumber(L,-1);

		CQuestManager& q = CQuestManager::instance();

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		LPITEM item = ch->GetInventoryItem(bCell);

		int ret = fishing::RealRefineRod(ch, item);
		lua_pushnumber(L, ret);
		return 1;
	}

	ALUA(_give_char_privilege)
	{
		// migrated from CHARACTER::give_char_privilege
		// DUAL-PATH: legacy only during migration window
		int pid = ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(CQuestManager::instance().GetCurrentCharacterPtr()));
		int type = (int)lua_tonumber(L, 1);
		int value = (int)lua_tonumber(L, 2);

		if (MAX_PRIV_NUM <= type)
		{
			sys_err("PRIV_MANAGER: _give_char_privilege: wrong empire priv type({})", type);
			return 0;
		}

		CPrivManager::instance().RequestGiveCharacterPriv(pid, type, value);

		return 0;
	}

	ALUA(_give_empire_privilege)
	{
		// migrated from CHARACTER::give_empire_privilege
		// DUAL-PATH: legacy only during migration window
		int empire = (int)lua_tonumber(L,1);
		int type = (int)lua_tonumber(L, 2);
		int value = (int)lua_tonumber(L, 3);
		int time = (int) lua_tonumber(L,4);
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (MAX_PRIV_NUM <= type)
		{
			sys_err("PRIV_MANAGER: _give_empire_privilege: wrong empire priv type({})", type);
			return 0;
		}

		if (ch)
			LOG_INFO("_give_empire_privileage(empire={}, type={}, value={}, time={}), by quest, {}", empire, type, value, time, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
		else
			LOG_INFO("_give_empire_privileage(empire={}, type={}, value={}, time={}), by quest, NULL", empire, type, value, time);

		CPrivManager::instance().RequestGiveEmpirePriv(empire, type, value, time);
		return 0;
	}

	ALUA(_give_guild_privilege)
	{
		// migrated from CHARACTER::give_guild_privilege
		// DUAL-PATH: legacy only during migration window
		int guild_id = (int)lua_tonumber(L,1);
		int type = (int)lua_tonumber(L, 2);
		int value = (int)lua_tonumber(L, 3);
		int time = (int)lua_tonumber( L, 4 );

		if (MAX_PRIV_NUM <= type)
		{
			sys_err("PRIV_MANAGER: _give_guild_privilege: wrong empire priv type({})", type);
			return 0;
		}

		LOG_INFO("_give_guild_privileage(empire={}, type={}, value={}, time={})", guild_id, type, value, time);

		CPrivManager::instance().RequestGiveGuildPriv(guild_id,type,value,time);

		return 0;
	}

	ALUA(_get_empire_privilege_string)
	{
		// migrated from CHARACTER::get_empire_privilege_string
		// DUAL-PATH: legacy only during migration window
		int empire = (int) lua_tonumber(L, 1);
		ostringstream os;
		bool found = false;

		for (int type = PRIV_NONE + 1; type < MAX_PRIV_NUM; ++type)
		{
			CPrivManager::SPrivEmpireData* pkPrivEmpireData = CPrivManager::instance().GetPrivByEmpireEx(empire, type);

			if (pkPrivEmpireData && pkPrivEmpireData->m_value)
			{
				if (found)
					os << ", ";

				os << c_apszPrivNames[type] << " : " <<
					pkPrivEmpireData->m_value << "%" << " (" <<
					((pkPrivEmpireData->m_end_time_sec-get_global_time())/3600.0f) << " hours)" << endl;
				found = true;
			}
		}

		if (!found)
			os << "None!" << endl;

		lua_pushstring(L, os.str().c_str());
		return 1;
	}

	ALUA(_get_empire_privilege)
	{
		// migrated from CHARACTER::get_empire_privilege
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L,1) || !lua_isnumber(L,2))
		{
			lua_pushnumber(L,0);
			return 1;
		}
		int empire = (int)lua_tonumber(L,1);
		int type = (int)lua_tonumber(L,2);
		int value = CPrivManager::instance().GetPrivByEmpire(empire,type);
		lua_pushnumber(L, value);
		return 1;
	}

	ALUA(_get_guild_privilege_string)
	{
		// migrated from CHARACTER::get_guild_privilege_string
		// DUAL-PATH: legacy only during migration window
		int guild = (int) lua_tonumber(L,1);
		ostringstream os;
		bool found = false;

		for (int type = PRIV_NONE+1; type < MAX_PRIV_NUM; ++type)
		{
			const CPrivManager::SPrivGuildData* pPrivGuildData = CPrivManager::instance().GetPrivByGuildEx( guild, type );

			if (pPrivGuildData && pPrivGuildData->value)
			{
				if (found)
					os << ", ";

				os << c_apszPrivNames[type] << " : " << pPrivGuildData->value << "%"
					<< " (" << ((pPrivGuildData->end_time_sec - get_global_time()) / 3600.0f) << " hours)" << endl;
				found = true;
			}
		}

		if (!found)
			os << "None!" << endl;

		lua_pushstring(L, os.str().c_str());
		return 1;
	}

	ALUA(_get_guildid_byname)
	{
		// migrated from CHARACTER::get_guildid_byname
		// DUAL-PATH: legacy only during migration window
		if ( !lua_isstring( L, 1 ) ) {
			sys_err("_get_guildid_byname() - invalud argument");
			lua_pushnumber( L, 0 );
			return 1;
		}

		const char* pszGuildName = lua_tostring( L, 1 );
		CGuild* pFindGuild = CGuildManager::instance().FindGuildByName( pszGuildName );
		if ( pFindGuild )
			lua_pushnumber( L, pFindGuild->GetID() );
		else
			lua_pushnumber( L, 0 );

		return 1;
	}

	ALUA(_get_guild_privilege)
	{
		// migrated from CHARACTER::get_guild_privilege
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L,1) || !lua_isnumber(L,2))
		{
			lua_pushnumber(L,0);
			return 1;
		}
		int guild = (int)lua_tonumber(L,1);
		int type = (int)lua_tonumber(L,2);
		int value = CPrivManager::instance().GetPrivByGuild(guild,type);
		lua_pushnumber(L, value);
		return 1;
	}

	ALUA(_item_name)
	{
		// migrated from CHARACTER::item_name
		// DUAL-PATH: legacy only during migration window
		if (lua_isnumber(L,1))
		{
			uint32_t dwVnum = (uint32_t)lua_tonumber(L,1);
			TItemTable* pTable = ITEM_MANAGER::instance().GetTable(dwVnum);
#ifdef ENABLE_MULTI_NAMES
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
#endif

			if (pTable)
#ifdef ENABLE_MULTI_NAMES
				lua_pushstring(L,pTable->szLocaleName[ch && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetLanguage() : 0]);
#else
				lua_pushstring(L,pTable->szLocaleName);
#endif
			else
				lua_pushstring(L,"");
		}
		else
			lua_pushstring(L,"");
		return 1;
	}

	ALUA(_mob_name)
	{
		// migrated from CHARACTER::mob_name
		// DUAL-PATH: legacy only during migration window
		if (lua_isnumber(L, 1))
		{
			uint32_t dwVnum = (uint32_t) lua_tonumber(L,1);
			const CMob * pkMob = CMobManager::instance().Get(dwVnum);

#ifdef ENABLE_MULTI_NAMES
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
#endif

			if (pkMob)
#ifdef ENABLE_MULTI_NAMES
				lua_pushstring(L, pkMob->m_table.szLocaleName[ch && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)) ? ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetLanguage() : 0]);
#else
				lua_pushstring(L, pkMob->m_table.szLocaleName);
#endif
			else
				lua_pushstring(L, "");
		}
		else
			lua_pushstring(L,"");

		return 1;
	}

	ALUA(_mob_vnum)
	{
		// migrated from CHARACTER::mob_vnum
		// DUAL-PATH: legacy only during migration window
		if (lua_isstring(L,1))
		{
			const char* str = lua_tostring(L, 1);
			const CMob* pkMob = CMobManager::instance().Get(str, false);
			if (pkMob)
				lua_pushnumber(L,pkMob->m_table.dwVnum);
			else
				lua_pushnumber(L,0);
		}
		else
			lua_pushnumber(L,0);

		return 1;
	}

	ALUA(_get_global_time)
	{
		// migrated from CHARACTER::get_global_time
		// DUAL-PATH: legacy only during migration window
		lua_pushnumber(L, get_global_time());
		return 1;
	}


	ALUA(_get_channel_id)
	{
		// migrated from CHARACTER::get_channel_id
		// DUAL-PATH: legacy only during migration window
		lua_pushnumber(L, g_bChannel);

		return 1;
	}

	ALUA(_do_command)
	{
		// migrated from CHARACTER::do_command
		// DUAL-PATH: legacy only during migration window
		if (!lua_isstring(L, 1))
			return 0;

		const char * str = lua_tostring(L, 1);
		size_t len = strlen(str);

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		interpret_command(ch, str, len);
		return 0;
	}

	ALUA(_find_pc)
	{
		// migrated from CHARACTER::find_pc
		// DUAL-PATH: legacy only during migration window
		if (!lua_isstring(L, 1))
		{
			sys_err("invalid argument");
			lua_pushnumber(L, 0);
			return 1;
		}

		const char * name = lua_tostring(L, 1);
		LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(name);
		lua_pushnumber(L, tch ? ((tch)->GetLegacyVID()) : 0);
		return 1;
	}

	ALUA(_find_pc_cond)
	{
		// migrated from CHARACTER::find_pc_cond
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			lua_pushnumber(L, 0);
			return 1;
		}

		int iMinLev = (int) lua_tonumber(L, 1);
		int iMaxLev = (int) lua_tonumber(L, 2);
		unsigned int uiJobFlag = (unsigned int) lua_tonumber(L, 3);

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		LPCHARACTER tch;

		if (test_server)
		{
			LOG_INFO("find_pc_cond map={}, job={}, level={}~{}", ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), uiJobFlag, iMinLev, iMaxLev);
		}

		tch = CHARACTER_MANAGER::instance().FindSpecifyPC(uiJobFlag,
				ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)),
				ch,
				iMinLev,
				iMaxLev);

		lua_pushnumber(L, tch ? ((tch)->GetLegacyVID()) : 0);
		return 1;
	}

	ALUA(_find_npc_by_vnum)
	{
		// migrated from CHARACTER::find_npc_by_vnum
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1))
		{
			sys_err("invalid argument");
			lua_pushnumber(L, 0);
			return 1;
		}

		uint32_t race = (uint32_t) lua_tonumber(L, 1);

		CharacterVectorInteractor i;

		if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(race, i))
		{
			CharacterVectorInteractor::iterator it = i.begin();

			while (it != i.end())
			{
				LPCHARACTER tch = *(it++);

				if (ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(tch)) == ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(CQuestManager::instance().GetCurrentCharacterPtr())))
				{
					lua_pushnumber(L, ((tch)->GetLegacyVID()));
					return 1;
				}
			}
		}

		//"not find(race=%d)", race);

		lua_pushnumber(L, 0);
		return 1;
	}

	// ���ο� state�� �����.
	ALUA(_set_quest_state)
	{
		// migrated from CHARACTER::set_quest_state
		// DUAL-PATH: legacy only during migration window
		if (!lua_isstring(L, 1) || !lua_isstring(L, 2))
			return 0;

		CQuestManager& q = CQuestManager::instance();
		QuestState * pqs = q.GetCurrentState();
		PC* pPC = q.GetCurrentPC();
		//assert(L == pqs->co);
		if (L!=pqs->co)
		{
			luaL_error(L, "running thread != current thread???");
			LOG_INFO("running thread != current thread???");
			return -1;
		}
		if (pPC)
		{
			//const char* szQuestName = lua_tostring(L, 1);
			//const char* szStateName = lua_tostring(L, 2);
			const string stQuestName(lua_tostring(L, 1));
			const string stStateName(lua_tostring(L, 2));
			if ( test_server )
				LOG_INFO("set_state {} {} ", stQuestName.c_str(), stStateName.c_str());
			if (pPC->GetCurrentQuestName() == stQuestName)
			{
				pqs->st = q.GetQuestStateIndex(pPC->GetCurrentQuestName(), lua_tostring(L, -1));
				pPC->SetCurrentQuestStateName(lua_tostring(L,-1));
			}
			else
			{
				pPC->SetQuestState(stQuestName, stStateName);
			}
		}
		return 0;
	}

	ALUA(_get_quest_state)
	{
		// migrated from CHARACTER::get_quest_state
		// DUAL-PATH: legacy only during migration window
		if (!lua_isstring(L, 1) )
			return 0;

		CQuestManager& q = CQuestManager::instance();
		PC* pPC = q.GetCurrentPC();

		if (pPC)
		{
			std::string stQuestName	= lua_tostring(L, 1);
			stQuestName += ".__status";

			int nRet = pPC->GetFlag( stQuestName.c_str() );

			lua_pushnumber(L, nRet );

			if ( test_server )
				LOG_INFO("Get_quest_state name {} value {}", stQuestName.c_str(), nRet);
		}
		else
		{
			if ( test_server )
				LOG_INFO("PC == 0 ");

			lua_pushnumber(L, 0);
		}
		return 1;
	}

	ALUA(_under_han)
	{
		// migrated from CHARACTER::under_han
		// DUAL-PATH: legacy only during migration window
		if (!lua_isstring(L, 1))
			lua_pushboolean(L, 0);
		else
			lua_pushboolean(L, under_han(lua_tostring(L, 1)));
		return 1;
	}

#ifdef ENABLE_FULL_NOTICE
	ALUA(_big_notice)
	{
		// migrated from CHARACTER::big_notice
		// DUAL-PATH: legacy only during migration window
		ostringstream s;
		combine_lua_string(L, s);
		ecs::ChatSystem::Send(AIHelpers::EcsOf(CQuestManager::Instance().GetCurrentCharacterPtr()), CHAT_TYPE_BIG_NOTICE, "%s", s.str().c_str());
		return 0;
	}

	ALUA(_big_notice_in_map)
	{
		// migrated from CHARACTER::big_notice_in_map
		// DUAL-PATH: legacy only during migration window
		const LPCHARACTER pChar = CQuestManager::instance().GetCurrentCharacterPtr();
		if (pChar != nullptr) {
			SendNoticeMap(lua_tostring(L,1), ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(pChar)), true);
		}

		return 0;
	}

	ALUA(_big_notice_all)
	{
		// migrated from CHARACTER::big_notice_all
		// DUAL-PATH: legacy only during migration window
#ifdef TEXTS_IMPROVEMENT
		if (!lua_isnumber(L, 1)) {
			return 0;
		}

		if (!lua_isstring(L, 2)) {
			return 0;
		}

		BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, (uint32_t)lua_tonumber(L, 1), lua_tostring(L, 2));
#else
		ostringstream s;
		combine_lua_string(L, s);

		TPacketGGNotice p;
		p.bHeader = HEADER_GG_BIG_NOTICE;
		p.lSize = strlen(s.str().c_str()) + 1;

		TEMP_BUFFER buf;
		buf.write(&p, sizeof(p));
		buf.write(s.str().c_str(), p.lSize);

		P2P_MANAGER::instance().Send(buf.read_peek(), buf.size()); // HEADER_GG_NOTICE

		SendNotice(s.str().c_str(), true);
#endif
		return 1;
	}
#endif

	ALUA(_notice_all)
	{
		// migrated from CHARACTER::notice_all
		// DUAL-PATH: legacy only during migration window
#ifdef TEXTS_IMPROVEMENT
		if (!lua_isnumber(L, 1)) {
			return 0;
		}

		if (!lua_isstring(L, 2)) {
			return 0;
		}

		BroadcastNoticeNew(CHAT_TYPE_NOTICE, 0, 0, (uint32_t)lua_tonumber(L, 1), lua_tostring(L, 2));
#else
		ostringstream s;
		combine_lua_string(L, s);

		TPacketGGNotice p;
		p.bHeader = HEADER_GG_NOTICE;
		p.lSize = strlen(s.str().c_str()) + 1;

		TEMP_BUFFER buf;
		buf.write(&p, sizeof(p));
		buf.write(s.str().c_str(), p.lSize);

		P2P_MANAGER::instance().Send(buf.read_peek(), buf.size()); // HEADER_GG_NOTICE

		SendNotice(s.str().c_str());
#endif
		return 1;
	}

	EVENTINFO(warp_all_to_village_event_info)
	{
		uint32_t dwWarpMapIndex;

		warp_all_to_village_event_info()
		: dwWarpMapIndex( 0 )
		{
		}
	};

	struct FWarpAllToVillage
	{
		FWarpAllToVillage() {};
		void operator()(LPENTITY ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER ch = (LPCHARACTER) ent;
				if ((ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch))))
				{
					uint8_t bEmpire =  ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch));
					if ( bEmpire == 0 )
					{
						sys_err("Unkonwn Empire {} {} ", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));
						return;
					}

					ch->WarpSet( g_start_position[bEmpire][0], g_start_position[bEmpire][1] );
				}
			}
		}
	};

	EVENTFUNC(warp_all_to_village_event)
	{
		warp_all_to_village_event_info * info = dynamic_cast<warp_all_to_village_event_info *>(event->info);

		if ( info == nullptr)
		{
			sys_err("warp_all_to_village_event> <Factor> Null pointer");
			return 0;
		}

		LPSECTREE_MAP pSecMap = SECTREE_MANAGER::instance().GetMap( info->dwWarpMapIndex );

		if (nullptr != pSecMap)
		{
			FWarpAllToVillage f;
			pSecMap->for_each( f );
		}

		return 0;
	}

	ALUA(_warp_all_to_village)
	{
		// migrated from CHARACTER::warp_all_to_village
		// DUAL-PATH: legacy only during migration window
		int iMapIndex 	= static_cast<int>(lua_tonumber(L, 1));
		int iSec		= static_cast<int>(lua_tonumber(L, 2));

		warp_all_to_village_event_info* info = AllocEventInfo<warp_all_to_village_event_info>();

		info->dwWarpMapIndex = iMapIndex;

		event_create(warp_all_to_village_event, info, PASSES_PER_SEC(iSec));
#ifdef TEXTS_IMPROVEMENT
		SendNoticeNew(CHAT_TYPE_NOTICE, 0, iMapIndex, 586, "");
#endif
		return 0;
	}

	ALUA(_warp_to_village)
	{
		// migrated from CHARACTER::warp_to_village
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (nullptr != ch)
		{
			uint8_t bEmpire = ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch));
			ch->WarpSet( g_start_position[bEmpire][0], g_start_position[bEmpire][1] );
		}

		return 0;
	}

	ALUA(_say_in_map)
	{
		// migrated from CHARACTER::say_in_map
		// DUAL-PATH: legacy only during migration window
		int32_t iMapIndex 		= static_cast<int32_t>(lua_tonumber( L, 1 ));
		std::string Script(lua_tostring( L, 2 ));

		Script += "[ENTER]";
		Script += "[DONE]";

		struct ::packet_script packet_script;

		packet_script.header = HEADER_GC_SCRIPT;
		packet_script.skin = CQuestManager::QUEST_SKIN_NORMAL;
		packet_script.src_size = Script.size();
		packet_script.size = packet_script.src_size + sizeof(struct packet_script);

		FSendPacket f;
		f.buf.write(&packet_script, sizeof(struct packet_script));
		f.buf.write(&Script[0], Script.size());

		LPSECTREE_MAP pSecMap = SECTREE_MANAGER::instance().GetMap( iMapIndex );

		if ( pSecMap )
		{
			pSecMap->for_each( f );
		}

		return 0;
	}

	struct FKillSectree2
	{
		void operator () (LPENTITY ent)
		{
			if (ent->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER ch = (LPCHARACTER) ent;
#ifdef __NEWPET_SYSTEM__
				if (!(ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch))) && !ch->IsPet() && !ch->IsNewPet())
#else
				if (!(ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(ch))) && !ch->IsPet())
#endif
					ch->Dead();
			}
		}
	};

	ALUA(_kill_all_in_map)
	{
		// migrated from CHARACTER::kill_all_in_map
		// DUAL-PATH: legacy only during migration window
		LPSECTREE_MAP pSecMap = SECTREE_MANAGER::instance().GetMap( lua_tonumber(L,1) );

		if (nullptr != pSecMap)
		{
			FKillSectree2 f;
			pSecMap->for_each( f );
		}

		return 0;
	}

	//����: �� ������ �ȵǴ� �ʿ����� ���
	ALUA(_regen_in_map)
	{
		// migrated from CHARACTER::regen_in_map
		// DUAL-PATH: legacy only during migration window
		int32_t iMapIndex = static_cast<int32_t>(lua_tonumber(L, 1));
		std::string szFilename(lua_tostring(L, 2));

		LPSECTREE_MAP pkMap = SECTREE_MANAGER::instance().GetMap(iMapIndex);

		if (pkMap != nullptr)
		{
			regen_load_in_file( szFilename.c_str(), iMapIndex, pkMap->m_setting.iBaseX ,pkMap->m_setting.iBaseY );
		}

		return 0;
	}

	ALUA(_enable_over9refine)
	{
		// migrated from CHARACTER::enable_over9refine
		// DUAL-PATH: legacy only during migration window
		if ( lua_isnumber(L, 1) == true && lua_isnumber(L, 2) == true )
		{
			uint32_t dwVnumFrom = (uint32_t)lua_tonumber(L, 1);
			uint32_t dwVnumTo = (uint32_t)lua_tonumber(L, 2);

			COver9RefineManager::instance().enableOver9Refine(dwVnumFrom, dwVnumTo);
		}

		return 0;
	}

	ALUA(_add_ox_quiz)
	{
		// migrated from CHARACTER::add_ox_quiz
		// DUAL-PATH: legacy only during migration window
		int level = (int)lua_tonumber(L, 1);
		const char* quiz = lua_tostring(L, 2);
		bool answer = lua_toboolean(L, 3);

		if ( COXEventManager::instance().AddQuiz(level, quiz, answer) == false )
		{
			LOG_INFO("OXEVENT : Cannot add quiz. {} {} {}", level, quiz, answer);
		}

		return 1;
	}

	EVENTFUNC(warp_all_to_map_my_empire_event)
	{
		warp_all_to_map_my_empire_event_info * info = dynamic_cast<warp_all_to_map_my_empire_event_info *>(event->info);

		if ( info == nullptr)
		{
			sys_err("warp_all_to_map_my_empire_event> <Factor> Null pointer");
			return 0;
		}

		LPSECTREE_MAP pSecMap = SECTREE_MANAGER::instance().GetMap( info->m_lMapIndexFrom );

		if (pSecMap)
		{
			FWarpEmpire f;

			f.m_lMapIndexTo = info->m_lMapIndexTo;
			f.m_x			= info->m_x;
			f.m_y			= info->m_y;
			f.m_bEmpire		= info->m_bEmpire;

			pSecMap->for_each(f);
		}

		return 0;
	}

	ALUA(_block_chat)
	{
		// migrated from CHARACTER::block_chat
		// DUAL-PATH: legacy only during migration window
		const entt::entity pCharEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* pChar = ecs::LegacyCharOf(pCharEntity);
		if (pChar != nullptr)
		{
			if (lua_isstring(L, 1) != true && lua_isstring(L, 2) != true)
			{
				lua_pushboolean(L, false);
				return 1;
			}

			std::string strName(lua_tostring(L, 1));
			std::string strTime(lua_tostring(L, 2));

			std::string strArg = strName + " " + strTime;

			do_block_chat(pChar, const_cast<char*>(strArg.c_str()), 0, 0);

			lua_pushboolean(L, true);
			return 1;
		}

		lua_pushboolean(L, false);
		return 1;
	}

#ifdef ENABLE_NEWSTUFF
	ALUA(_spawn_mob0)
	{
		// migrated from CHARACTER::spawn_mob0
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		{
			lua_pushnumber(L, -1);
			return 1;
		}
		const uint32_t dwVnum = static_cast<uint32_t>(lua_tonumber(L, 1));
		const int32_t lMapIndex = static_cast<int32_t>(lua_tonumber(L, 2));
		const int32_t dwX = static_cast<int32_t>(lua_tonumber(L, 3));
		const int32_t dwY = static_cast<int32_t>(lua_tonumber(L, 4));

		const CMob* pMonster = CMobManager::instance().Get(dwVnum);
		if (!pMonster)
		{
			lua_pushnumber(L, -2);
			return 1;
		}
		LPSECTREE_MAP pkSectreeMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
		if (!pkSectreeMap)
		{
			lua_pushnumber(L, -3);
			return 1;
		}
		const LPCHARACTER ch = CHARACTER_MANAGER::instance().SpawnMob(dwVnum, lMapIndex, pkSectreeMap->m_setting.iBaseX+dwX*100, pkSectreeMap->m_setting.iBaseY+dwY*100, 0, false, -1);
		lua_pushnumber(L, (ch)?((ch)->GetLegacyVID()):0);
		return 1;
	}
#endif

	ALUA(_spawn_mob)
	{
		// migrated from CHARACTER::spawn_mob
		// DUAL-PATH: ECS update + legacy call during migration window
		if( false == lua_isnumber(L, 1) || false == lua_isnumber(L, 2) || false == lua_isboolean(L, 3) )
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		const uint32_t dwVnum = static_cast<uint32_t>(lua_tonumber(L, 1));
		const uint32_t count = UMINMAX(1, static_cast<uint32_t>(lua_tonumber(L, 2)), 10);
		const bool isAggresive = static_cast<bool>(lua_toboolean(L, 3));
		uint64_t SpawnCount = 0;

		const CMob* pMonster = CMobManager::instance().Get( dwVnum );

		if(nullptr != pMonster )
		{
			const LPCHARACTER pChar = CQuestManager::instance().GetCurrentCharacterPtr();

			for( uint32_t i=0 ; i < count ; ++i )
			{
				const LPCHARACTER pSpawnMonster = CHARACTER_MANAGER::instance().SpawnMobRange( dwVnum,
						ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(pChar)),
						((pChar)->GetX()) - number(200, 750),
						((pChar)->GetY()) - number(200, 750),
						((pChar)->GetX()) + number(200, 750),
						((pChar)->GetY()) + number(200, 750),
						true,
						pMonster->m_table.bType == CHAR_TYPE_STONE,
						isAggresive );

				if(nullptr != pSpawnMonster )
				{
					++SpawnCount;
				// DUAL-PATH: register spawned mob in ECS registry
				if (pSpawnMonster) {
					EntityFactory::CreateMonster(
						g_registry,
						pSpawnMonster->GetMobTable(),
						pSpawnMonster->GetX(),
						pSpawnMonster->GetY(),
						ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(pSpawnMonster)),
						ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(pSpawnMonster)));
				}
				}
			}

			LOG_INFO("QUEST Spawn Monstster: VNUM({}) COUNT({}) isAggresive(%b)", dwVnum, SpawnCount, isAggresive);
		}

		lua_pushnumber(L, SpawnCount);

		return 1;
	}

#ifdef ENABLE_NEWSTUFF
	ALUA(_spawn_mob_in_map)
	{
		// migrated from CHARACTER::spawn_mob_in_map
		// DUAL-PATH: legacy only during migration window
		if( false == lua_isnumber(L, 1) || false == lua_isnumber(L, 2) || false == lua_isboolean(L, 3) || false == lua_isnumber(L, 4) || false == lua_isnumber(L, 5) || false == lua_isnumber(L, 6) )
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		const uint32_t dwVnum = static_cast<uint32_t>(lua_tonumber(L, 1));
		const uint32_t count = UMINMAX(1, static_cast<uint32_t>(lua_tonumber(L, 2)), 10);
		const bool isAggressive = static_cast<bool>(lua_toboolean(L, 3));
		const int32_t iMapIndex = static_cast<int32_t>(lua_tonumber(L, 4));
		const int32_t iMapX = static_cast<int32_t>(lua_tonumber(L, 5));
		const int32_t iMapY = static_cast<int32_t>(lua_tonumber(L, 6));
		uint32_t SpawnCount = 0;
		LOG_INFO("QUEST _spawn_mob_in_map: VNUM({}) COUNT({}) isAggressive(%b) MapIndex({}) MapX({}) MapY({})", dwVnum, count, isAggressive, iMapIndex, iMapX, iMapY);

		PIXEL_POSITION pos;
		if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(iMapIndex, pos))
		{
			sys_err("QUEST _spawn_mob_in_map: cannot find base position in this map {}", iMapIndex);
			lua_pushnumber(L, 0);
			return 1;
		}

		const CMob* pMonster = CMobManager::instance().Get( dwVnum );

		if(nullptr != pMonster )
		{
			for(uint32_t i=0 ; i < count ; ++i )
			{
				const LPCHARACTER pSpawnMonster = CHARACTER_MANAGER::instance().SpawnMobRange(dwVnum,
						iMapIndex,
						pos.x - number(200, 750) + (iMapX * 100),
						pos.y - number(200, 750) + (iMapY * 100),
						pos.x + number(200, 750) + (iMapX * 100),
						pos.y + number(200, 750) + (iMapY * 100),
						true,
						pMonster->m_table.bType == CHAR_TYPE_STONE,
						isAggressive
				);

				if(nullptr != pSpawnMonster )
				{
					++SpawnCount;
				// DUAL-PATH: register spawned mob in ECS registry
				if (pSpawnMonster) {
					EntityFactory::CreateMonster(
						g_registry,
						pSpawnMonster->GetMobTable(),
						pSpawnMonster->GetX(),
						pSpawnMonster->GetY(),
						ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(pSpawnMonster)),
						ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(pSpawnMonster)));
				}
				}
			}

			LOG_INFO("QUEST Spawn Monster: VNUM({}) COUNT({}) isAggressive(%b)", dwVnum, SpawnCount, isAggressive);
		}

		lua_pushnumber(L, SpawnCount);

		return 1;
	}
#endif

	ALUA(_notice_in_map)
	{
		// migrated from CHARACTER::notice_in_map
		// DUAL-PATH: legacy only during migration window
		const LPCHARACTER pChar = CQuestManager::instance().GetCurrentCharacterPtr();

		if (nullptr != pChar)
		{
#ifdef TEXTS_IMPROVEMENT
			if (!lua_isnumber(L, 1)) {
				return 0;
			}

			if (!lua_isstring(L, 2)) {
				return 0;
			}

			SendNoticeNew(CHAT_TYPE_NOTICE, 0, ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(pChar)), (uint32_t)lua_tonumber(L, 1), lua_tostring(L, 2));
#else
			SendNoticeMap( lua_tostring(L,1), ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(pChar)), lua_toboolean(L,2) );
#endif
		}

		return 0;
	}

	ALUA(_get_locale_base_path)
	{
		// migrated from CHARACTER::get_locale_base_path
		// DUAL-PATH: legacy only during migration window
		lua_pushstring( L, LocaleService_GetBasePath().c_str() );

		return 1;
	}

	struct FPurgeArea
	{
		int32_t x1, y1, x2, y2;
		LPCHARACTER ExceptChar;

		FPurgeArea(int32_t a, int32_t b, int32_t c, int32_t d, LPCHARACTER p)
			: x1(a), y1(b), x2(c), y2(d),
			ExceptChar(p)
		{}

		void operator () (LPENTITY ent)
		{
			if (true == ent->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER pChar = static_cast<LPCHARACTER>(ent);

				if (pChar == ExceptChar)
					return;
#ifdef __NEWPET_SYSTEM__
				if (!pChar->IsPet() && !pChar->IsNewPet() && (true == pChar->IsMonster() || true == ecs::PlayerRuntime::IsStone(AIHelpers::EcsOf(pChar))))
#else
				if (!pChar->IsPet() && (true == pChar->IsMonster() || true == ecs::PlayerRuntime::IsStone(AIHelpers::EcsOf(pChar))))
#endif
				{
					if (x1 <= ((pChar)->GetX()) && ((pChar)->GetX()) <= x2 && y1 <= ((pChar)->GetY()) && ((pChar)->GetY()) <= y2)
					{
						M2_DESTROY_CHARACTER(pChar);
					}
				}
			}
		}
	};

	ALUA(_purge_area)
	{
		// migrated from CHARACTER::purge_area
		// DUAL-PATH: legacy only - area purge affects many entities
		int32_t x1 = (int32_t)lua_tonumber(L, 1);
		int32_t y1 = (int32_t)lua_tonumber(L, 2);
		int32_t x2 = (int32_t)lua_tonumber(L, 3);
		int32_t y2 = (int32_t)lua_tonumber(L, 4);

		const int32_t mapIndex = SECTREE_MANAGER::instance().GetMapIndex( x1, y1 );

		if (0 == mapIndex)
		{
			sys_err("_purge_area: cannot get a map index with ({}, {})", x1, y1);
			return 0;
		}

		LPSECTREE_MAP pSectree = SECTREE_MANAGER::instance().GetMap(mapIndex);

		if (nullptr != pSectree)
		{
			FPurgeArea func(x1, y1, x2, y2, CQuestManager::instance().GetCurrentNPCCharacterPtr());

			pSectree->for_each(func);
		}

		return 0;
	}

	struct FWarpAllInAreaToArea
	{
		int32_t from_x1, from_y1, from_x2, from_y2;
		int32_t to_x1, to_y1, to_x2, to_y2;
		uint64_t warpCount;

		FWarpAllInAreaToArea(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g, int32_t h)
			: from_x1(a), from_y1(b), from_x2(c), from_y2(d),
			to_x1(e), to_y1(f), to_x2(g), to_y2(h),
			warpCount(0)
		{}

		void operator () (LPENTITY ent)
		{
			if (true == ent->IsType(ENTITY_CHARACTER))
			{
				LPCHARACTER pChar = static_cast<LPCHARACTER>(ent);

				if (true == (ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(pChar))))
				{
					if (from_x1 <= ((pChar)->GetX()) && ((pChar)->GetX()) <= from_x2 && from_y1 <= ((pChar)->GetY()) && ((pChar)->GetY()) <= from_y2)
					{
						++warpCount;

						pChar->WarpSet( number(to_x1, to_x2), number(to_y1, to_y2) );
					}
				}
			}
		}
	};

	ALUA(_warp_all_in_area_to_area)
	{
		// migrated from CHARACTER::warp_all_in_area_to_area
		// DUAL-PATH: legacy only during migration window
		int32_t from_x1 = (int32_t)lua_tonumber(L, 1);
		int32_t from_y1 = (int32_t)lua_tonumber(L, 2);
		int32_t from_x2 = (int32_t)lua_tonumber(L, 3);
		int32_t from_y2 = (int32_t)lua_tonumber(L, 4);

		int32_t to_x1 = (int32_t)lua_tonumber(L, 5);
		int32_t to_y1 = (int32_t)lua_tonumber(L, 6);
		int32_t to_x2 = (int32_t)lua_tonumber(L, 7);
		int32_t to_y2 = (int32_t)lua_tonumber(L, 8);

		const int32_t mapIndex = SECTREE_MANAGER::instance().GetMapIndex( from_x1, from_y1 );

		if (0 == mapIndex)
		{
			sys_err("_warp_all_in_area_to_area: cannot get a map index with ({}, {})", from_x1, from_y1);
			lua_pushnumber(L, 0);
			return 1;
		}

		LPSECTREE_MAP pSectree = SECTREE_MANAGER::instance().GetMap(mapIndex);

		if (nullptr != pSectree)
		{
			FWarpAllInAreaToArea func(from_x1, from_y1, from_x2, from_y2, to_x1, to_y1, to_x2, to_y2);

			pSectree->for_each(func);

			lua_pushnumber(L, func.warpCount);
			LOG_INFO("_warp_all_in_area_to_area: {} character warp", func.warpCount);
			return 1;
		}
		else
		{
			lua_pushnumber(L, 0);
			sys_err("_warp_all_in_area_to_area: no sectree");
			return 1;
		}
	}

	ALUA(_get_special_item_group)
	{
		// migrated from CHARACTER::get_special_item_group
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber (L, 1))
		{
			sys_err("invalid argument");
			return 0;
		}

		const CSpecialItemGroup* pItemGroup = ITEM_MANAGER::instance().GetSpecialItemGroup((uint32_t)lua_tonumber(L, 1));

		if (!pItemGroup)
		{
			sys_err("cannot find special item group {}", (uint32_t)lua_tonumber(L, 1));
			return 0;
		}

		int count = pItemGroup->GetGroupSize();

		for (int i = 0; i < count; i++)
		{
			lua_pushnumber(L, (int)pItemGroup->GetVnum(i));
			lua_pushnumber(L, (int)pItemGroup->GetCount(i));
		}

		return count*2;
	}

#ifdef ENABLE_NEWSTUFF
	ALUA(_get_table_postfix)
	{
		// migrated from CHARACTER::get_table_postfix
		// DUAL-PATH: legacy only during migration window
		lua_pushstring(L, get_table_postfix());
		return 1;
	}

#ifdef _MSC_VER
#define INFINITY (DBL_MAX+DBL_MAX)
#define NAN (INFINITY-INFINITY)
#endif
	ALUA(_mysql_direct_query)
	{
		// migrated from CHARACTER::mysql_direct_query
		// DUAL-PATH: legacy only during migration window
		if (!lua_isstring(L, 1))
			return 0;

		int i=0, m=1;
		MYSQL_ROW row;
		MYSQL_FIELD * field;
		MYSQL_RES * result;

		std::unique_ptr<SQLMsg> pMsg(DBManager::instance().DirectQuery("%s", lua_tostring(L, 1)));
		if (pMsg)
		{
			// ret1 (number of affected rows)
			lua_pushnumber(L, pMsg->Get()->uiAffectedRows);
			//-1 if error such as duplicate occurs (-2147483648 via lua)
			//   if wrong syntax error occurs (4294967295 via lua)
			// ret2 (table of affected rows)
			lua_newtable(L);
			if ((result = pMsg->Get()->pSQLResult) &&
					!(pMsg->Get()->uiAffectedRows == 0 || pMsg->Get()->uiAffectedRows == (uint32_t)-1))
			{

				while((row = mysql_fetch_row(result)))
				{
					lua_pushnumber(L, m);
					lua_newtable(L);
					while((field = mysql_fetch_field(result)))
					{
						lua_pushstring(L, field->name);
						if (!(field->flags & NOT_NULL_FLAG) && (row[i]== nullptr))
						{
							// lua_pushstring(L, "NULL");
							lua_pushnil(L);
						}
						else if (IS_NUM(field->type))
						{
							double val = NAN;
							lua_pushnumber(L, (sscanf(row[i],"%lf",&val)==1)?val:NAN);
						}
						else if (field->type == MYSQL_TYPE_BLOB)
						{
							lua_newtable(L);
							for (uint32_t iBlob=0; iBlob < field->max_length; iBlob++)
							{
								lua_pushnumber(L, row[i][iBlob]);
								lua_rawseti(L, -2, iBlob+1);
							}
						}
						else
							lua_pushstring(L, row[i]);

						lua_rawset(L, -3);
						i++;
					}
					mysql_field_seek(result, 0);
					i=0;

					lua_rawset(L, -3);
					m++;
				}
			}
		}
		else {lua_pushnumber(L, 0); lua_newtable(L);}

		return 2;
	}

	ALUA(_mysql_escape_string)
	{
		// migrated from CHARACTER::mysql_escape_string
		// DUAL-PATH: legacy only during migration window
		char szQuery[1024] = {0};

		if (!lua_isstring(L, 1))
			return 0;

		DBManager::instance().EscapeString(szQuery, sizeof(szQuery), lua_tostring(L, 1), strlen(lua_tostring(L, 1)));
		lua_pushstring(L, szQuery);
		return 1;
	}

	/*ALUA(_mysql_password)
	{
		// migrated from CHARACTER::mysql_password
		// DUAL-PATH: legacy only during migration window
		lua_pushstring(L, mysql_hash_password(lua_tostring(L, 1)).c_str());
		return 1;
	}*/
#endif
#ifdef __VERSION_162__
	ALUA(_add_restart_city_pos)
	{
		// migrated from CHARACTER::add_restart_city_pos
		// DUAL-PATH: legacy only during migration window
		int iMapIndex = (int)lua_tonumber(L, 1);
		int iEmpire = (int)lua_tonumber(L, 2);
		int iX = (int)lua_tonumber(L, 3);
		int iY = (int)lua_tonumber(L, 4);
		int iZ = (int)lua_tonumber(L, 5);
		SECTREE_MANAGER::instance().AddRestartCityPos(iMapIndex, iEmpire, iX, iY, iZ);
		return 0;
	}
#endif


	void RegisterGlobalFunctionTable(lua_State* L)
	{
		extern ALUA(quest_setstate);

		luaL_reg global_functions[] =
		{
		// migrated from CHARACTER::quest_setstate
		// DUAL-PATH: legacy only during migration window
			{	"sys_err",					_syserr					},
			{	"sys_log",					_syslog					},
			{	"char_log",					_char_log				},
			{	"item_log",					_item_log				},
			{	"set_state",				quest_setstate			},
			{	"set_skin",					_set_skin				},
			{	"setskin",					_set_skin				},
			{	"time_to_str",				_time_to_str			},
			{	"say",						_say					},
			{	"chat",						_chat					},
			{	"cmdchat",					_cmdchat				},
			{	"syschat",					_syschat				},
			{	"get_locale",				_get_locale				},
			{	"setleftimage",				_left_image				},
			{	"settopimage",				_top_image				},
			{	"server_timer",				_set_server_timer		},
			{	"clear_server_timer",		_clear_server_timer		},
			{	"server_loop_timer",		_set_server_loop_timer	},
			{	"get_server_timer_arg",		_get_server_timer_arg	},
			{	"set_timer",				_timer					},
			{	"timer",					_timer					},
			{	"set_named_timer",			_set_named_timer		},
			{	"loop_timer",				_set_named_loop_timer	},
			{	"set_named_loop_timer",		_set_named_loop_timer	},
			{	"cleartimer",				_clear_named_timer		},
			{	"getnpcid",					_getnpcid				},
			{	"is_test_server",			_is_test_server			},
			{	"raw_script",				_raw_script				},
			{	"number",					_number	   				},

			// LUA_ADD_BGM_INFO
			{	"set_bgm_volume_enable",	_set_bgm_volume_enable	},
			{	"add_bgm_info",				_add_bgm_info			},
			// END_OF_LUA_ADD_BGM_INFO

			// LUA_ADD_GOTO_INFO
			{	"add_goto_info",			_add_goto_info			},
			// END_OF_LUA_ADD_GOTO_INFO

			// REFINE_PICK
			{	"__refine_pick",			_refine_pick			},
			// END_OF_REFINE_PICK

			{	"add_ox_quiz",					_add_ox_quiz					},
			{	"__fish_real_refine_rod",		_fish_real_refine_rod			}, // XXX
			{	"__give_char_priv",				_give_char_privilege			},
			{	"__give_empire_priv",			_give_empire_privilege			},
			{	"__give_guild_priv",			_give_guild_privilege			},
			{	"__get_empire_priv_string",		_get_empire_privilege_string	},
			{	"__get_empire_priv",			_get_empire_privilege			},
			{	"__get_guild_priv_string",		_get_guild_privilege_string		},
			{	"__get_guildid_byname",			_get_guildid_byname				},
			{	"__get_guild_priv",				_get_guild_privilege			},
			{	"item_name",					_item_name						},
			{	"mob_name",						_mob_name						},
			{	"mob_vnum",						_mob_vnum						},
			{	"get_time",						_get_global_time				},
			{	"get_global_time",				_get_global_time				},
			{	"get_channel_id",				_get_channel_id					},
			{	"command",						_do_command						},
			{	"find_pc_cond",					_find_pc_cond					},
			{	"find_pc_by_name",				_find_pc						},
			{	"find_npc_by_vnum",				_find_npc_by_vnum				},
			{	"set_quest_state",				_set_quest_state				},
			{	"get_quest_state",				_get_quest_state				},
			{	"under_han",					_under_han						},
			{	"notice",						_notice							},
			{	"notice_all",					_notice_all						},
			{	"notice_in_map",				_notice_in_map					},
#ifdef ENABLE_FULL_NOTICE
			{	"big_notice",					_big_notice						},
			{	"big_notice_all",				_big_notice_all					},
			{	"big_notice_in_map",			_big_notice_in_map				},
#endif
			{	"warp_all_to_village",			_warp_all_to_village			},
			{	"warp_to_village",				_warp_to_village				},
			{	"say_in_map",					_say_in_map						},
			{	"kill_all_in_map",				_kill_all_in_map				},
			{	"regen_in_map",					_regen_in_map					},
			{	"enable_over9refine",			_enable_over9refine				},
			{	"block_chat",					_block_chat						},
			{	"spawn_mob",					_spawn_mob						},
			{	"get_locale_base_path",			_get_locale_base_path			},
			{	"purge_area",					_purge_area						},
			{	"warp_all_in_area_to_area",		_warp_all_in_area_to_area		},
			{	"get_special_item_group",		_get_special_item_group			},
#ifdef ENABLE_NEWSTUFF
			{	"spawn_mob0",					_spawn_mob0						},
			{	"spawn_mob_in_map",				_spawn_mob_in_map				},
			{	"get_table_postfix",			_get_table_postfix				},	// get table postfix [return lua string]
			{	"mysql_direct_query",			_mysql_direct_query				},	// get the number of the affected rows and a table containing 'em [return lua number, lua table]
			{	"mysql_escape_string",			_mysql_escape_string			},	// escape <str> [return lua string]
			//{	"mysql_password",				_mysql_password					},	// same as the sql function PASSWORD(<str>) [return lua string]
#endif
#ifdef __VERSION_162__
			{"add_restart_city_pos", _add_restart_city_pos},
#endif

			{nullptr, nullptr}
		};

		int i = 0;

		while (global_functions[i].name != nullptr)
		{
			lua_register(L, global_functions[i].name, global_functions[i].func);
			++i;
		}
	}
}











