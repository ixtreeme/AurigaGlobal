#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/ItemSystem.hpp"

#include "questlua.h"
#include "questmanager.h"
#include "horsename_manager.h"
#include "affect.h"
#include "config.h"
#include "utils.h"

#include "PetSystem.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

namespace quest
{

#ifdef __PET_SYSTEM__
	// syntax in LUA: pet.summon(mob_vnum, pet's name, (bool)run to me from far away)
	ALUA(pet_summon)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (!ecs::PlayerRuntime::IsValid(chEntity) || !ItemSystem::IsValidItem(item))
		{
			lua_pushnumber (L, 0);
			return 1;
		}

#ifdef ENABLE_PVP_ADVANCED
		if (ecs::PlayerRuntime::GetDuelOption(chEntity, "BlockPet"))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 516, "");
#endif
			lua_pushnumber (L, 0);
			return 1;
		}
#endif

		// ��ȯ���� vnum
		uint32_t mobVnum= lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		// ��ȯ���� �̸�
		ecs::PetActorState* pet = PetSystem::Summon(chEntity, mobVnum, item, "", false);

		if (pet != nullptr && ecs::PlayerRuntime::IsValid(pet->character))
			lua_pushnumber (L, ecs::PlayerRuntime::GetPacketVID(pet->character));
		else
			lua_pushnumber (L, 0);

		return 1;
	}

	// syntax: pet.unsummon(mob_vnum)
	ALUA(pet_unsummon)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		// ��ȯ���� vnum
		uint32_t mobVnum= lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		PetSystem::Unsummon(chEntity, mobVnum);
#ifdef ENABLE_RECALL
		const CAffect* pAffect = AffectSystem::FindAffect(chEntity, AFFECT_RECALL1);
		if (pAffect) {
			AffectSystem::RemoveAffect(chEntity, const_cast<CAffect*>(pAffect));
		}
#endif
		return 1;
	}

	// syntax: pet.unsummon(mob_vnum)
	ALUA(pet_count_summoned)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		lua_Number count = static_cast<lua_Number>(PetSystem::CountSummoned(chEntity));

		lua_pushnumber(L, count);

		return 1;
	}

	// syntax: pet.is_summon(mob_vnum)
	ALUA(pet_is_summon)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		// ��ȯ���� vnum
		uint32_t mobVnum= lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		lua_pushboolean(L, PetSystem::IsPetSummoned(chEntity, mobVnum));

		return 1;
	}

	ALUA(pet_spawn_effect)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		uint32_t mobVnum = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		const ecs::PetActorState* petActor = PetSystem::FindActor(chEntity, mobVnum);
		if (nullptr == petActor)
			return 0;
		const entt::entity pet = petActor->character;
		if (!ecs::PlayerRuntime::IsValid(pet))
			return 0;

		if (lua_isstring(L, 2))
		{
			NetworkSyncSystem::BroadcastSpecificEffect(g_registry, pet, lua_tostring(L, 2));
		}
		return 0;
	}

	void RegisterPetFunctionTable()
	{
		luaL_reg pet_functions[] =
		{
			{ "summon",			pet_summon			},
			{ "unsummon",		pet_unsummon		},
			{ "is_summon",		pet_is_summon		},
			{ "count_summoned",	pet_count_summoned	},
			{ "spawn_effect",	pet_spawn_effect	},
			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("pet", pet_functions);
	}
#endif

}



