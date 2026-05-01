#include "stdafx.h"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "questmanager.h"
#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif
#include "char_interface.hpp"
#include "ecs/CharacterAccessors.hpp"
#include "sectree_manager.h"
#include "target.h"

namespace quest
{
	//
	// "target" Lua functions
	//
	ALUA(target_pos)
	{
		// migrated from CHARACTER::Target position lookup
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		uint32_t iQuestIndex = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		if (!lua_isstring(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
		{
			sys_err("invalid argument, name: {}, quest_index {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), iQuestIndex);
			return 0;
		}

		PIXEL_POSITION pos;

		if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)), pos))
		{
			sys_err("cannot find base position in this map {}", ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)));
			return 0;
		}

		int x = pos.x + (int) lua_tonumber(L, 2) * 100;
		int y = pos.y + (int) lua_tonumber(L, 3) * 100;

		CTargetManager::instance().CreateTarget((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))),
				iQuestIndex,
				lua_tostring(L, 1),
				TARGET_TYPE_POS,
				x,
				y,
				(int) lua_tonumber(L, 4),
				lua_isstring(L, 5) ? lua_tostring(L, 5) : nullptr,
				lua_isnumber(L, 6) ? (int)lua_tonumber(L, 6): 1);

		return 0;
	}

	ALUA(target_vid)
	{
		// migrated from CHARACTER::Target VID lookup
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		uint32_t iQuestIndex = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		if (!lua_isstring(L, 1) || !lua_isnumber(L, 2))
		{
			sys_err("invalid argument, name: {}, quest_index {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), iQuestIndex);
			return 0;
		}


		CTargetManager::instance().CreateTarget((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))),
				iQuestIndex,
				lua_tostring(L, 1),
				TARGET_TYPE_VID,
				(int) lua_tonumber(L, 2),
				0,
				ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch)),
				lua_isstring(L, 3) ? lua_tostring(L, 3) : nullptr,
				lua_isnumber(L, 4) ? (int)lua_tonumber(L, 4): 1);

		return 0;
	}

	// ���� ����Ʈ�� ��ϵ� Ÿ���� ���� �Ѵ�.
	ALUA(target_delete)
	{
		// migrated from CHARACTER::DeleteTarget
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		uint32_t iQuestIndex = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		if (!lua_isstring(L, 1))
		{
			sys_err("invalid argument, name: {}, quest_index {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), iQuestIndex);
			return 0;
		}

		CTargetManager::instance().DeleteTarget((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))), iQuestIndex, lua_tostring(L, 1));

		return 0;
	}

	// ���� ����Ʈ �ε����� �Ǿ��ִ� Ÿ���� ��� �����Ѵ�.
	ALUA(target_clear)
	{
		// migrated from CHARACTER::DeleteTarget
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		uint32_t iQuestIndex = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		CTargetManager::instance().DeleteTarget((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))), iQuestIndex, nullptr);

		return 0;
	}

	ALUA(target_id)
	{
		// migrated from CHARACTER::GetTargetEvent
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		uint32_t dwQuestIndex = CQuestManager::instance().GetCurrentPC()->GetCurrentQuestIndex();

		if (!lua_isstring(L, 1))
		{
			sys_err("invalid argument, name: {}, quest_index {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), dwQuestIndex);
			lua_pushnumber(L, 0);
			return 1;
		}

		LPEVENT pkEvent = CTargetManager::instance().GetTargetEvent((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))), dwQuestIndex, (const char *) lua_tostring(L, 1));

		if (pkEvent)
		{
			const auto pInfo = dynamic_cast<TargetInfo *>(pkEvent->info);

			if ( pInfo == nullptr)
			{
				sys_err("target_id> <Factor> Null pointer");
				lua_pushnumber(L, 0);
				return 1;
			}

			if (pInfo->iType == TARGET_TYPE_VID)
			{
				lua_pushnumber(L, pInfo->iArg1);
				return 1;
			}
		}

		lua_pushnumber(L, 0);
		return 1;
	}

	void RegisterTargetFunctionTable()
	{
		luaL_reg target_functions[] =
		{
			{ "pos",			target_pos		},
			{ "vid",			target_vid		},
			{ "npc",			target_vid		}, // TODO: delete this
			{ "pc",			target_vid		}, // TODO: delete this
			{ "delete",			target_delete		},
			{ "clear",			target_clear		},
			{ "id",			target_id		},
			{nullptr, nullptr},
		};

		CQuestManager::instance().AddLuaFunctionTable("target", target_functions);
	}
};



