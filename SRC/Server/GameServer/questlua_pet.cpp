#include "stdafx.h"

#include "questlua.h"
#include "questmanager.h"
#include "horsename_manager.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "affect.h"
#include "config.h"
#include "utils.h"

#include "PetSystem.h"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, __VA_ARGS__)
#endif

namespace quest
{

#ifdef __PET_SYSTEM__
	// syntax in LUA: pet.summon(mob_vnum, pet's name, (bool)run to me from far away)
	ALUA(pet_summon)
	{
		// migrated from CHARACTER CPetSystem
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CPetSystem* petSystem = ch->GetPetSystem();
		LPITEM pItem = CQuestManager::instance().GetCurrentItem();
		if (!ch || !petSystem || !pItem)
		{
			lua_pushnumber (L, 0);
			return 1;
		}

#ifdef ENABLE_PVP_ADVANCED
		if ((ch->GetDuel("BlockPet")))
		{
#ifdef TEXTS_IMPROVEMENT
			ch->ChatPacketNew(CHAT_TYPE_INFO, 516, "");
#endif
			lua_pushnumber (L, 0);
			return 1;
		}
#endif

		if (nullptr == petSystem)
		{
			lua_pushnumber (L, 0);
			return 1;
		}

		// 소환수의 vnum
		uint32_t mobVnum= lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		// 소환수의 이름
		CPetActor* pet = petSystem->Summon(mobVnum, pItem, "", false);

		if (pet != nullptr)
			lua_pushnumber (L, pet->GetVID());
		else
			lua_pushnumber (L, 0);

		return 1;
	}

	// syntax: pet.unsummon(mob_vnum)
	ALUA(pet_unsummon)
	{
		// migrated from CHARACTER CPetSystem
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
			return 0;
		
		CPetSystem* petSystem = ch->GetPetSystem();

		if (nullptr == petSystem)
			return 0;

		// 소환수의 vnum
		uint32_t mobVnum= lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		petSystem->Unsummon(mobVnum);
#ifdef ENABLE_RECALL
		const CAffect* pAffect = ch->FindAffect(AFFECT_RECALL1);
		if (pAffect) {
			ch->RemoveAffect(const_cast<CAffect*>(pAffect));
		}
#endif
		return 1;
	}

	// syntax: pet.unsummon(mob_vnum)
	ALUA(pet_count_summoned)
	{
		// migrated from CHARACTER CPetSystem
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CPetSystem* petSystem = ch->GetPetSystem();

		lua_Number count = 0;

		if (nullptr != petSystem)
			count = static_cast<lua_Number>(petSystem->CountSummoned());

		lua_pushnumber(L, count);

		return 1;
	}

	// syntax: pet.is_summon(mob_vnum)
	ALUA(pet_is_summon)
	{
		// migrated from CHARACTER CPetSystem
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CPetSystem* petSystem = ch->GetPetSystem();

		if (nullptr == petSystem)
			return 0;

		// 소환수의 vnum
		uint32_t mobVnum= lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		CPetActor* petActor = petSystem->GetByVnum(mobVnum);

		if (nullptr == petActor)
			lua_pushboolean(L, false);
		else
			lua_pushboolean(L, petActor->IsSummoned());

		return 1;
	}

	ALUA(pet_spawn_effect)
	{
		// migrated from CHARACTER CPetSystem
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CPetSystem* petSystem = ch->GetPetSystem();

		if (nullptr == petSystem)
			return 0;

		uint32_t mobVnum = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		CPetActor* petActor = petSystem->GetByVnum(mobVnum);
		if (nullptr == petActor)
			return 0;
		LPCHARACTER pet_ch = petActor->GetCharacter();
		if (nullptr == pet_ch)
			return 0;

		if (lua_isstring(L, 2))
		{
			pet_ch->SpecificEffectPacket (lua_tostring(L, 2));
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


