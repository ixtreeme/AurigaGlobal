#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "utils.h"
#include "config.h"
#include "questmanager.h"
#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif
#include "char_interface.hpp"
#include "party.h"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "shop_manager.h"
#include "guild.h"
#include "ecs/quest_helpers.hpp"
#include "ecs/events.hpp"

namespace quest
{
	//
	// "npc" lua functions
	//
	ALUA(npc_open_shop)
	{
		// migrated from CHARACTER::StartShopping
		// DUAL-PATH: legacy only during migration window
		int iShopVnum = 0;
		if (lua_gettop(L) == 1)
		{
			if (lua_isnumber(L, 1))
				iShopVnum = (int) lua_tonumber(L, 1);
		}

		if (CQuestManager::instance().GetCurrentNPCCharacterPtr())
			CShopManager::instance().StartShopping(CQuestManager::instance().GetCurrentCharacterPtr(), CQuestManager::instance().GetCurrentNPCCharacterPtr(), iShopVnum);
		return 0;
	}

	ALUA(npc_is_pc)
	{
		lua_pushboolean(L, ecs::PlayerRuntime::IsPC(CQuestManager::instance().GetNPCEntity(L)) ? 1 : 0);
		return 1;
	}

	ALUA(npc_get_empire)
	{
		lua_pushnumber(L, ecs::PlayerRuntime::GetEmpire(CQuestManager::instance().GetNPCEntity(L)));
		return 1;
	}

	ALUA(npc_get_race)
	{
		// migrated from CHARACTER::GetRaceNum()
		entt::entity npcE = CQuestManager::instance().GetNPCEntity(L);
		auto* race = ECS_TryGet<ecs::RaceComponent>(npcE);
		if (!race) {
			lua_pushnumber(L, CQuestManager::instance().GetCurrentNPCRace());
			return 1;
		}
		lua_pushnumber(L, race->value);
		return 1;
	}

	ALUA(npc_get_guild)
	{
		CGuild* guild = ecs::SocialSystem::GetGuild(CQuestManager::instance().GetNPCEntity(L));
		lua_pushnumber(L, guild ? guild->GetID() : 0);
		return 1;
	}

	ALUA(npc_is_quest)
	{
		CQuestManager& q = CQuestManager::instance();
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		const std::string& questName = q.GetCurrentQuestName();
		lua_pushboolean(L,
			q.GetQuestIndexByName(questName) == ecs::PlayerRuntime::GetQuestBy(npcEntity));
		return 1;
	}

	ALUA(npc_kill)
	{
		CQuestManager& q = CQuestManager::instance();
		const entt::entity npcE = q.GetNPCEntity(L);
		const entt::entity pcE = q.GetPCEntity(L);
		ecs::PlayerRuntime::SetQuestNPCID(pcE, 0);
		if (g_registry.valid(npcE)) {
			g_registry.emplace_or_replace<ecs::DeadTag>(npcE);
			g_dispatcher.trigger(ecs::EvEntityDied{pcE, npcE});
		}
		CombatSystem::Dead(npcE);
		return 0;
	}

	ALUA(npc_purge)
	{
		CQuestManager& q = CQuestManager::instance();
		const entt::entity npcE = q.GetNPCEntity(L);
		const entt::entity pcE = q.GetPCEntity(L);
		ecs::PlayerRuntime::SetQuestNPCID(pcE, 0);
		if (g_registry.valid(npcE)) {
			g_registry.emplace_or_replace<ecs::DeadTag>(npcE);
			g_dispatcher.trigger(ecs::EvEntityDied{pcE, npcE});
		}
		ecs::PlayerRuntime::DestroyCharacter(npcE);
		return 0;
	}

	ALUA(npc_is_near)
	{
		// migrated from CHARACTER::GetX/GetY
		CQuestManager& q = CQuestManager::instance();
		entt::entity pcE = q.GetPCEntity(L);
		entt::entity npcE = q.GetNPCEntity(L);
		auto* pcPos = ECS_TryGet<ecs::Position>(pcE);
		auto* npcPos = ECS_TryGet<ecs::Position>(npcE);
		lua_Number dist = 10;
		if (lua_isnumber(L, 1))
			dist = lua_tonumber(L, 1);

		if (!pcPos || !npcPos)
		{
			lua_pushboolean(L, false);
			return 1;
		}

		lua_pushboolean(L, DISTANCE_APPROX(pcPos->x - npcPos->x, pcPos->y - npcPos->y) < dist*100);
		return 1;
	}

