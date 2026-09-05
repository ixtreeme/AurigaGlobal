#include "stdafx.h"
#include <Core/Logging.hpp>

#include "config.h"
#include "questmanager.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "ecs/systems/DragonSoulSystem.hpp"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

namespace quest
{
	ALUA(ds_open_refine_window)
	{
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		DragonSoulSystem::OpenRefineWindow(character, CQuestManager::instance().GetCurrentNPCEntity());
		return 0;
	}

	void RegisterDragonSoulFunctionTable()
	{
		luaL_reg ds_functions[] =
		{
			{ "open_refine_window"	, ds_open_refine_window },
			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("ds", ds_functions);
	}
};


