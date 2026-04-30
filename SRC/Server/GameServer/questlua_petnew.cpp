#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/AIHelpers.hpp"

#include "questlua.h"
#include "questmanager.h"
#include "horsename_manager.h"
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "affect.h"
#include "config.h"
#include "utils.h"
#include "db.h"

#include "New_PetSystem.h"
#include "ecs/EntityFactory.hpp"
#include "ecs/Registry.hpp"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

extern int(*check_name) (const char * str);

namespace quest
{

#ifdef __NEWPET_SYSTEM__
	// syntax in LUA: pet.summon(mob_vnum, pet's name, (bool)run to me from far away)
	ALUA (newpet_summon)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CNewPetSystem* petSystem = ch->GetNewPetSystem();
		LPITEM pItem = CQuestManager::instance().GetCurrentItem();
		if (!ch || !petSystem || !pItem)
		{
			lua_pushnumber(L, 0);
			return 1;
		}

#ifdef ENABLE_PVP_ADVANCED
		if ((ch->GetDuel("BlockPet")))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 516, "");
#endif
			lua_pushnumber (L, 0);
			return 1;
		}
#endif

		if (nullptr == petSystem)
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		// 소환수의 vnum
		uint32_t mobVnum = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		// 소환수의 이름		

		const char* petName = lua_isstring(L, 2) ? lua_tostring(L, 2) : nullptr;

		// 소환하면 멀리서부터 달려오는지 여부
		bool bFromFar = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : false;

		CNewPetActor* pet = petSystem->Summon(mobVnum, EntityFactory::CreateItemEntity(g_registry, pItem), petName, bFromFar);

		if (pet != nullptr)
			lua_pushnumber(L, pet->GetVID());
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	// syntax: pet.unsummon(mob_vnum)
	ALUA (newpet_unsummon)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
			return 0;
		
		CNewPetSystem* petSystem = ch->GetNewPetSystem();

		if (nullptr == petSystem)
			return 0;

		// 소환수의 vnum
		uint32_t mobVnum = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		petSystem->Unsummon(mobVnum);
#ifdef ENABLE_RECALL
		const CAffect* pAffect = ch->FindAffect(AFFECT_RECALL2);
		if (pAffect) {
			AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), const_cast<CAffect*>(pAffect));
		}
#endif
		return 1;
	}

	// syntax: pet.unsummon(mob_vnum)
	ALUA (newpet_count_summoned)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CNewPetSystem* petSystem = ch->GetNewPetSystem();

		lua_Number count = 0;

		if (nullptr != petSystem)
			count = static_cast<lua_Number>(petSystem->CountSummoned());

		lua_pushnumber(L, count);

		return 1;
	}

	// syntax: pet.is_summon(mob_vnum)
	ALUA (newpet_is_summon)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CNewPetSystem* petSystem = ch->GetNewPetSystem();

		if (nullptr == petSystem)
			return 0;

		// 소환수의 vnum
		uint32_t mobVnum = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		CNewPetActor* petActor = petSystem->GetByVnum(mobVnum);

		if (nullptr == petActor)
			lua_pushboolean(L, false);
		else
			lua_pushboolean(L, petActor->IsSummoned());

		return 1;
	}

	ALUA (newpet_increaseskill)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CNewPetSystem* petSystem = ch->GetNewPetSystem();

		if (nullptr == petSystem)
			return 0;

		// 소환수의 vnum
#ifdef ENABLE_NEW_PET_EDITS
		lua_pushboolean(L, false);
#else
		uint32_t skill = lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;
		bool petActor = petSystem->IncreasePetSkill(skill);
		if (!petActor)
			lua_pushboolean(L, false);
		else
			lua_pushboolean(L, petActor);