	ALUA(npc_is_near_vid)
	{
		// migrated from CHARACTER::GetX/GetY
		if (!lua_isnumber(L, 1))
		{
			sys_err("invalid vid");
			lua_pushboolean(L, 0);
			return 1;
		}

		CQuestManager& q = CQuestManager::instance();
		entt::entity targetE = CVIDRegistry::Instance().Find((uint32_t)lua_tonumber(L, 1));
		entt::entity npcE = q.GetNPCEntity(L);
		auto* targetPos = ECS_TryGet<ecs::Position>(targetE);
		auto* npcPos = ECS_TryGet<ecs::Position>(npcE);
		lua_Number dist = 10;
		if (lua_isnumber(L, 2))
			dist = lua_tonumber(L, 2);

		if (!targetPos || !npcPos)
		{
			lua_pushboolean(L, false);
			return 1;
		}

		lua_pushboolean(L, DISTANCE_APPROX(targetPos->x - npcPos->x, targetPos->y - npcPos->y) < dist*100);
		return 1;
	}

	ALUA(npc_unlock)
	{
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		if (npcEntity != entt::null && g_registry.valid(npcEntity))
		{
			if (ecs::PlayerRuntime::IsPC(npcEntity))
				return 0;

			if (ecs::PlayerRuntime::GetQuestNPCID(npcEntity) == ecs::PlayerRuntime::GetPlayerID(chEntity))
				ecs::PlayerRuntime::SetQuestNPCID(npcEntity, 0);
		}
		return 0;
	}

	ALUA(npc_lock)
	{
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		if (npcEntity == entt::null || !g_registry.valid(npcEntity) || ecs::PlayerRuntime::IsPC(npcEntity))
		{
			lua_pushboolean(L, true);
			return 1;
		}

		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const uint32_t lockOwner = ecs::PlayerRuntime::GetQuestNPCID(npcEntity);
		if (lockOwner == 0 || lockOwner == playerID)
		{
			ecs::PlayerRuntime::SetQuestNPCID(npcEntity, playerID);
			lua_pushboolean(L, true);
		}
		else
		{
			lua_pushboolean(L, false);
		}

		return 1;
	}

	ALUA(npc_get_leader_vid)
	{
		const entt::entity leader = ecs::SocialSystem::GetPartyLeader(
			CQuestManager::instance().GetNPCEntity(L));
		lua_pushnumber(L, ecs::PlayerRuntime::GetPacketVID(leader));
		return 1;
	}

	ALUA(npc_get_vid)
	{
		lua_pushnumber(L, ecs::PlayerRuntime::GetPacketVID(CQuestManager::instance().GetNPCEntity(L)));
		return 1;
	}

	ALUA(npc_get_vid_attack_mul)
	{
		// migrated from CHARACTER::GetAttMul
		// DUAL-PATH: legacy fallback during migration window
		if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			lua_pushnumber(L, 0);
			return 1;
		}

		int32_t vid = (int32_t)lua_tonumber(L, 1);

		const entt::entity character = CVIDRegistry::Instance().Find(vid);
		if (character == entt::null || !g_registry.valid(character)) {
			sys_err("The vid {} not exist.", vid);
			lua_pushnumber(L, 0);
			return 1;
		}

		lua_pushnumber(L, CombatSystem::GetAttackMultiplier(character));
		return 1;
	}

	ALUA(npc_set_vid_attack_mul)
	{
		// migrated from CHARACTER::SetAttMul
		// DUAL-PATH: legacy only during migration window
		if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t vid = (int32_t)lua_tonumber(L, 1);

		const entt::entity character = CVIDRegistry::Instance().Find(vid);
		if (character == entt::null || !g_registry.valid(character)) {
			sys_err("The vid {} not exist.", vid);
			return 0;
		}

		CombatSystem::SetAttackMultiplier(character, static_cast<float>(lua_tonumber(L, 2)));
		return 0;
	}

	ALUA(npc_get_vid_damage_mul)
	{
		// migrated from CHARACTER::GetDamMul
		// DUAL-PATH: legacy fallback during migration window
		if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			lua_pushnumber(L, 0);
			return 1;
		}

		int32_t vid = (int32_t)lua_tonumber(L, 1);

		const entt::entity character = CVIDRegistry::Instance().Find(vid);
		if (character == entt::null || !g_registry.valid(character)) {
			sys_err("The vid {} not exist.", vid);
			lua_pushnumber(L, 0);
			return 1;
		}

		lua_pushnumber(L, CombatSystem::GetDamageMultiplier(character));
		return 1;
	}

	ALUA(npc_set_vid_damage_mul)
	{
		// migrated from CHARACTER::SetDamMul
		// DUAL-PATH: legacy only during migration window
		if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			sys_err("not enough arguments.");
			return 0;
		}

		int32_t vid = (int32_t)lua_tonumber(L, 1);

		const entt::entity character = CVIDRegistry::Instance().Find(vid);
		if (character == entt::null || !g_registry.valid(character)) {
			sys_err("The vid {} not exist.", vid);
			return 0;
		}

		CombatSystem::SetDamageMultiplier(character, static_cast<float>(lua_tonumber(L, 2)));
		return 0;
	}

