
#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include <Core/Logging.hpp>
#include "questmanager.h"
#include "arena.h"

namespace quest
{
	ALUA(arena_start_duel)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity opponent = ecs::PlayerRuntime::FindByPlayerName(lua_tostring(L, 1));
		int nSetPoint = (int)lua_tonumber(L, 2);

		if (!ecs::PlayerRuntime::IsPC(chEntity) || !ecs::PlayerRuntime::IsPC(opponent))
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		if (MountSystem::IsRiding(chEntity))
		{
			MountSystem::StopRiding(chEntity);
			MountSystem::SummonHorse(chEntity, false);
		}

		if (MountSystem::IsRiding(opponent))
		{
			MountSystem::StopRiding(opponent);
			MountSystem::SummonHorse(opponent, false);
		}

		if (CArenaManager::instance().IsMember(ecs::PlayerRuntime::GetMapIndex(chEntity), ecs::PlayerRuntime::GetPlayerID(chEntity)) != MEMBER_NO ||
				CArenaManager::instance().IsMember(ecs::PlayerRuntime::GetMapIndex(opponent), ecs::PlayerRuntime::GetPlayerID(opponent)) != MEMBER_NO)
		{
			lua_pushnumber(L, 2);
			return 1;
		}

		if (!CArenaManager::instance().StartDuel(chEntity, opponent, nSetPoint))
		{
			lua_pushnumber(L, 3);
			return 1;
		}

		lua_pushnumber(L, 1);

		return 1;
	}

	ALUA(arena_add_map)
	{
		// migrated from arena system
		// DUAL-PATH: legacy only during migration window
		int mapIdx		= (int)lua_tonumber(L, 1);
		int startposAX	= (int)lua_tonumber(L, 2);
		int startposAY	= (int)lua_tonumber(L, 3);
		int startposBX	= (int)lua_tonumber(L, 4);
		int startposBY	= (int)lua_tonumber(L, 5);

		if ( CArenaManager::instance().AddArena(mapIdx, startposAX, startposAY, startposBX, startposBY) == false )
		{
			LOG_INFO("Failed to load arena map info(map:{} AX:{} AY:{} BX:{} BY:{}", mapIdx, startposAX, startposAY, startposBX, startposBY);
		}
		else
		{
			LOG_INFO("Add Arena Map:{} startA({},{}) startB({},{})", mapIdx, startposAX, startposAY, startposBX, startposBY);
		}

		return 1;
	}

	ALUA(arena_get_duel_list)
	{
		// migrated from arena system
		// DUAL-PATH: legacy only during migration window
		CArenaManager::instance().GetDuelList(L);

		return 1;
	}

	ALUA(arena_add_observer)
	{
		int mapIdx = (int)lua_tonumber(L, 1);
		int ObPointX = (int)lua_tonumber(L, 2);
		int ObPointY = (int)lua_tonumber(L, 3);
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		CArenaManager::instance().AddObserver(chEntity, mapIdx, ObPointX, ObPointY);

		return 1;
	}

	ALUA(arena_is_in_arena)
	{
		uint32_t pid = (uint32_t)lua_tonumber(L, 1);

		const entt::entity character = ecs::PlayerRuntime::FindByPlayerID(pid);

		if (character == entt::null)
		{
			lua_pushnumber(L, 1);
		}
		else
		{
			if (ecs::PlayerRuntime::GetArena(character) == nullptr || ecs::PlayerRuntime::IsArenaObserverMode(character))
			{
				if (CArenaManager::instance().IsMember(ecs::PlayerRuntime::GetMapIndex(character), ecs::PlayerRuntime::GetPlayerID(character)) == MEMBER_DUELIST)
					lua_pushnumber(L, 1);
				else
					lua_pushnumber(L, 0);
			}
			else
			{
				lua_pushnumber(L, 0);
			}
		}
		return 1;
	}

	void RegisterArenaFunctionTable()
	{
		luaL_reg arena_functions[] =
		{
			{"start_duel",		arena_start_duel		},
			{"add_map",			arena_add_map			},
			{"get_duel_list",	arena_get_duel_list		},
			{"add_observer",	arena_add_observer		},
			{"is_in_arena",		arena_is_in_arena		},

			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("arena", arena_functions);
	}
}



