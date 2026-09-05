
#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/Registry.hpp"

#include <sstream>

#include "questmanager.h"
#include "questlua.h"
#include "config.h"
#include "desc.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "buffer_manager.h"
#include "db.h"
#include "locale_service.h"
#include "regen.h"
#include "affect.h"
#include "guild.h"
#include "guild_manager.h"
#include "sectree_manager.h"
#include "ecs/systems/CombatSystem.hpp"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

namespace quest
{
	using namespace std;

	string ScriptToString(const string& str)
	{
		lua_State* L = CQuestManager::instance().GetLuaState();
		int x = lua_gettop(L);

		int errcode = lua_dobuffer(L, ("return "+str).c_str(), str.size()+7, "ScriptToString");
		string retstr;
		if (!errcode)
		{
			if (lua_isstring(L,-1))
				retstr = lua_tostring(L, -1);
		}
		else
		{
			sys_err("LUA ScriptRunError (code:{} src:[{}])", errcode, str.c_str());
		}
		lua_settop(L,x);
		return retstr;
	}

	void FSetWarpLocation::operator() (LPCHARACTER ch) const
	{
		if ((ecs::PlayerRuntime::IsPC(((ch) ? (ch)->GetEntityHandle() : entt::null))))
		{
			ch->SetWarpLocation (map_index, x, y);
		}
	}

	void FSetQuestFlag::operator() (LPCHARACTER ch) const
	{
		const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
		if (!(ecs::PlayerRuntime::IsPC(chEntity)))
			return;

		if (PC * pPC = CQuestManager::instance().GetPCForce((ecs::PlayerRuntime::GetPlayerID(chEntity))))
			pPC->SetFlag(flagname, value);
	}

	bool FPartyCheckFlagLt::operator() (LPCHARACTER ch) const
	{
		const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
		if (!(ecs::PlayerRuntime::IsPC(chEntity)))
			return false;

		PC * pPC = CQuestManager::instance().GetPCForce((ecs::PlayerRuntime::GetPlayerID(chEntity)));
		bool returnBool = false;
		if (pPC)
		{
			int flagValue = pPC->GetFlag(flagname);
			if (value > flagValue)
				returnBool = true;
			else
				returnBool = false;
		}

		return returnBool;
	}

	FPartyChat::FPartyChat(int ChatType, const char* str) : iChatType(ChatType), str(str)
	{
	}

	void FPartyChat::operator() (LPCHARACTER ch) const
	{
		ecs::ChatSystem::Send(((ch) ? (ch)->GetEntityHandle() : entt::null), static_cast<uint8_t>(iChatType), "%s", str);
	}

	void FPartyClearReady::operator() (LPCHARACTER ch) const
	{
		AffectSystem::RemoveAffect(((ch) ? (ch)->GetEntityHandle() : entt::null), AFFECT_DUNGEON_READY);
	}

