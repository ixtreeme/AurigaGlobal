#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"

#include "questlua.h"
#include "questmanager.h"
#include "horsename_manager.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
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
		// migrated from CHARACTER::IsHorseRiding()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* ms = ECS_TryGet<ecs::MountState>(e);
		if (!ms)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			lua_pushnumber(L, (ch && ch->IsHorseRiding()) ? 1 : 0);
			return 1;
		}

		lua_pushnumber(L, ms->mountVnum != 0 ? 1 : 0);
		return 1;
	}

	ALUA(horse_is_summon)
	{
		// migrated from CHARACTER::GetHorse
		// DUAL-PATH: legacy fallback during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (nullptr != ch)
		{
			lua_pushboolean(L, (ch->GetHorse() != nullptr) ? true : false);
		}
		else
		{
			lua_pushboolean(L, false);
		}

		return 1;
	}

	ALUA(horse_ride)
	{
		// migrated from CHARACTER::StartRiding
		// DUAL-PATH: ECS update + legacy call during migration window
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* ms = ECS_TryGet<ecs::MountState>(e);
		auto* sf = ECS_TryGet<ecs::StatusFlags>(e);
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
#ifdef ENABLE_PVP_ADVANCED
	if ((ch->GetDuel("BlockRide")))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 516, "");
#endif
		return 0;
	}
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (ch->IsRidingMount())
			return 0;
		if (ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)) == 113 || CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch))) == true)
			return 0;
#endif

		if (ms && sf)
		{
			sf->isMountActive = true;
			ms->mountTime = get_dword_time();
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}

		if (ch)
			ch->StartRiding();
		return 0;
	}

	ALUA(horse_unride)
	{
		// migrated from CHARACTER::StopRiding
		// DUAL-PATH: ECS update + legacy call during migration window
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* sf = ECS_TryGet<ecs::StatusFlags>(e);
		if (sf)
		{
			sf->isMountActive = false;
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch)
			ch->StopRiding();
		return 0;
	}

	ALUA(horse_summon)
	{
		// migrated from CHARACTER::HorseSummon
		// DUAL-PATH: ECS update + legacy call during migration window
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* ms = ECS_TryGet<ecs::MountState>(e);
		auto* sf = ECS_TryGet<ecs::StatusFlags>(e);
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
#ifdef ENABLE_PVP_ADVANCED
	if ((ch->GetDuel("BlockRide")))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 516, "");
#endif
		return 0;
	}
#endif

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		if (ch->IsRidingMount())
			return 0;
		if (ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)) == 113 || CArenaManager::instance().IsArenaMap(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch))) == true)
			return 0;
