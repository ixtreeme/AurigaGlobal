#include "stdafx.h"

#include "config.h"
#include "questmanager.h"
#include "char.h"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, __VA_ARGS__)
#endif

namespace quest
{
	ALUA(ds_open_refine_window)
	{
		// migrated from CHARACTER DragonSoul refine window
		// TODO Phase 8: DragonSoulComponent
		// DUAL-PATH: legacy only during migration window
		const LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		if (nullptr == ch)
		{
			sys_err ("NULL POINT ERROR");
			return 0;
		}

		ch->DragonSoul_RefineWindow_Open(CQuestManager::instance().GetCurrentNPCCharacterPtr());
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