	void FSendPacket::operator() (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			const auto ch = dynamic_cast<LPCHARACTER>(ent);
			const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
			if (ecs::PlayerRuntime::GetDesc(chEntity))
			{
				ecs::PlayerRuntime::GetDesc(chEntity)->Packet(buf.read_peek(), buf.size());
			}
		}
	}

	void FSendPacketToEmpire::operator() (LPENTITY ent)
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			const auto ch = dynamic_cast<LPCHARACTER>(ent);
			const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
			if (ecs::PlayerRuntime::GetDesc(chEntity))
			{
				if (ecs::PlayerRuntime::GetEmpire(chEntity) == bEmpire)
					ecs::PlayerRuntime::GetDesc(chEntity)->Packet(buf.read_peek(), buf.size());
			}
		}
	}

	void FWarpEmpire::operator() (LPENTITY ent) const
	{
		if (ent->IsType(ENTITY_CHARACTER))
		{
			const auto ch = dynamic_cast<LPCHARACTER>(ent);
			const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
			if ((ecs::PlayerRuntime::IsPC(chEntity)) && ecs::PlayerRuntime::GetEmpire(chEntity) == m_bEmpire)
			{
				ecs::MovementSystem::WarpSet(chEntity, m_x, m_y, m_lMapIndexTo);
			}
		}
	}

	FBuildLuaGuildWarList::FBuildLuaGuildWarList(lua_State * lua_state) : L(lua_state), m_count(1)
	{
		lua_newtable(lua_state);
	}

	void FBuildLuaGuildWarList::operator() (uint32_t g1, uint32_t g2)
	{
		CGuild* g = CGuildManager::instance().FindGuild(g1);

		if (!g)
			return;

		if (g->GetGuildWarType(g2) == GUILD_WAR_TYPE_FIELD)
			return;

		if (g->GetGuildWarState(g2) != GUILD_WAR_ON_WAR)
			return;

		lua_newtable(L);
		lua_pushnumber(L, g1);
		lua_rawseti(L, -2, 1);
		lua_pushnumber(L, g2);
		lua_rawseti(L, -2, 2);
		lua_rawseti(L, -2, m_count++);
	}

	bool IsScriptTrue(const char* code, int size)
	{
		if (size==0)
			return true;

		lua_State* L = CQuestManager::instance().GetLuaState();
		int x = lua_gettop(L);
		int errcode = lua_dobuffer(L, code, size, "IsScriptTrue");
		int bStart = lua_toboolean(L, -1);
		if (errcode)
		{
			sys_err("LUA ScriptRunError (code:{} src:[{}])", errcode, std::string(code, size));
		}
		lua_settop(L,x);
		return bStart != 0;
	}

	void combine_lua_string(lua_State * L, ostringstream & s)
	{
		char buf[32];

		int n = lua_gettop(L);
		int i;

		for (i = 1; i <= n; ++i)
		{
			if (lua_isstring(L,i))
				// LOG_TRACE("{}", lua_tostring(L,i));
				s << lua_tostring(L, i);
			else if (lua_isnumber(L, i))
			{
				snprintf(buf, sizeof(buf), "%.14g\n", lua_tonumber(L,i));
				s << buf;
			}
		}
	}

	ALUA(highscore_show)
	{
		CQuestManager & q = CQuestManager::instance();
		const char * pszBoardName = lua_tostring(L, 1);
		uint32_t mypid = ecs::PlayerRuntime::GetPlayerID(q.GetCurrentCharacter());
		bool bOrder = (int) lua_tonumber(L, 2) != 0 ? true : false;

		DBManager::instance().ReturnQuery(QID_HIGHSCORE_SHOW, mypid, nullptr,
				"SELECT h.pid, p.name, h.value FROM highscore%s as h, player%s as p WHERE h.board = '%s' AND h.pid = p.id ORDER BY h.value %s LIMIT 10",
				get_table_postfix(), get_table_postfix(), pszBoardName, bOrder ? "DESC" : "");
		return 0;
	}

	ALUA(highscore_register)
	{
		CQuestManager & q = CQuestManager::instance();

		THighscoreRegisterQueryInfo * qi = M2_NEW THighscoreRegisterQueryInfo;

		strlcpy(qi->szBoard, lua_tostring(L, 1), sizeof(qi->szBoard));
		qi->dwPID = ecs::PlayerRuntime::GetPlayerID(q.GetCurrentCharacter());
		qi->iValue = (int) lua_tonumber(L, 2);
		qi->bOrder = (int) lua_tonumber(L, 3);

		DBManager::instance().ReturnQuery(QID_HIGHSCORE_REGISTER, qi->dwPID, qi,
				"SELECT value FROM highscore%s WHERE board='%s' AND pid=%u", get_table_postfix(), qi->szBoard, qi->dwPID);
		return 1;
	}

	//
	// "member" Lua functions
	//
	ALUA(member_chat)
	{
		ostringstream s;
		combine_lua_string(L, s);
		ecs::ChatSystem::Send(((CQuestManager::Instance().GetCurrentPartyMember()) ? (CQuestManager::Instance().GetCurrentPartyMember())->GetEntityHandle() : entt::null), CHAT_TYPE_TALKING, "%s", s.str().c_str());
		return 0;
	}

	ALUA(member_clear_ready)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentPartyMember();
		AffectSystem::RemoveAffect(((ch) ? (ch)->GetEntityHandle() : entt::null), AFFECT_DUNGEON_READY);
		return 0;
	}

	ALUA(member_set_ready)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentPartyMember();
		AffectSystem::AddAffect(((ch) ? (ch)->GetEntityHandle() : entt::null), AFFECT_DUNGEON_READY, POINT_NONE, 0, AFF_DUNGEON_READY, 65535, 0, true);
		return 0;
	}

	ALUA(mob_spawn)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		{
			sys_err("invalid argument");
			return 0;
		}

		uint32_t mob_vnum = (uint32_t)lua_tonumber(L, 1);
		int32_t local_x = (int32_t) lua_tonumber(L, 2)*100;
		int32_t local_y = (int32_t) lua_tonumber(L, 3)*100;
		float radius = (float) lua_tonumber(L, 4)*100;
		bool bAggressive = lua_toboolean(L, 5);
		uint32_t count = (lua_isnumber(L, 6))?(uint32_t) lua_tonumber(L, 6):1;

		if (count == 0)
			count = 1;
		else if (count > 10)
		{
			sys_err("count bigger than 10");
			count = 10;
		}

		const entt::entity ch = CQuestManager::instance().GetCurrentCharacter();

		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(ecs::PlayerRuntime::GetMapIndex(ch));
		if (pMap == nullptr) {
			return 0;
		}
		uint32_t dwQuestIdx = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		bool ret = false;
		LPCHARACTER mob = nullptr;

		while (count--)
		{
			for (int loop = 0; loop < 8; ++loop)
			{
				float angle = number(0, 999) * M_PI * 2 / 1000;
				float r = number(0, 999) * radius / 1000;

				int32_t x = local_x + pMap->m_setting.iBaseX + (int32_t)(r * cos(angle));
				int32_t y = local_y + pMap->m_setting.iBaseY + (int32_t)(r * sin(angle));

				mob = CHARACTER_MANAGER::instance().SpawnMob(mob_vnum, ecs::PlayerRuntime::GetMapIndex(ch), x, y, 0);

				if (mob)
					break;
			}

			if (mob)
			{
				if (bAggressive)
					CombatSystem::SetAggressive(mob->GetEntityHandle());

				mob->SetQuestBy(dwQuestIdx);

				if (!ret)
				{
					ret = true;
					lua_pushnumber(L, ecs::PlayerRuntime::GetPacketVID(((mob) ? (mob)->GetEntityHandle() : entt::null)));
				}
			}
		}

		if (!ret)
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(mob_spawn_group)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !lua_isnumber(L, 6))
		{
			sys_err("invalid argument");
			lua_pushnumber(L, 0);
			return 1;
		}

		uint32_t group_vnum = static_cast<uint32_t>(lua_tonumber(L, 1));
		int32_t local_x = static_cast<int32_t>(lua_tonumber(L, 2)) * 100;
		int32_t local_y = static_cast<int32_t>(lua_tonumber(L, 3)) * 100;
		float radius = static_cast<float>(lua_tonumber(L, 4)) * 100;
		bool bAggressive = lua_toboolean(L, 5);
		uint32_t count = static_cast<uint32_t>(lua_tonumber(L, 6));

		if (count == 0)
			count = 1;
		else if (count > 10)
		{
			sys_err("count bigger than 10");
			count = 10;
		}

		const entt::entity ch = CQuestManager::instance().GetCurrentCharacter();

		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(ecs::PlayerRuntime::GetMapIndex(ch));
		if (pMap == nullptr) {
			lua_pushnumber(L, 0);
			return 1;
		}
		const uint32_t dwQuestIdx = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		bool ret = false;
		LPCHARACTER mob = nullptr;

		while (count--)
		{
			for (int loop = 0; loop < 8; ++loop)
			{
				float angle = number(0, 999) * M_PI * 2 / 1000;
				float r = number(0, 999)*radius/1000;

				int32_t x = local_x + pMap->m_setting.iBaseX + static_cast<int32_t>(r * cos(angle));
				int32_t y = local_y + pMap->m_setting.iBaseY + static_cast<int32_t>(r * sin(angle));

				mob = CHARACTER_MANAGER::instance().SpawnGroup(group_vnum, ecs::PlayerRuntime::GetMapIndex(ch), x, y, x, y, nullptr, bAggressive);

				if (mob)
					break;
			}

			if (mob)
			{
				mob->SetQuestBy(dwQuestIdx);

				if (!ret)
				{
					ret = true;
					lua_pushnumber(L, ecs::PlayerRuntime::GetPacketVID(((mob) ? (mob)->GetEntityHandle() : entt::null)));
				}
			}
		}

		if (!ret)
			lua_pushnumber(L, 0);

		return 1;
	}

	//
	// global Lua functions
	//
	//
	// Registers Lua function table
	//
	void CQuestManager::AddLuaFunctionTable(const char * c_pszName, luaL_reg * preg, bool bCheckIfExists) const
	{
#ifdef ENABLE_NEWSTUFF
		bool bIsExists = false;
		if (bCheckIfExists)
		{
			int x = lua_gettop(L);
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				lua_settop(L, x);
				bIsExists = true;
			}
		}
		if (!bIsExists)
			lua_newtable(L);
