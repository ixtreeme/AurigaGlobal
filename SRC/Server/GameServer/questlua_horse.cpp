#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"

#include "questlua.h"
#include "questmanager.h"
#include "horsename_manager.h"
#include "char_interface.hpp"
#include "affect.h"
#include "config.h"
#include "utils.h"
#include "arena.h"
#include "ecs/quest_helpers.hpp"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

namespace quest
{
	//
	// "horse" Lua functions
	//
	ALUA(horse_is_riding)
	{
		lua_pushnumber(L, MountSystem::IsRiding(CQuestManager::instance().GetPCEntity(L)) ? 1 : 0);
		return 1;
	}

	ALUA(horse_is_summon)
	{
		lua_pushboolean(L, MountSystem::IsSummoned(CQuestManager::instance().GetPCEntity(L)));
		return 1;
	}

	ALUA(horse_ride)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
#ifdef ENABLE_PVP_ADVANCED
	if (ecs::PlayerRuntime::GetDuelOption(character, "BlockRide"))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 516, "");
#endif
		return 0;
	}
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (MountSystem::IsRidingCostume(character))
			return 0;
		if (ecs::PlayerRuntime::GetMapIndex(character) == 113 ||
			CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(character)))
			return 0;
#endif
		MountSystem::StartRiding(character);
		return 0;
	}

	ALUA(horse_unride)
	{
		MountSystem::StopRiding(CQuestManager::instance().GetPCEntity(L));
		return 0;
	}

	ALUA(horse_summon)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
#ifdef ENABLE_PVP_ADVANCED
	if (ecs::PlayerRuntime::GetDuelOption(character, "BlockRide"))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 516, "");
#endif
		return 0;
	}
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (MountSystem::IsRidingCostume(character))
			return 0;
		if (ecs::PlayerRuntime::GetMapIndex(character) == 113 ||
			CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(character)))
			return 0;
