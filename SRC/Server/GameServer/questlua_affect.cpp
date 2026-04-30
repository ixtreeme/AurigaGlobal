#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/AIHelpers.hpp"
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
#include "ecs/CharacterAccessors.hpp"
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
		// migrated from CHARACTER::AddAffect
		// TODO Phase 8: CAffect construction requires CHARACTER* - legacy only
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			return 0;
		}

		CQuestManager & q = CQuestManager::instance();

		uint8_t applyOn = static_cast<uint8_t>(lua_tonumber(L, 1));

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (applyOn >= MAX_APPLY_NUM || applyOn < 1)
		{
			sys_err("apply is out of range : {}", applyOn);
			return 0;
		}

		if (ch->FindAffect(AFFECT_QUEST_START_IDX, applyOn)) // 퀘스트로 인해 같은 곳에 효과가 걸려있으면 스킵
			return 0;

		int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));

		AffectSystem::AddAffect(AIHelpers::EcsOf(ch), AFFECT_QUEST_START_IDX, aApplyInfo[applyOn].bPointType, value, 0, duration, 0, false);

		return 0;
	}

	ALUA(affect_remove)
	{
		// migrated from CHARACTER::RemoveAffect
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();
		uint32_t iType;

		if (lua_isnumber(L, 1))
		{
			iType = static_cast<uint32_t>(lua_tonumber(L, 1));

			if (iType == 0)
				iType = q.GetCurrentPC()->GetCurrentQuestIndex() + AFFECT_QUEST_START_IDX;
		}
		else
			iType = q.GetCurrentPC()->GetCurrentQuestIndex() + AFFECT_QUEST_START_IDX;

		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), iType);

		return 0;
	}

	ALUA(affect_remove_bad)
	{
		// migrated from CHARACTER::RemoveBadAffect
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		ch->RemoveBadAffect();
		return 0;
	}

	ALUA(affect_remove_good)
	{
		// migrated from CHARACTER::RemoveGoodAffect
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		ch->RemoveGoodAffect();
		return 0;
	}

	ALUA(affect_add_hair)
	{
		// migrated from CHARACTER::AddAffect
		// TODO Phase 8: CAffect construction requires CHARACTER* - legacy only
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			return 0;
		}

		CQuestManager & q = CQuestManager::instance();

		uint8_t applyOn = static_cast<uint8_t>(lua_tonumber(L, 1));

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (applyOn >= MAX_APPLY_NUM || applyOn < 1)
		{
			sys_err("apply is out of range : {}", applyOn);
			return 0;
		}

		int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));

		AffectSystem::AddAffect(AIHelpers::EcsOf(ch), AFFECT_HAIR, aApplyInfo[applyOn].bPointType, value, 0, duration, 0, false);

		return 0;
	}

	ALUA(affect_remove_hair) // 헤어 효과를 없앤다.
	{
		// migrated from CHARACTER::FindAffect
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CAffect* pkAff = ch->FindAffect( AFFECT_HAIR );

		if ( pkAff != nullptr)
		{
			lua_pushnumber(L, pkAff->lDuration);
			AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch),  pkAff );
		}
		else
		{
			lua_pushnumber(L, 0);
		}

		return 1;
	}

	// 현재 캐릭터가 AFFECT_TYPE affect를 갖고있으면 bApplyOn 값을 반환하고 없으면 nil을 반환하는 함수.
	// usage :	applyOn = affect.get_apply(AFFECT_TYPE)
	ALUA(affect_get_apply_on)
	{
		// migrated from CHARACTER::FindAffect()
		if (!lua_isnumber(L, 1))
		{
			sys_err("invalid argument");
			return 0;
		}

		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		uint32_t affectType = lua_tonumber(L, 1);
		auto* al = ECS_TryGet<ecs::AffectList>(e);
		if (!al)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			if (!ch)
			{
				lua_pushnumber(L, 0);
				return 1;
			}
			CAffect* pkAff = ch->FindAffect(affectType);
			if (pkAff != nullptr)
				lua_pushnumber(L, pkAff->bApplyOn);
			else
				lua_pushnil(L);
			return 1;
		}

		for (auto* aff : al->affects)
		{
			if (aff && aff->dwType == affectType)
			{
				lua_pushnumber(L, aff->bApplyOn);
				return 1;
			}
		}

		lua_pushnil(L);
		return 1;
	}

	ALUA(affect_add_collect)
	{
		// migrated from CHARACTER::AddAffect
		// TODO Phase 8: CAffect construction requires CHARACTER* - legacy only
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			return 0;
		}

		CQuestManager & q = CQuestManager::instance();

		uint8_t applyOn = static_cast<uint8_t>(lua_tonumber(L, 1));

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (applyOn >= MAX_APPLY_NUM || applyOn < 1)
		{
			sys_err("apply is out of range : {}", applyOn);
			return 0;
		}

		int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));

		AffectSystem::AddAffect(AIHelpers::EcsOf(ch), AFFECT_COLLECT, aApplyInfo[applyOn].bPointType, value, 0, duration, 0, false);

		return 0;
	}