#else
		lua_newtable(L);
#endif

		while ((preg->name))
		{
			lua_pushstring(L, preg->name);
			lua_pushcfunction(L, preg->func);
			lua_rawset(L, -3);
			preg++;
		}

		lua_setglobal(L, c_pszName);
	}

	void CQuestManager::AddLuaFunctionSubTable(const char * c_pszName, const char * c_pszSubName, luaL_reg * preg) const
	{
		// lua_State* L = CQuestManager::instance().GetLuaState();
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				sys_err("{} global index not found for {}", c_pszName, c_pszSubName);
				lua_settop(L, x);
				return;
			}
			lua_pushstring(L, c_pszSubName);
			{
				lua_newtable(L);
				while ((preg->name))
				{
					lua_pushstring(L, preg->name);
					lua_pushcfunction(L, preg->func);
					lua_rawset(L, -3);
					preg++;
				}
			}
			lua_rawset(L, -3);
			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}

#ifdef ENABLE_NEWSTUFF
	void CQuestManager::AppendLuaFunctionTable(const char * c_pszName, luaL_reg * preg, bool bForceCreation) const
	{
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				sys_err("{} global index not found (force={})", c_pszName, bForceCreation);
				lua_settop(L, x);
				if (bForceCreation)
					AddLuaFunctionTable(c_pszName, preg);
				return;
			}

			while ((preg->name))
			{
				lua_pushstring(L, preg->name);
				lua_pushcfunction(L, preg->func);
				lua_rawset(L, -3);
				preg++;
			}

			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}

	void CQuestManager::AddLuaConstantGlobal(const char * c_pszName, lua_Number lNumber, bool bOverwrite) const
	{
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (lua_isnumber(L, -1))
			{
				if (!bOverwrite)
				{
					sys_err("{} global index already defined", c_pszName);
					lua_settop(L, x);
					return;
				}
			}
			lua_pushnumber(L, lNumber);
			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}

	void CQuestManager::AddLuaConstantInTable(const char * c_pszName, const char * c_pszSubName, lua_Number lNumber, bool bForceCreation) const
	{
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				if (!bForceCreation)
				{
					sys_err("{} global index for {} already defined", c_pszName, c_pszSubName);
					lua_settop(L, x);
					return;
				}
				lua_newtable(L);
			}
			{
				lua_pushstring(L, c_pszSubName);
				lua_pushnumber(L, lNumber);
				lua_rawset(L, -3);
			}
			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}

	void CQuestManager::AddLuaConstantInTable(const char * c_pszName, const char * c_pszSubName, const char * szString, bool bForceCreation) const
	{
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				if (!bForceCreation)
				{
					sys_err("{} global index for {} already defined", c_pszName, c_pszSubName);
					lua_settop(L, x);
					return;
				}
				lua_newtable(L);
			}
			{
				lua_pushstring(L, c_pszSubName);
				lua_pushstring(L, szString);
				lua_rawset(L, -3);
			}
			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}

	void CQuestManager::AddLuaConstantSubTable(const char * c_pszName, const char * c_pszSubName, luaC_tab * preg) const
	{
		// lua_State* L = CQuestManager::instance().GetLuaState();
		int x = lua_gettop(L);
		{
			lua_getglobal(L, c_pszName);
			if (!lua_istable(L, -1))
			{
				sys_err("{} global index not found for {}", c_pszName, c_pszSubName);
				lua_settop(L, x);
				return;
			}
			lua_pushstring(L, c_pszSubName);
			{
				lua_newtable(L);
				while ((preg->name))
				{
					lua_pushstring(L, preg->name);
					switch (preg->val.type)
					{
						case ETL_CFUN:
							lua_pushcfunction(L, preg->val.cfVal);
							break;
						case ETL_LNUM:
							lua_pushnumber(L, preg->val.lnVal);
							break;
						case ETL_LSTR:
							lua_pushstring(L, preg->val.lsVal);
							break;
						case ETL_NIL:
							lua_pushnil(L);
							break;
					}
					lua_rawset(L, -3);
					preg++;
				}
			}
			lua_rawset(L, -3);
			lua_setglobal(L, c_pszName);
		}
		lua_settop(L, x);
	}