#endif
		return 1;

	}

	ALUA (newpet_increaseevolution)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CNewPetSystem* petSystem = ch->GetNewPetSystem();

		if (nullptr == petSystem)
			return 0;

		// 소환수의 vnum		

		bool petActor = petSystem->IncreasePetEvolution();

		if (!petActor)
			lua_pushboolean(L, false);
		else
			lua_pushboolean(L, petActor);
		return 1;

	}

	ALUA (newpet_get_level)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CNewPetSystem* petSystem = ch->GetNewPetSystem();

		if (nullptr == petSystem) {
			lua_pushnumber(L, -1);
			return 0;
		}
		int pet_level = petSystem->GetLevel();

		if (pet_level == 0)
			lua_pushnumber(L, -1);
		else
			lua_pushnumber(L, pet_level);

		return 1;

	}

	ALUA (newpet_get_evo)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CNewPetSystem* petSystem = ch->GetNewPetSystem();

		if (nullptr == petSystem) {
			lua_pushnumber(L, -1);
			return 0;
		}
		int pet_evo = petSystem->GetEvolution();

		if (0 == pet_evo)
			lua_pushnumber(L, -1);
		else
			lua_pushnumber(L, pet_evo);

		return 1;

	}

	ALUA (newpet_restore_pet)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent

		uint32_t id = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;
		if (id == 0){
			lua_pushboolean(L, false);
			return 0;
		}

		char szQuery1[1024];
		snprintf(szQuery1, sizeof(szQuery1), "SELECT duration,tduration FROM new_petsystem WHERE id = %u ", id);
		std::unique_ptr<SQLMsg> pmsg2(DBManager::instance().DirectQuery(szQuery1));
		if (pmsg2->Get()->uiNumRows > 0) {
			MYSQL_ROW row = mysql_fetch_row(pmsg2->Get()->pSQLResult);
			if (atoi(row[0]) <= 0){
				delete(DBManager::instance().DirectQuery("UPDATE new_petsystem SET duration=%d WHERE id = %u ", atoi(row[1]), id));
				lua_pushboolean(L, true);
			}
			else{
				lua_pushboolean(L, false);
			}
		}
		else{
			lua_pushboolean(L, false);
		}

		return 1;
	}

	ALUA (newpet_spawn_effect)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CNewPetSystem* petSystem = ch->GetNewPetSystem();

		if (nullptr == petSystem)
			return 0;

		uint32_t mobVnum = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		CNewPetActor* petActor = petSystem->GetByVnum(mobVnum);
		if (nullptr == petActor)
			return 0;
		LPCHARACTER pet_ch = petActor->GetCharacter();
		if (nullptr == pet_ch)
			return 0;

		if (lua_isstring(L, 2))
		{
			pet_ch->SpecificEffectPacket(lua_tostring(L, 2));
		}
		return 0;
	}

	ALUA(newpet_eggrequest)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		int evid = lua_isnumber(L, 0) ? static_cast<int>(lua_tonumber(L, 0)) : 0;
		ch->SetEggVid(evid);
		return 1;
	}
	
#ifdef ENABLE_NEW_PET_EDITS
	ALUA(newpet_reset_skills)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch) {
			lua_pushnumber(L, 2);
			return 1;
		}
		
		CNewPetSystem* petSystem = ch->GetNewPetSystem();
		if (!petSystem) {
			lua_pushnumber(L, 2);
			return 1;
		}
		
		lua_pushnumber(L, petSystem->ResetSkills());
		return 1;
	}
	
	ALUA(newpet_reset_skill)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch) {
			lua_pushnumber(L, 2);
			return 1;
		}
		
		CNewPetSystem* petSystem = ch->GetNewPetSystem();
		if (!petSystem) {
			lua_pushnumber(L, 2);
			return 1;
		}
		
		int iType = lua_isnumber(L, 1) ? static_cast<int>(lua_tonumber(L, 1)) : 0;
		lua_pushnumber(L, petSystem->ResetSkill(iType));
		return 1;
	}
#endif

	ALUA(newpet_change_name)
	{
		// migrated from CHARACTER CNewPetSystem
		// TODO Phase 8: dedicated NewPetComponent
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
			return 0;

		if (lua_isstring(L, 1) != true) {
			lua_pushnumber(L, 0);
			return 1;
		}

		const char * szName = lua_tostring(L, 1);
		if (check_name(szName) == false) {
			lua_pushnumber(L, 0);
			return 1;
		}

		CNewPetSystem* petSystem = ch->GetNewPetSystem();
		if (!petSystem) {
			lua_pushnumber(L, 3);
			return 1;
		}

		char query1[256] = {0};
		snprintf(query1, sizeof(query1), "SELECT id FROM player.new_petsystem WHERE name='%s';", szName);
		std::unique_ptr<SQLMsg> pRes(DBManager::instance().DirectQuery(query1));
		if (pRes->Get()->uiNumRows > 0) {
			lua_pushnumber(L, 2);
			return 1;
		} else {
			petSystem->ChangeName(szName);
			lua_pushnumber(L, 1);
			return 1;
		}
	}

	void RegisterNewPetFunctionTable()
	{
		luaL_reg pet_functions[] =
		{
			{ "EggRequest",		newpet_eggrequest},
			{ "summon",			newpet_summon },
			{ "unsummon",		newpet_unsummon },
			{ "is_summon",		newpet_is_summon },
			{ "count_summoned",	newpet_count_summoned },
			{ "spawn_effect",	newpet_spawn_effect },
			{ "increaseskill",	newpet_increaseskill},
			{ "increaseevo",	newpet_increaseevolution},
			{ "getlevel",		newpet_get_level },
			{ "getevo",			newpet_get_evo },
			{ "restorepet",		newpet_restore_pet},
#ifdef ENABLE_NEW_PET_EDITS
			{"reset_skills", newpet_reset_skills},
			{"reset_skill", newpet_reset_skill},
#endif
			{"change_name", newpet_change_name},
			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("newpet", pet_functions);
	}
#endif

}


