
#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "questmanager.h"
#include "sectree_manager.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"

namespace quest
{
	struct FWarpToHome
	{
		void operator() ( LPENTITY ent )
		{
			if ( ent->IsType(ENTITY_CHARACTER) )
			{
				LPCHARACTER ch = (LPCHARACTER) ent;

				if ( (ecs::PlayerRuntime::IsPC(((ch) ? (ch)->GetEntityHandle() : entt::null))) == true && ch->IsGM() != true )
				{
					if ( ((ecs::PlayerRuntime::GetX(((ch) ? (ch)->GetEntityHandle() : entt::null)) >= 764503 && ecs::PlayerRuntime::GetX(((ch) ? (ch)->GetEntityHandle() : entt::null)) <= 772362) && (ecs::PlayerRuntime::GetY(((ch) ? (ch)->GetEntityHandle() : entt::null)) >= 22807 && ecs::PlayerRuntime::GetY(((ch) ? (ch)->GetEntityHandle() : entt::null)) <= 26499)) == false )
					{
						ch->GoHome();
					}
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