#endif

	void CQuestManager::BuildStateIndexToName(const char* questName) const
	{
		int x = lua_gettop(L);
		lua_getglobal(L, questName);

		if (lua_isnil(L,-1))
		{
			sys_err("QUEST wrong quest state file for quest {}", questName);
			lua_settop(L,x);
			return;
		}

		for (lua_pushnil(L); lua_next(L, -2);)
		{
			if (lua_isstring(L, -2) && lua_isnumber(L, -1))
			{
				lua_pushvalue(L, -2);
				lua_rawset(L, -4);
			}
			else
			{
				lua_pop(L, 1);
			}
		}

		lua_settop(L, x);
	}

	/**
	 * @version 05/06/08	Bang2ni - __get_guildid_byname ��ũ��Ʈ �Լ� ���
	 */
	bool CQuestManager::InitializeLua()
	{
//#if LUA_V == 503
		L = lua_open();

		luaopen_base(L);
		luaopen_table(L);
		luaopen_string(L);
		luaopen_math(L);
		//TEMP
		luaopen_io(L);
		luaopen_debug(L);
//#else
//	#error "lua version not found"
//#endif

		RegisterAffectFunctionTable();
		RegisterBuildingFunctionTable();
		RegisterDungeonFunctionTable();
		RegisterGameFunctionTable();
		RegisterGuildFunctionTable();
		RegisterHorseFunctionTable();
#ifdef __PET_SYSTEM__
		RegisterPetFunctionTable();
#endif
#ifdef __NEWPET_SYSTEM__
		RegisterNewPetFunctionTable();
#endif
		RegisterITEMFunctionTable();
		RegisterMarriageFunctionTable();
		RegisterNPCFunctionTable();
		RegisterPartyFunctionTable();
		RegisterPCFunctionTable();
		RegisterQuestFunctionTable();
		RegisterTargetFunctionTable();
		RegisterArenaFunctionTable();
		RegisterOXEventFunctionTable();
		RegisterBattleArenaFunctionTable();
		RegisterDanceEventFunctionTable();
		RegisterDragonSoulFunctionTable();

		{
			luaL_reg member_functions[] =
			{
				{ "chat",			member_chat		},
				{ "set_ready",			member_set_ready	},
				{ "clear_ready",		member_clear_ready	},
				{nullptr, nullptr}
			};

			AddLuaFunctionTable("member", member_functions);
		}

		{
			luaL_reg highscore_functions[] =
			{
				{ "register",			highscore_register	},
				{ "show",			highscore_show		},
				{nullptr, nullptr}
			};

			AddLuaFunctionTable("highscore", highscore_functions);
		}

		{
			luaL_reg mob_functions[] =
			{
				{ "spawn",			mob_spawn		},
				{ "spawn_group",		mob_spawn_group		},
				{nullptr, nullptr}
			};

			AddLuaFunctionTable("mob", mob_functions);
		}

		//
		// global namespace functions
		//
		RegisterGlobalFunctionTable(L);

		// LUA_INIT_ERROR_MESSAGE
		{
			char settingsFileName[256];
			snprintf(settingsFileName, sizeof(settingsFileName), "%s/settings.lua", LocaleService_GetBasePath().c_str());

			int settingsLoadingResult = luaL_loadfile(L, settingsFileName);
			LOG_INFO("LoadSettings({}), returns {}", settingsFileName, settingsLoadingResult);
			if (settingsLoadingResult != 0)
			{
				sys_err("LOAD_SETTINGS_FAILURE({})", settingsFileName);
				return false;
			}

			int settingsExecutionResult = lua_pcall(L, 0, 0, 0);
			LOG_INFO("ExecuteSettings({}), returns {}", settingsFileName, settingsExecutionResult);
			if (settingsExecutionResult != 0)
			{
				sys_err("EXECUTE_SETTINGS_FAILURE({})", settingsFileName);
				return false;
			}
		}

		{
			char questlibFileName[256];
			snprintf(questlibFileName, sizeof(questlibFileName), "%s/questlib.lua", LocaleService_GetQuestPath().c_str());

			int questlibLoadingResult = lua_dofile(L, questlibFileName);
			LOG_INFO("LoadQuestlib({}), returns {}", questlibFileName, questlibLoadingResult);
			if (questlibLoadingResult != 0)
			{
				sys_err("LOAD_QUESTLIB_FAILURE({})", questlibFileName);
				return false;
			}
		}

#define ENABLE_TRANSLATE_LUA
#ifdef ENABLE_TRANSLATE_LUA
		{
			char translateFileName[256];
			snprintf(translateFileName, sizeof(translateFileName), "%s/translate.lua", LocaleService_GetBasePath().c_str());

			int translateLoadingResult = lua_dofile(L, translateFileName);
			LOG_INFO("LoadTranslate({}), returns {}", translateFileName, translateLoadingResult);
			if (translateLoadingResult != 0)
			{
				sys_err("LOAD_TRANSLATE_ERROR({})", translateFileName);
				return false;
			}
		}
#ifdef ENABLE_MULTILANGUAGE_SYSTEM
			std::string eMultiLanguages[] =
			{
				"en", "de", "it", "tr", "ro", "pl", "pt", "hu", "es"
			};

			for (int i = 0; i < _countof(eMultiLanguages); i++)
			{
				char translateFileNameNew[256];
				snprintf(translateFileNameNew, sizeof(translateFileNameNew), "%s/translate/%s/translate.lua", LocaleService_GetBasePath().c_str(), eMultiLanguages[i].c_str());
				if (lua_dofile(L, translateFileNameNew) != 0)
				{
					sys_err("LOAD_TRANSLATE_ERROR({})", translateFileNameNew);
					return false;
				}
			}
#endif
#endif

		{
			char questLocaleFileName[256];
			snprintf(questLocaleFileName, sizeof(questLocaleFileName), "%s/locale.lua", g_stQuestDir.c_str());

			int questLocaleLoadingResult = lua_dofile(L, questLocaleFileName);
			LOG_INFO("LoadQuestLocale({}), returns {}", questLocaleFileName, questLocaleLoadingResult);
			if (questLocaleLoadingResult != 0)
			{
				sys_err("LoadQuestLocale({}) FAILURE", questLocaleFileName);
				return false;
			}
		}
		// END_OF_LUA_INIT_ERROR_MESSAGE

		for (auto it = g_setQuestObjectDir.begin(); it != g_setQuestObjectDir.end(); ++it)
		{
			const string& stQuestObjectDir = *it;
			char buf[PATH_MAX];
			snprintf(buf, sizeof(buf), "%s/state/", stQuestObjectDir.c_str());
			DIR * pdir = opendir(buf);
			int iQuestIdx = 0;

			if (pdir)
			{
				dirent * pde;

				while ((pde = readdir(pdir)))
				{
					if (pde->d_name[0] == '.')
						continue;

					snprintf(buf + 11, sizeof(buf) - 11, "%s", pde->d_name);

					RegisterQuest(pde->d_name, ++iQuestIdx);
					int ret = lua_dofile(L, (stQuestObjectDir + "/state/" + pde->d_name).c_str());
					LOG_TRACE("QUEST: loading {}, returns {}", (stQuestObjectDir + "/state/" + pde->d_name).c_str(), ret);

					BuildStateIndexToName(pde->d_name);
				}

				closedir(pdir);
			}
		}

//#if LUA_V == 503
		lua_setgcthreshold(L, 0);
//#endif
		lua_newtable(L);
		lua_setglobal(L, "__codecache");
		return true;
	}

	void CQuestManager::GotoSelectState(QuestState& qs)
	{
		lua_checkstack(qs.co, 1);

		//int n = lua_gettop(L);
		int n = luaL_getn(qs.co, -1);
		qs.args = n;
		//cout << "select here (1-" << qs.args << ")" << endl;
		//

		ostringstream os;
		os << "[QUESTION ";

		for (int i=1; i<=n; i++)
		{
			lua_rawgeti(qs.co,-1,i);
			if (lua_isstring(qs.co,-1))
			{
				// LOG_TRACE("{}\t{}", i, lua_tostring(qs.co,-1));
				if (i != 1)
					os << "|";
				os << i << ";" << lua_tostring(qs.co,-1);
			}
			else
			{
				sys_err("SELECT wrong data {}", lua_typename(qs.co, -1));
				sys_err("here");
			}
			lua_pop(qs.co,1);
		}
		os << "]";


		AddScript(os.str());
		qs.suspend_state = SUSPEND_STATE_SELECT;
		if ( test_server )
			LOG_INFO("{}", m_strScript.c_str());
		SendScript();
	}

	EVENTINFO(confirm_timeout_event_info)
	{
		uint32_t dwWaitPID;
		uint32_t dwReplyPID;

		confirm_timeout_event_info()
		: dwWaitPID( 0 )
		, dwReplyPID( 0 )
		{
		}
	};

	EVENTFUNC(confirm_timeout_event)
	{
		const auto info = dynamic_cast<confirm_timeout_event_info *>(event->info);

		if ( info == nullptr)
		{
			sys_err("confirm_timeout_event> <Factor> Null pointer");
			return 0;
		}

		LPCHARACTER chWait = CHARACTER_MANAGER::instance().FindByPID(info->dwWaitPID);
		LPCHARACTER chReply = nullptr; //CHARACTER_MANAGER::info().FindByPID(info->dwReplyPID);

		if (chReply)
		{
			// �ð� ������ �˾Ƽ� ����
		}

		if (chWait)
		{
			CQuestManager::instance().Confirm(info->dwWaitPID, CONFIRM_TIMEOUT);
		}

		return 0;
	}

	void CQuestManager::GotoConfirmState(QuestState & qs)
	{
		qs.suspend_state = SUSPEND_STATE_CONFIRM;
		uint32_t dwVID = static_cast<uint32_t>(lua_tonumber(qs.co, -3));
		const char* szMsg = lua_tostring(qs.co, -2);
		int iTimeout = static_cast<int>(lua_tonumber(qs.co, -1));

		LOG_INFO("GotoConfirmState vid {} msg '{}', timeout {}", dwVID, szMsg, iTimeout);

		const entt::entity ch = CHARACTER_MANAGER::instance().FindEntity(dwVID);
		const entt::entity chEntity = ch;

		if (ch != entt::null && (ecs::PlayerRuntime::IsPC(chEntity)))
		{
			NetworkSyncSystem::SendConfirmWithMsg(g_registry, chEntity, szMsg, iTimeout, ecs::PlayerRuntime::GetPlayerID(GetCurrentCharacter()));
		}

		GetCurrentPC()->SetConfirmWait((ch != entt::null && (ecs::PlayerRuntime::IsPC(chEntity)))?(ecs::PlayerRuntime::GetPlayerID(chEntity)):0);
		ostringstream os;
		os << "[CONFIRM_WAIT timeout;" << iTimeout << "]";
		AddScript(os.str());
		SendScript();

		confirm_timeout_event_info* info = AllocEventInfo<confirm_timeout_event_info>();

		info->dwWaitPID = ecs::PlayerRuntime::GetPlayerID(GetCurrentCharacter());
		info->dwReplyPID = (ch != entt::null && (ecs::PlayerRuntime::IsPC(chEntity))) ? (ecs::PlayerRuntime::GetPlayerID(chEntity)) : 0;

		event_create(confirm_timeout_event, info, PASSES_PER_SEC(iTimeout));
	}

	void CQuestManager::GotoSelectItemState(QuestState& qs)
	{
		qs.suspend_state = SUSPEND_STATE_SELECT_ITEM;
		AddScript("[SELECT_ITEM]");
		SendScript();
	}

	void CQuestManager::GotoInputState(QuestState & qs)
	{
		qs.suspend_state = SUSPEND_STATE_INPUT;
		AddScript("[INPUT]");
		SendScript();

		//event_create(input_timeout_event, dwEI, PASSES_PER_SEC(iTimeout));
	}

	void CQuestManager::GotoPauseState(QuestState & qs)
	{
		qs.suspend_state = SUSPEND_STATE_PAUSE;
		AddScript("[NEXT]");
		SendScript();
	}

	void CQuestManager::GotoEndState(QuestState & qs)
	{
		AddScript("[DONE]");
		SendScript();
	}

	QuestState CQuestManager::OpenState(const string& quest_name, int state_index) const
	{
		QuestState qs;
		qs.args=0;
		qs.st = state_index;
		qs.co = lua_newthread(L);
		qs.ico = lua_ref(L, 1/*qs.co*/);
		return qs;
	}


	bool CQuestManager::RunState(QuestState & qs)
	{
		ClearError();

		m_CurrentRunningState = &qs;

		int ret = lua_resume(qs.co, qs.args);
		if (ret == 0)
		{
			if (lua_gettop(qs.co) == 0)
			{
				// end of quest
				GotoEndState(qs);
				return false;
			}

			if (!strcmp(lua_tostring(qs.co, 1), "select"))
			{
				GotoSelectState(qs);
				return true;
			}

			if (!strcmp(lua_tostring(qs.co, 1), "wait"))
			{
				GotoPauseState(qs);
				return true;
			}

			if (!strcmp(lua_tostring(qs.co, 1), "input"))
			{
				GotoInputState(qs);
				return true;
			}

			if (!strcmp(lua_tostring(qs.co, 1), "confirm"))
			{
				GotoConfirmState(qs);
				return true;
			}

			if (!strcmp(lua_tostring(qs.co, 1), "select_item"))
			{
				GotoSelectItemState(qs);
				return true;
			}
		}
		else
		{
			sys_err("LUA_ERROR: {}", lua_tostring(qs.co, 1));
		}

		WriteRunningStateToSyserr();
		SetError();

		GotoEndState(qs);
		return false;
	}

	void CQuestManager::CloseState(QuestState& qs) const
	{
		if (qs.co)
		{
			//cerr << "ICO "<<qs.ico <<endl;
			lua_unref(L, qs.ico);
			qs.co = nullptr;
		}
	}
}