#ifdef ENABLE_NEWSTUFF
	ALUA(npc_get_level0)
	{
		lua_pushnumber(L, ecs::PointSystem::GetLevel(CQuestManager::instance().GetNPCEntity(L)));
		return 1;
	}

	ALUA(npc_get_name0)
	{
		lua_pushstring(L, ecs::PlayerRuntime::GetName(CQuestManager::instance().GetNPCEntity(L)).data());
		return 1;
	}

	ALUA(npc_get_pid0)
	{
		lua_pushnumber(L, ecs::PlayerRuntime::GetPlayerID(CQuestManager::instance().GetNPCEntity(L)));
		return 1;
	}

	ALUA(npc_get_vnum0)
	{
		lua_pushnumber(L, ecs::PlayerRuntime::GetRaceNum(CQuestManager::instance().GetNPCEntity(L)));
		return 1;
	}

	ALUA(npc_is_available0)
	{
		const entt::entity npc = CQuestManager::instance().GetNPCEntity(L);
		lua_pushboolean(L, npc != entt::null && g_registry.valid(npc));
		return 1;
	}

#endif
#ifdef ENABLE_MONKEY_DUNGI_BY_RAZOR93
	int npc_decrease_hp_by_percent_razor93(lua_State* L)
	{
		lua_Number vid = lua_tonumber(L, 1);
		int percent = (int)lua_tonumber(L, 2);

		const entt::entity npc = CVIDRegistry::Instance().Find(static_cast<uint32_t>(vid));
		if (npc != entt::null && g_registry.valid(npc) && percent > 0 && percent <= 100)
		{
			const int damage = ecs::PointSystem::GetMaxHP(npc) * percent / 100;
			ecs::PointSystem::Change(npc, POINT_HP, -damage);
			lua_pushboolean(L, 1);
		}
		else
		{
			lua_pushboolean(L, 0);
		}
		return 1;
	}
#endif
	void RegisterNPCFunctionTable()
	{
		luaL_reg npc_functions[] =
		{
#ifdef ENABLE_MONKEY_DUNGI_BY_RAZOR93
			{ "npc_decrease_hp_by_percent_razor93",			npc_decrease_hp_by_percent_razor93			},
#endif
			{ "getrace",			npc_get_race			},
			{ "get_race",			npc_get_race			},
			{ "open_shop",			npc_open_shop			},
			{ "get_empire",			npc_get_empire			},
			{ "is_pc",				npc_is_pc			},
			{ "is_quest",			npc_is_quest			},
			{ "kill",				npc_kill			},
			{ "purge",				npc_purge			},
			{ "is_near",			npc_is_near			},
			{ "is_near_vid",			npc_is_near_vid			},
			{ "lock",				npc_lock			},
			{ "unlock",				npc_unlock			},
			{ "get_guild",			npc_get_guild			},
			{ "get_leader_vid",		npc_get_leader_vid	},
			{ "get_vid",			npc_get_vid	},
			{"get_vid_attack_mul", npc_get_vid_attack_mul},
			{"set_vid_attack_mul", npc_set_vid_attack_mul},
			{"get_vid_damage_mul", npc_get_vid_damage_mul},
			{"set_vid_damage_mul", npc_set_vid_damage_mul},
#ifdef ENABLE_NEWSTUFF
			{ "get_level0",			npc_get_level0},	// [return lua number]
			{ "get_name0",			npc_get_name0},		// [return lua string]
			{ "get_pid0",			npc_get_pid0},		// [return lua number]
			{ "get_vnum0",			npc_get_vnum0},		// [return lua number]
			{ "is_available0",		npc_is_available0},	// [return lua boolean]
#endif
			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("npc", npc_functions);
	}
};