#endif

		const bool bFromFar = lua_isboolean(L, 1) ? lua_toboolean(L, 1) : false;
		const uint32_t horseVnum = lua_isnumber(L, 2) ? static_cast<uint32_t>(lua_tonumber(L, 2)) : 0;
		const char* name = lua_isstring(L, 3) ? lua_tostring(L, 3) : nullptr;
		MountSystem::SummonHorse(character, true, bFromFar, horseVnum, name);
		return 0;
	}

	ALUA(horse_unsummon)
	{
		MountSystem::SummonHorse(CQuestManager::instance().GetPCEntity(L), false);
		return 0;
	}

	ALUA(horse_is_mine)
	{
		lua_pushboolean(L, MountSystem::IsOwnedHorse(
			CQuestManager::instance().GetPCEntity(L), CQuestManager::instance().GetNPCEntity(L)));
		return 1;
	}

	ALUA(horse_set_level)
	{
		if (!lua_isnumber(L, 1))
			return 0;
		const int newLevel = MINMAX(0, static_cast<int>(lua_tonumber(L, 1)), HORSE_MAX_LEVEL);
		MountSystem::SetHorseLevel(CQuestManager::instance().GetPCEntity(L), newLevel);
		return 0;
	}

	ALUA(horse_get_level)
	{
		lua_pushnumber(L, MountSystem::GetHorseLevel(CQuestManager::instance().GetPCEntity(L)));
		return 1;
	}

	ALUA(horse_advance)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const int currentLevel = MountSystem::GetHorseLevel(character);
		if (currentLevel >= HORSE_MAX_LEVEL)
			return 0;
		MountSystem::SetHorseLevel(character, currentLevel + 1);
		return 0;
	}

	ALUA(horse_get_health)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		lua_pushnumber(L, MountSystem::GetHorseLevel(character) > 0
			? MountSystem::GetHorseHealth(character) : 0);
		return 1;
	}

	ALUA(horse_get_health_pct)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const int maximum = MountSystem::GetHorseMaxHealth(character);
		const int pct = maximum > 0
			? MINMAX(0, MountSystem::GetHorseHealth(character) * 100 / maximum, 100)
			: 0;
		LOG_INFO("horse.get_health_pct {}", pct);
		lua_pushnumber(L, MountSystem::GetHorseLevel(character) > 0 ? pct : 0);
		return 1;
	}

	ALUA(horse_get_stamina)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		lua_pushnumber(L, MountSystem::GetHorseLevel(character) > 0
			? MountSystem::GetHorseStamina(character) : 0);
		return 1;
	}

	ALUA(horse_get_stamina_pct)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const int maximum = MountSystem::GetHorseMaxStamina(character);
		const int pct = maximum > 0
			? MINMAX(0, MountSystem::GetHorseStamina(character) * 100 / maximum, 100)
			: 0;
		LOG_INFO("horse.get_stamina_pct {}", pct);
		lua_pushnumber(L, MountSystem::GetHorseLevel(character) > 0 ? pct : 0);
		return 1;
	}

	ALUA(horse_get_grade)
	{
		lua_pushnumber(L, MountSystem::GetHorseGrade(CQuestManager::instance().GetPCEntity(L)));
		return 1;
	}

	ALUA(horse_is_dead)
	{
		lua_pushboolean(L, MountSystem::GetHorseHealth(CQuestManager::instance().GetPCEntity(L)) <= 0);
		return 1;
	}

	ALUA(horse_revive)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		if (MountSystem::GetHorseLevel(character) > 0 && MountSystem::GetHorseHealth(character) <= 0)
			MountSystem::ReviveHorse(character);
		return 0;
	}

	ALUA(horse_feed)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		if (MountSystem::GetHorseLevel(character) > 0 && MountSystem::GetHorseHealth(character) > 0)
			MountSystem::FeedHorse(character);
		return 0;
	}

	ALUA(horse_set_name)
	{
		// migrated from CHARACTER::SetQuestFlag
		// DUAL-PATH: legacy only during migration window
		// ���ϰ�
		// 0 : ������ ���� ����
		// 1 : �߸��� �̸��̴�
		// 2 : �̸� �ٲٱ� ����

		if ( lua_isstring(L, -1) != true ) return 0;

		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		if (MountSystem::GetHorseLevel(character) > 0)
		{
			const char* pHorseName = lua_tostring(L, -1);

			if ( pHorseName == nullptr || check_name(pHorseName) == 0 )
			{
				lua_pushnumber(L, 1);
			}
			else
			{
				int nHorseNameDuration = test_server == true ? 60*5 : 60*60*24*30;

				ecs::QuestSystem::SetFlag(character, "horse_name.valid_till", get_global_time() + nHorseNameDuration);
				AffectSystem::AddAffect(character, AFFECT_HORSE_NAME, 0, 0, 0, PASSES_PER_SEC(nHorseNameDuration), 0, true);
				std::string name = pHorseName;
				name += " Horse";
				CHorseNameManager::instance().UpdateHorseName(
					ecs::PlayerRuntime::GetPlayerID(character), name.c_str(), true);

				if (MountSystem::IsSummoned(character)) {
					MountSystem::SummonHorse(character, false, true);
					MountSystem::SummonHorse(character, true, true);
				}

				lua_pushnumber(L, 2);
			}
		}
		else
		{
			lua_pushnumber(L, 0);
		}

		return 1;
	}

	ALUA(horse_get_name)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		if (character != entt::null && g_registry.valid(character))
		{
			const char* pHorseName = CHorseNameManager::instance().GetHorseName(
				ecs::PlayerRuntime::GetPlayerID(character));

			if ( pHorseName != nullptr)
			{
				lua_pushstring(L, pHorseName);
				return 1;
			}
		}

		lua_pushstring(L, "");

		return 1;
	}
	void RegisterHorseFunctionTable()
	{
		luaL_reg horse_functions[] =
		{
			{ "is_mine",		horse_is_mine			},
			{ "is_riding",		horse_is_riding			},
			{ "is_summon",		horse_is_summon			},
			{ "ride",			horse_ride				},
			{ "unride",			horse_unride			},
			{ "summon",			horse_summon			},
			{ "unsummon",		horse_unsummon			},
			{ "advance",		horse_advance			},
			{ "get_level",		horse_get_level			},
			{ "set_level",		horse_set_level			},
			{ "get_health",		horse_get_health		},
			{ "get_health_pct",	horse_get_health_pct	},
			{ "get_stamina",	horse_get_stamina		},
			{ "get_stamina_pct",horse_get_stamina_pct	},
			{ "get_grade",      horse_get_grade         },
			{ "is_dead",		horse_is_dead			},
			{ "revive",			horse_revive			},
			{ "feed",			horse_feed				},
			{ "set_name",		horse_set_name			},
			{ "get_name",		horse_get_name			},
			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("horse", horse_functions);
	}
}







