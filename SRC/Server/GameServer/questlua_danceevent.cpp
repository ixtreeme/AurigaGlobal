
#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "questmanager.h"
#include "sectree_manager.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"

namespace quest
{
	struct FWarpToHome
	{
		void operator() ( entt::entity character )
		{
			if (!ecs::IsCharacter(character))
				return;

			if ( (ecs::PlayerRuntime::IsPC(character)) == true && ecs::PlayerRuntime::IsGM(character) != true )
			{
				if ( ((ecs::PlayerRuntime::GetX(character) >= 764503 && ecs::PlayerRuntime::GetX(character) <= 772362) && (ecs::PlayerRuntime::GetY(character) >= 22807 && ecs::PlayerRuntime::GetY(character) <= 26499)) == false )
				{
					ecs::MovementSystem::GoHome(character);
				}
			}
		};
	};

	ALUA(dance_event_go_home)
	{
		LPSECTREE_MAP pSecMap = SECTREE_MANAGER::instance().GetMap( 115 );

		if ( pSecMap != nullptr)
		{
			FWarpToHome f;
			pSecMap->for_each( f );
		}

		return 0;
	}

	void RegisterDanceEventFunctionTable()
	{
		luaL_reg dance_event_functions[] =
		{
			{ "gohome",		dance_event_go_home	},

			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("dance_event", dance_event_functions);
	}
}



