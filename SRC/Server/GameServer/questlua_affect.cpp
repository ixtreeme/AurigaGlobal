#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "config.h"
#include "questmanager.h"
#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif
#include "sectree_manager.h"
#include "char_interface.hpp"
#include "affect.h"
#include "db.h"
#include "ecs/quest_helpers.hpp"

namespace quest
{
	//
	// "affect" Lua functions
	//
	ALUA(affect_add)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			return 0;
		}
		const uint8_t applyOn = static_cast<uint8_t>(lua_tonumber(L, 1));
		if (applyOn >= MAX_APPLY_NUM || applyOn < 1)
		{
			sys_err("apply is out of range : {}", applyOn);
			return 0;
		}
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		if (AffectSystem::FindAffect(character, AFFECT_QUEST_START_IDX, applyOn))
			return 0;
		const int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		const int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));
		AffectSystem::AddAffect(character, AFFECT_QUEST_START_IDX,
			aApplyInfo[applyOn].bPointType, value, 0, duration, 0, false);
		return 0;
	}

	ALUA(affect_remove)
	{
		CQuestManager& q = CQuestManager::instance();
		uint32_t type = q.GetCurrentPC()->GetCurrentQuestIndex() + AFFECT_QUEST_START_IDX;
		if (lua_isnumber(L, 1))
		{
			type = static_cast<uint32_t>(lua_tonumber(L, 1));
			if (type == 0)
				type = q.GetCurrentPC()->GetCurrentQuestIndex() + AFFECT_QUEST_START_IDX;
		}
		AffectSystem::RemoveAffect(q.GetPCEntity(L), type);
		return 0;
	}

	ALUA(affect_remove_bad)
	{
		AffectSystem::RemoveBadAffects(CQuestManager::instance().GetPCEntity(L));
		return 0;
	}

	ALUA(affect_remove_good)
	{
		AffectSystem::RemoveGoodAffects(CQuestManager::instance().GetPCEntity(L));
		return 0;
	}

	ALUA(affect_add_hair)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			return 0;
		}
		const uint8_t applyOn = static_cast<uint8_t>(lua_tonumber(L, 1));
		if (applyOn >= MAX_APPLY_NUM || applyOn < 1)
		{
			sys_err("apply is out of range : {}", applyOn);
			return 0;
		}
		const int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		const int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));
		AffectSystem::AddAffect(CQuestManager::instance().GetPCEntity(L), AFFECT_HAIR,
			aApplyInfo[applyOn].bPointType, value, 0, duration, 0, false);
		return 0;
	}

	ALUA(affect_remove_hair)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		CAffect* affect = AffectSystem::FindAffect(character, AFFECT_HAIR);
		if (affect)
		{
			lua_pushnumber(L, affect->lDuration);
			AffectSystem::RemoveAffect(character, affect);
		}
		else
			lua_pushnumber(L, 0);
		return 1;
	}

	// 현재 캐릭터가 AFFECT_TYPE affect를 갖고있으면 bApplyOn 값을 반환하고 없으면 nil을 반환하는 함수.
	// usage :	applyOn = affect.get_apply(AFFECT_TYPE)
	ALUA(affect_get_apply_on)
	{
		if (!lua_isnumber(L, 1))
		{
			sys_err("invalid argument");
			return 0;
		}
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const uint32_t type = static_cast<uint32_t>(lua_tonumber(L, 1));
		if (const CAffect* affect = AffectSystem::FindAffect(character, type))
			lua_pushnumber(L, affect->bApplyOn);
		else
			lua_pushnil(L);
		return 1;
	}

	ALUA(affect_add_collect)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			return 0;
		}
		const uint8_t applyOn = static_cast<uint8_t>(lua_tonumber(L, 1));
		if (applyOn >= MAX_APPLY_NUM || applyOn < 1)
		{
			sys_err("apply is out of range : {}", applyOn);
			return 0;
		}
		const int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		const int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));
		AffectSystem::AddAffect(CQuestManager::instance().GetPCEntity(L), AFFECT_COLLECT,
			aApplyInfo[applyOn].bPointType, value, 0, duration, 0, false);
		return 0;
	}
