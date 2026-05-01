
#include "stdafx.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "questmanager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "arena.h"

namespace quest
{
	ALUA(arena_start_duel)
	{
		// migrated from arena system
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		LPCHARACTER ch2 = CHARACTER_MANAGER::instance().FindPC(lua_tostring(L,1));
		int nSetPoint = (int)lua_tonumber(L, 2);

		if ( ch == nullptr || ch2 == nullptr)
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		if ( ch->IsHorseRiding() == true )
		{
			ch->StopRiding();
			ch->HorseSummon(false);
		}

		if ( ch2->IsHorseRiding() == true )
		{
			ch2->StopRiding();
			ch2->HorseSummon(false);
		}

		if ( CArenaManager::instance().IsMember(((ch)->GetMapIndex()), (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))) != MEMBER_NO ||
				CArenaManager::instance().IsMember(ch2->GetMapIndex(), ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch2))) != MEMBER_NO	)
		{
			lua_pushnumber(L, 2);
			return 1;
		}

		if ( CArenaManager::instance().StartDuel(ch, ch2, nSetPoint) == false )
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
		// migrated from arena observer system
		// DUAL-PATH: legacy only during migration window
		int mapIdx = (int)lua_tonumber(L, 1);
		int ObPointX = (int)lua_tonumber(L, 2);
		int ObPointY = (int)lua_tonumber(L, 3);
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CArenaManager::instance().AddObserver(ch, mapIdx, ObPointX, ObPointY);

		return 1;
	}

	ALUA(arena_is_in_arena)
	{
		// migrated from arena system
		// DUAL-PATH: legacy only during migration window
		uint32_t pid = (uint32_t)lua_tonumber(L, 1);

		LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);

		if ( ch == nullptr)
		{
			lua_pushnumber(L, 1);
		}
		else
		{
			if ( ch->GetArena() == nullptr || ch->GetArenaObserverMode() == true )
			{
				if ( CArenaManager::instance().IsMember(((ch)->GetMapIndex()), (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)))) == MEMBER_DUELIST )
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