#ifdef __NEWPET_SYSTEM__
	ALUA (affect_pet_bonus)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			return 0;
		}

		CQuestManager & q = CQuestManager::instance();

		uint8_t applyOn = static_cast<uint8_t>(lua_tonumber(L, 1));

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (applyOn >= MAX_APPLY_NUM || applyOn < 1)
		{
			sys_err("apply is out of range : {}", applyOn);
			return 0;
		}

		int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));

		AffectSystem::AddAffect(AIHelpers::EcsOf(ch), AFFECT_PET, aApplyInfo[applyOn].bPointType, value, 0, duration, 0, false);

		return 0;
	}
#endif
	ALUA(affect_add_collect_point)
	{
		// migrated from CHARACTER::AddAffect
		// TODO Phase 8: CAffect construction requires CHARACTER* - legacy only
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument");
			return 0;
		}

		CQuestManager & q = CQuestManager::instance();

		uint8_t point_type = static_cast<uint8_t>(lua_tonumber(L, 1));

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (point_type >= POINT_MAX_NUM || point_type < 1)
		{
			sys_err("point is out of range : {}", point_type);
			return 0;
		}

		int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));

		AffectSystem::AddAffect(AIHelpers::EcsOf(ch), AFFECT_COLLECT, point_type, value, 0, duration, 0, false);

		return 0;
	}

	ALUA(affect_remove_collect)
	{
		// migrated from CHARACTER::RemoveAffect
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if ( ch != nullptr)
		{
			uint8_t bApply = static_cast<uint8_t>(lua_tonumber(L, 1));

			if ( bApply >= MAX_APPLY_NUM ) return 0;

			bApply = aApplyInfo[bApply].bPointType;
			int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));

			const std::list<CAffect*>& rList = ch->GetAffectContainer();
			const CAffect* pAffect = nullptr;

			for (auto iter = rList.begin(); iter != rList.end(); ++iter )
			{
				pAffect = *iter;

				if ( pAffect->dwType == AFFECT_COLLECT )
				{
					if ( pAffect->bApplyOn == bApply && pAffect->lApplyValue == value )
					{
						break;
					}
				}

				pAffect = nullptr;
			}

			if ( pAffect != nullptr)
			{
				AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch),  const_cast<CAffect*>(pAffect) );
			}
		}

		return 0;
	}

	ALUA(affect_remove_all_collect)
	{
		// migrated from CHARACTER::RemoveAffect
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if ( ch != nullptr)
		{
			AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), AFFECT_COLLECT);
		}

		return 0;
	}
	
#ifdef ENABLE_VOTE4BUFF
	ALUA(affect_add_affect)
	{
		// migrated from CHARACTER::AddAffect
		// TODO Phase 8: CAffect construction requires CHARACTER* - legacy only
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) || !ch)
			return 0;
		uint32_t affectType = (uint32_t)lua_tonumber(L, 1);
		uint8_t pointType = (uint8_t)lua_tonumber(L, 2);
		int32_t pointValue = (int32_t)lua_tonumber(L, 3);
		int32_t duration = (int32_t)lua_tonumber(L, 4);
		if (pointType >= POINT_MAX_NUM)
			return 0;
		AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), affectType);
		AffectSystem::AddAffect(AIHelpers::EcsOf(ch), affectType, pointType, pointValue, AFF_NONE, duration, 0, false, false);
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