#endif

		bool bFromFar = lua_isboolean(L, 1) ? lua_toboolean(L, 1) : false;
		uint32_t horseVnum = lua_isnumber(L, 2) ? lua_tonumber(L, 2) : 0;
		const char* name = lua_isstring(L, 3) ? lua_tostring(L, 3) : nullptr;
		if (ms && sf)
		{
			ms->mountVnum = horseVnum;
			ms->mountTime = get_dword_time();
			sf->isMountActive = (horseVnum != 0);
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}
		if (ch)
			ch->HorseSummon(true, bFromFar, horseVnum, name);
		return 0;
	}

	ALUA(horse_unsummon)
	{
		// migrated from CHARACTER::HorseSummon(false)
		// DUAL-PATH: ECS update + legacy call during migration window
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* ms = ECS_TryGet<ecs::MountState>(e);
		auto* sf = ECS_TryGet<ecs::StatusFlags>(e);
		if (ms && sf)
		{
			ms->mountVnum = 0;
			sf->isMountActive = false;
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch)
			ch->HorseSummon(false);
		return 0;
	}

	ALUA(horse_is_mine)
	{
		// migrated from CHARACTER::GetHorse() ownership check
		entt::entity pcE = CQuestManager::instance().GetPCEntity(L);
		entt::entity npcE = CQuestManager::instance().GetNPCEntity(L);
		auto* mount = ECS_TryGet<ecs::MountState>(pcE);
		auto* npcVid = ECS_TryGet<ecs::VIDComponent>(npcE);
		if (!mount || !npcVid)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			const entt::entity horseEntity = CQuestManager::instance().GetCurrentNPCEntity();
			auto* horse = ecs::LegacyCharOf(horseEntity);
			lua_pushboolean(L, horse && horse->GetRider() == ch);
			return 1;
		}

		lua_pushboolean(L, mount->mountVnum == npcVid->value ? 1 : 0);
		return 1;
	}

	ALUA(horse_set_level)
	{
		// migrated from CHARACTER::SetHorseLevel
		// DUAL-PATH: ECS update + legacy call during migration window
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* ms = ECS_TryGet<ecs::MountState>(e);
		if (!lua_isnumber(L, 1))
			return 0;

		int newlevel = MINMAX(0, (int)lua_tonumber(L, 1), HORSE_MAX_LEVEL);
		if (ms)
		{
			ms->sendHorseLevel = static_cast<uint8_t>(newlevel);
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch)
		{
			ch->SetHorseLevel(newlevel);
			ch->ComputePoints();
			ch->SkillLevelPacket();
		}
		return 0;
	}

	ALUA(horse_get_level)
	{
		// migrated from CHARACTER::GetHorseLevel()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* ms = ECS_TryGet<ecs::MountState>(e);
		if (!ms)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			lua_pushnumber(L, ch ? ch->GetHorseLevel() : 0);
			return 1;
		}

		lua_pushnumber(L, ms->sendHorseLevel);
		return 1;
	}

	ALUA(horse_advance)
	{
		// migrated from CHARACTER::SetHorseLevel
		// DUAL-PATH: ECS update + legacy call during migration window
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* ms = ECS_TryGet<ecs::MountState>(e);
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch && ch->GetHorseLevel() >= HORSE_MAX_LEVEL)
			return 0;

		if (ms && ms->sendHorseLevel < HORSE_MAX_LEVEL)
		{
			++ms->sendHorseLevel;
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}

		if (ch)
		{
			ch->SetHorseLevel(ch->GetHorseLevel() + 1);
			ch->ComputePoints();
			ch->SkillLevelPacket();
		}
		return 0;
	}

	ALUA(horse_get_health)
	{
		// migrated from CHARACTER::GetHorseHealth()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* ms = ECS_TryGet<ecs::MountState>(e);
		if (!ms)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			lua_pushnumber(L, (ch && ch->GetHorseLevel()) ? ch->GetHorseHealth() : 0);
			return 1;
		}

		lua_pushnumber(L, ms->sendHorseLevel ? ms->sendHorseHealthGrade : 0);
		return 1;
	}

	ALUA(horse_get_health_pct)
	{
		// migrated from CHARACTER::GetHorseHealth
		// DUAL-PATH: legacy fallback during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		int pct = MINMAX(0, ch->GetHorseHealth() * 100 / ch->GetHorseMaxHealth(), 100);
		LOG_INFO("horse.get_health_pct {}", pct);

		if (ch->GetHorseLevel())
			lua_pushnumber(L, pct);
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(horse_get_stamina)
	{
		// migrated from CHARACTER::GetHorseStamina()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* ms = ECS_TryGet<ecs::MountState>(e);
		if (!ms)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			lua_pushnumber(L, (ch && ch->GetHorseLevel()) ? ch->GetHorseStamina() : 0);
			return 1;
		}

		lua_pushnumber(L, ms->sendHorseLevel ? ms->sendHorseStaminaGrade : 0);
		return 1;
	}

	ALUA(horse_get_stamina_pct)
	{
		// migrated from CHARACTER::GetHorseStamina
		// DUAL-PATH: legacy fallback during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		int pct = MINMAX(0, ch->GetHorseStamina() * 100 / ch->GetHorseMaxStamina(), 100);
		LOG_INFO("horse.get_stamina_pct {}", pct);

		if (ch->GetHorseLevel())
			lua_pushnumber(L, pct);
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(horse_get_grade)
	{
		// migrated from CHARACTER::GetHorseGrade
		// DUAL-PATH: legacy fallback during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		lua_pushnumber(L, ch->GetHorseGrade());
		return 1;
	}

	ALUA(horse_is_dead)
	{
		// migrated from CHARACTER::GetHorseHealth
		// DUAL-PATH: legacy fallback during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		lua_pushboolean(L, ch->GetHorseHealth()<=0);
		return 1;
	}

	ALUA(horse_revive)
	{
		// migrated from CHARACTER::ReviveHorse
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch->GetHorseLevel() > 0 && ch->GetHorseHealth() <= 0)
		{
			ch->ReviveHorse();
		}
		return 0;
	}

	ALUA(horse_feed)
	{
		// migrated from CHARACTER::FeedHorse
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		//uint32_t dwHorseFood = ch->GetHorseLevel() + ITEM_HORSE_FOOD_1 - 1;
		if (ch->GetHorseLevel() > 0 && ch->GetHorseHealth() > 0)
		{
			ch->FeedHorse();
		}
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

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if ( ch->GetHorseLevel() > 0 )
		{
			const char* pHorseName = lua_tostring(L, -1);

			if ( pHorseName == nullptr || check_name(pHorseName) == 0 )
			{
				lua_pushnumber(L, 1);
			}
			else
			{
				int nHorseNameDuration = test_server == true ? 60*5 : 60*60*24*30;

				ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "horse_name.valid_till", get_global_time() + nHorseNameDuration);
				AffectSystem::AddAffect(AIHelpers::EcsOf(ch), AFFECT_HORSE_NAME, 0, 0, 0, PASSES_PER_SEC(nHorseNameDuration), 0, true);
				std::string name = pHorseName;
				name += " Horse";
				CHorseNameManager::instance().UpdateHorseName((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))), name.c_str(), true);

				if (ch->GetHorse() != nullptr) {
					ch->HorseSummon(false, true);
					ch->HorseSummon(true, true);
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
		// migrated from CHARACTER::GetPlayerID
		// DUAL-PATH: legacy fallback during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if ( ch != nullptr)
		{
			const char* pHorseName = CHorseNameManager::instance().GetHorseName((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))));

			if ( pHorseName != nullptr)
			{
				lua_pushstring(L, pHorseName);
				return 1;
			}
		}

		lua_pushstring(L, "");

		return 1;
	}
// #ifdef ENABLE_NEWSTUFF
	// ALUA(horse_set_stat0)
	// {
		// int m_health = MINMAX(0, lua_tonumber(L, 1), 50);
		// int m_stamina = MINMAX(0, lua_tonumber(L, 2), 200);

		// LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		// ch->UpdateHorseHealth(m_health - ch->GetHorseHealth());
		// ch->UpdateHorseStamina(m_stamina - ch->GetHorseStamina());

		// return 0;
	// }
// #endif
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
// #ifdef ENABLE_NEWSTUFF
			// horse.set_stat0(health, stamina) -- /do_horse_set_stat
			// { "set_stat0",		horse_set_stat0			},	// [return nothing]
// #endif
			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("horse", horse_functions);
	}
}







