#include "stdafx.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "utils.h"
#include "config.h"
#include "questmanager.h"
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
		// migrated from CHARACTER::IsPC()
		entt::entity npcE = CQuestManager::instance().GetNPCEntity(L);
		if (npcE == entt::null || !g_registry.valid(npcE)) {
			const entt::entity npcEntity = CQuestManager::instance().GetCurrentNPCEntity();
			auto* npc = ecs::LegacyCharOf(npcEntity);
			lua_pushboolean(L, (npc && ((npc)->IsPC())) ? 1 : 0);
			return 1;
		}
		lua_pushboolean(L, g_registry.all_of<ecs::TagPC>(npcE) ? 1 : 0);
		return 1;
	}

	ALUA(npc_get_empire)
	{
		// migrated from CHARACTER::GetEmpire()
		entt::entity npcE = CQuestManager::instance().GetNPCEntity(L);
		auto* emp = ECS_TryGet<ecs::EmpireComponent>(npcE);
		if (!emp) {
			const entt::entity npcEntity = CQuestManager::instance().GetCurrentNPCEntity();
			auto* npc = ecs::LegacyCharOf(npcEntity);
			lua_pushnumber(L, npc ? npc->GetEmpire() : 0);
			return 1;
		}
		lua_pushnumber(L, emp->value);
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
		// migrated from CHARACTER::GetGuild()
		entt::entity npcE = CQuestManager::instance().GetNPCEntity(L);
		auto* gm = ECS_TryGet<ecs::GuildMembership>(npcE);
		if (!gm) {
			CQuestManager& q = CQuestManager::instance();
			const entt::entity npcEntity = q.GetCurrentNPCEntity();
			auto* npc = ecs::LegacyCharOf(npcEntity);
			CGuild* pGuild = npc ? npc->GetGuild() : nullptr;
			lua_pushnumber(L, pGuild ? pGuild->GetID() : 0);
			return 1;
		}
		lua_pushnumber(L, (gm->guild ? gm->guild->GetID() : 0));
		return 1;
	}

	ALUA(npc_is_quest)
	{
		// migrated from CHARACTER::GetQuestBy
		// DUAL-PATH: legacy only during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
		if (npc)
		{
			const std::string & r_st = q.GetCurrentQuestName();

			if (q.GetQuestIndexByName(r_st) == npc->GetQuestBy())
			{
				lua_pushboolean(L, 1);
				return 1;
			}
		}

		lua_pushboolean(L, 0);
		return 1;
	}

	ALUA(npc_kill)
	{
		// migrated from CHARACTER::Dead
		// DUAL-PATH: ECS update + legacy call during migration window
		CQuestManager& q = CQuestManager::instance();
		entt::entity npcE = q.GetNPCEntity(L);
		entt::entity pcE = q.GetPCEntity(L);
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
		if (ch)
			ch->SetQuestNPCID(0);
		if (g_registry.valid(npcE)) {
			g_registry.emplace_or_replace<ecs::DeadTag>(npcE);
			g_dispatcher.trigger(ecs::EvEntityDied{pcE, npcE});
		}
		if (npc)
			npc->Dead();
		return 0;
	}

	ALUA(npc_purge)
	{
		// migrated from CHARACTER::Dead
		// DUAL-PATH: ECS update + legacy call during migration window
		CQuestManager& q = CQuestManager::instance();
		entt::entity npcE = q.GetNPCEntity(L);
		entt::entity pcE = q.GetPCEntity(L);
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
		if (ch)
			ch->SetQuestNPCID(0);
		if (g_registry.valid(npcE)) {
			g_registry.emplace_or_replace<ecs::DeadTag>(npcE);
			g_dispatcher.trigger(ecs::EvEntityDied{pcE, npcE});
		}
		if (npc)
			M2_DESTROY_CHARACTER(npc);
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
			const entt::entity chEntity = q.GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			const entt::entity npcEntity = q.GetCurrentNPCEntity();
			auto* npc = ecs::LegacyCharOf(npcEntity);
			if (ch == nullptr || npc == nullptr)
				lua_pushboolean(L, false);
			else
				lua_pushboolean(L, DISTANCE_APPROX(((ch)->GetX()) - ((npc)->GetX()), ((ch)->GetY()) - ((npc)->GetY())) < dist*100);
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
			LPCHARACTER ch = CHARACTER_MANAGER::instance().Find((uint32_t)lua_tonumber(L, 1));
			const entt::entity npcEntity = q.GetCurrentNPCEntity();
			auto* npc = ecs::LegacyCharOf(npcEntity);
			if (ch == nullptr || npc == nullptr)
				lua_pushboolean(L, false);
			else
				lua_pushboolean(L, DISTANCE_APPROX(((ch)->GetX()) - ((npc)->GetX()), ((ch)->GetY()) - ((npc)->GetY())) < dist*100);
			return 1;
		}

		lua_pushboolean(L, DISTANCE_APPROX(targetPos->x - npcPos->x, targetPos->y - npcPos->y) < dist*100);
		return 1;
	}

	ALUA(npc_unlock)
	{
		// migrated from CHARACTER::SetQuestNPCID
		// DUAL-PATH: legacy only during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
		if ( npc != nullptr)
		{
			if (((npc)->IsPC()))
				return 0;

			if (npc->GetQuestNPCID() == ((ch)->GetPlayerID()))
			{
				npc->SetQuestNPCID(0);
			}
		}
		return 0;
	}

	ALUA(npc_lock)
	{
		// migrated from CHARACTER::SetQuestNPCID
		// DUAL-PATH: legacy only during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
		if (!npc || ((npc)->IsPC()))
		{
			lua_pushboolean(L, true);
			return 1;
		}

		if (npc->GetQuestNPCID() == 0 || npc->GetQuestNPCID() == ((ch)->GetPlayerID()))
		{
			npc->SetQuestNPCID(((ch)->GetPlayerID()));
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
		// migrated from CHARACTER::GetParty
		// DUAL-PATH: legacy fallback during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
#ifdef ENABLE_BUG_FIXES
		LPPARTY party = npc ? npc->GetParty() : nullptr;
		LPCHARACTER leader = party ? party->GetLeader() : nullptr;
		lua_pushnumber(L, leader ? leader->GetPacketVID() : 0);
#else
		PPARTY party = npc->GetParty();
		LPCHARACTER leader = party->GetLeader();

		if (leader)
			lua_pushnumber(L, leader->GetPacketVID());
		else
			lua_pushnumber(L, 0);
#endif
		return 1;
	}

	ALUA(npc_get_vid)
	{
		// migrated from CHARACTER::GetVID()
		CQuestManager& q = CQuestManager::instance();
		entt::entity npcE = q.GetNPCEntity(L);
		auto* vid = ECS_TryGet<ecs::VIDComponent>(npcE);
		if (!vid)
		{
			const entt::entity npcEntity = q.GetCurrentNPCEntity();
			auto* npc = ecs::LegacyCharOf(npcEntity);
			lua_pushnumber(L, npc ? ((npc)->GetLegacyVID()) : 0);
			return 1;
		}
		lua_pushnumber(L, vid->value);
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

		LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid);
		if (!ch) {
			sys_err("The vid %d not exist.", vid);
			lua_pushnumber(L, 0);
			return 1;
		}
		
		lua_pushnumber(L, ch->GetAttMul());
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

		LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid);
		if (!ch) {
			sys_err("The vid %d not exist.", vid);
			return 0;
		}

		ch->SetAttMul((float)lua_tonumber(L, 2));
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

		LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid);
		if (!ch) {
			sys_err("The vid %d not exist.", vid);
			lua_pushnumber(L, 0);
			return 1;
		}

		lua_pushnumber(L, ch->GetDamMul());
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

		LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(vid);
		if (!ch) {
			sys_err("The vid %d not exist.", vid);
			return 0;
		}

		ch->SetDamMul((float)lua_tonumber(L, 2));
		return 0;
	}