#ifdef __NEWPET_SYSTEM__
	ALUA(affect_pet_bonus)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			return 0;
		}
		const uint8_t applyOn = static_cast<uint8_t>(lua_tonumber(L, 1));
		if (applyOn >= MAX_APPLY_NUM || applyOn < 1)
		{
			sys_err("apply is out of range : {}", applyOn);
			return 0;
		}
		const int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		const int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));
		AffectSystem::AddAffect(CQuestManager::instance().GetPCEntity(L), AFFECT_PET,
			aApplyInfo[applyOn].bPointType, value, 0, duration, 0, false);
		return 0;
	}
#endif
	ALUA(affect_add_collect_point)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			return 0;
		}
		const uint8_t pointType = static_cast<uint8_t>(lua_tonumber(L, 1));
		if (pointType >= POINT_MAX_NUM || pointType < 1)
		{
			sys_err("point is out of range : {}", pointType);
			return 0;
		}
		const int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		const int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));
		AffectSystem::AddAffect(CQuestManager::instance().GetPCEntity(L), AFFECT_COLLECT,
			pointType, value, 0, duration, 0, false);
		return 0;
	}

	ALUA(affect_remove_collect)
	{
		uint8_t apply = static_cast<uint8_t>(lua_tonumber(L, 1));
		if (apply >= MAX_APPLY_NUM)
			return 0;
		apply = aApplyInfo[apply].bPointType;
		const int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		if (CAffect* affect = AffectSystem::FindAffect(character, AFFECT_COLLECT, apply, value))
			AffectSystem::RemoveAffect(character, affect);
		return 0;
	}

	ALUA(affect_remove_all_collect)
	{
		AffectSystem::RemoveAffect(CQuestManager::instance().GetPCEntity(L), AFFECT_COLLECT);
		return 0;
	}
	
#ifdef ENABLE_VOTE4BUFF
	ALUA(affect_add_affect)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) ||
			!lua_isnumber(L, 4) || character == entt::null || !g_registry.valid(character))
			return 0;
		const uint32_t affectType = static_cast<uint32_t>(lua_tonumber(L, 1));
		const uint8_t pointType = static_cast<uint8_t>(lua_tonumber(L, 2));
		const int32_t pointValue = static_cast<int32_t>(lua_tonumber(L, 3));
		const int32_t duration = static_cast<int32_t>(lua_tonumber(L, 4));
		if (pointType >= POINT_MAX_NUM)
			return 0;
		AffectSystem::RemoveAffect(character, affectType);
		AffectSystem::AddAffect(character, affectType, pointType, pointValue,
			AFF_NONE, duration, 0, false, false);
		return 0;
	}
#endif

	void RegisterAffectFunctionTable()
	{
		luaL_reg affect_functions[] =
		{
			{ "add",		affect_add		},
			{ "remove",		affect_remove		},
			{ "remove_bad",	affect_remove_bad	},
			{ "remove_good",	affect_remove_good	},
			{ "add_hair",		affect_add_hair		},
			{ "remove_hair",	affect_remove_hair		},
			{ "add_collect",		affect_add_collect		},
#ifdef __NEWPET_SYSTEM__
			{ "pet_bonus",		affect_pet_bonus		},
#endif
			{ "add_collect_point",		affect_add_collect_point		},
			{ "remove_collect",		affect_remove_collect	},
			{ "remove_all_collect",	affect_remove_all_collect	},
			{ "get_apply_on",	affect_get_apply_on },
			#ifdef ENABLE_VOTE4BUFF
			{ "add_affect",	affect_add_affect },
#endif

			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("affect", affect_functions);
	}
};


