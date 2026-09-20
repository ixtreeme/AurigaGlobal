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
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		if (!ecs::PlayerRuntime::IsValid(chEntity) || !ItemSystem::IsValidItem(item))
		{
			lua_pushnumber(L, 0);
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
		uint32_t mobVnum = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		// ��ȯ���� �̸�		

		const char* petName = lua_isstring(L, 2) ? lua_tostring(L, 2) : nullptr;

		// ��ȯ�ϸ� �ָ������� �޷������� ����
		bool bFromFar = lua_isboolean(L, 3) ? lua_toboolean(L, 3) : false;

		ecs::NewPetActorState* pet = NewPetSystem::Summon(chEntity, mobVnum, item, petName, bFromFar);

		if (pet != nullptr)
			lua_pushnumber(L, pet->vid);
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	// syntax: pet.unsummon(mob_vnum)
	ALUA (newpet_unsummon)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (!ecs::PlayerRuntime::IsValid(chEntity))
			return 0;

		// ��ȯ���� vnum
		uint32_t mobVnum = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		NewPetSystem::Unsummon(chEntity, mobVnum);
#ifdef ENABLE_RECALL
		const CAffect* pAffect = AffectSystem::FindAffect(chEntity, AFFECT_RECALL2);
		if (pAffect) {
			AffectSystem::RemoveAffect(chEntity, const_cast<CAffect*>(pAffect));
		}
#endif
		return 1;
	}

	// syntax: pet.unsummon(mob_vnum)
	ALUA (newpet_count_summoned)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		lua_Number count = static_cast<lua_Number>(NewPetSystem::CountSummoned(chEntity));

		lua_pushnumber(L, count);

		return 1;
	}

	// syntax: pet.is_summon(mob_vnum)
	ALUA (newpet_is_summon)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (!ecs::PlayerRuntime::IsValid(chEntity))
			return 0;

		// ��ȯ���� vnum
		uint32_t mobVnum = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		ecs::NewPetActorState* petActor = NewPetSystem::FindActor(chEntity, mobVnum);

		lua_pushboolean(L, petActor != nullptr && NewPetSystem::IsSummoned(*petActor));

		return 1;
	}

	ALUA (newpet_increaseskill)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (!ecs::PlayerRuntime::IsValid(chEntity))
			return 0;

		// ��ȯ���� vnum
#ifdef ENABLE_NEW_PET_EDITS
		lua_pushboolean(L, false);
#else
		uint32_t skill = lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0;
		lua_pushboolean(L, NewPetSystem::IncreasePetSkill(chEntity, skill));
#endif
		return 1;

	}

	ALUA (newpet_increaseevolution)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (!ecs::PlayerRuntime::IsValid(chEntity))
			return 0;

		// ��ȯ���� vnum		

		lua_pushboolean(L, NewPetSystem::IncreasePetEvolution(chEntity));
		return 1;

	}

	ALUA (newpet_get_level)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (!ecs::PlayerRuntime::IsValid(chEntity)) {
			lua_pushnumber(L, -1);
			return 0;
		}
		int pet_level = NewPetSystem::GetLevel(chEntity);

		if (pet_level == 0)
			lua_pushnumber(L, -1);
		else
			lua_pushnumber(L, pet_level);

		return 1;

	}

	ALUA (newpet_get_evo)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (!ecs::PlayerRuntime::IsValid(chEntity)) {
			lua_pushnumber(L, -1);
			return 0;
		}
		int pet_evo = NewPetSystem::GetEvolution(chEntity);

		if (0 == pet_evo)
			lua_pushnumber(L, -1);
		else
			lua_pushnumber(L, pet_evo);

		return 1;

	}

	ALUA (newpet_restore_pet)
	{
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
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		uint32_t mobVnum = lua_isnumber(L, 1) ? static_cast<uint32_t>(lua_tonumber(L, 1)) : 0;

		ecs::NewPetActorState* petActor = NewPetSystem::FindActor(chEntity, mobVnum);
		if (petActor == nullptr || !NewPetSystem::IsSummoned(*petActor))
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

	ALUA(newpet_eggrequest)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		int evid = lua_isnumber(L, 0) ? static_cast<int>(lua_tonumber(L, 0)) : 0;
		ecs::PlayerRuntime::SetEggVID(chEntity, evid);
		return 1;
	}
	
#ifdef ENABLE_NEW_PET_EDITS
	ALUA(newpet_reset_skills)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, NewPetSystem::ResetSkills(chEntity));
		return 1;
	}
	
	ALUA(newpet_reset_skill)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		int iType = lua_isnumber(L, 1) ? static_cast<int>(lua_tonumber(L, 1)) : 0;
		lua_pushnumber(L, NewPetSystem::ResetSkill(chEntity, iType));
		return 1;
	}
#endif

	ALUA(newpet_change_name)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (lua_isstring(L, 1) != true) {
			lua_pushnumber(L, 0);
			return 1;
		}

		const char * szName = lua_tostring(L, 1);
		if (check_name(szName) == false) {
			lua_pushnumber(L, 0);
			return 1;
		}

		if (!ecs::PlayerRuntime::IsValid(chEntity)) {
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
			NewPetSystem::ChangeName(chEntity, szName);
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