#ifdef ENABLE_NEWSTUFF
	ALUA(npc_get_level0)
	{
		// migrated from CHARACTER::GetLevel
		// DUAL-PATH: legacy fallback during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
		lua_pushnumber(L, ((npc)->GetLevel()));
		return 1;
	}

	ALUA(npc_get_name0)
	{
		// migrated from CHARACTER::GetName
		// DUAL-PATH: legacy fallback during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
		lua_pushstring(L, ((npc)->GetName()));
		return 1;
	}

	ALUA(npc_get_pid0)
	{
		// migrated from CHARACTER::GetPlayerID
		// DUAL-PATH: legacy fallback during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
		lua_pushnumber(L, ((npc)->GetPlayerID()));
		return 1;
	}

	ALUA(npc_get_vnum0)
	{
		// migrated from CHARACTER::GetRaceNum
		// DUAL-PATH: legacy fallback during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
		lua_pushnumber(L, ((npc)->GetRaceNum()));
		return 1;
	}

	ALUA(npc_is_available0)
	{
		// migrated from CHARACTER::IsNPC
		// DUAL-PATH: legacy fallback during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity npcEntity = q.GetCurrentNPCEntity();
		auto* npc = ecs::LegacyCharOf(npcEntity);
		lua_pushboolean(L, npc!= nullptr);
		return 1;
	}

#endif
#ifdef ENABLE_MONKEY_DUNGI_BY_RAZOR93
	int npc_decrease_hp_by_percent_razor93(lua_State* L)
	{
		lua_Number vid = lua_tonumber(L, 1);
		int percent = (int)lua_tonumber(L, 2);

		LPCHARACTER npc = CHARACTER_MANAGER::instance().Find(vid);
		if (npc && percent > 0 && percent <= 100)
		{
			int damage = npc->GetMaxHP() * percent / 100;
			ecs::PointSystem::Change(AIHelpers::EcsOf(npc), POINT_HP, -damage);  // HP-t csökkentjük
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



