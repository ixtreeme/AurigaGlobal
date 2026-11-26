#include "TypedShop.hpp"
#include "../mission/TypedMissionRuntime.hpp"
#include "../item/ItemStore.hpp"
#include "../item/TypedMoney.hpp"
#include "../item/TypedQuestItems.hpp"
#include "../item/TypedDungeonActions.hpp"
#include "../item/TypedDrops.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "typed_money_continuation.h"
#include "TypedIdentityScrolls.hpp"
#include "stdafx.h"
#include "../warrior/WarriorSkillSystem.hpp"
#include <cmath>
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PersistentRace.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/AIHelpers.hpp"

#include "config.h"
#include "questmanager.h"
#include "sectree_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "../ecs/CharacterAccessors.hpp"
#include "affect.h"
#include "guild_manager.h"
#include "war_map.h"
#include "start_position.h"
#include "marriage.h"
#include "p2p.h"
#include "desc_client.h"
#include "messenger_manager.h"
#include "log.h"
#include "utils.h"
#include "unique_item.h"
#include "mob_manager.h"
#include "dungeon.h"
#include "../ecs/quest_helpers.hpp"
#include "../ecs/EntityFactory.hpp"
#include "../ecs/events.hpp"
#include "../ecs/components/appearance_components.hpp"
#include "../ecs/components/inventory_components.hpp"
#include "../ecs/components/social_components.hpp"


#ifdef ENABLE_NEWSTUFF
#include "pvp.h"
#endif


#ifdef ENABLE_DICE_SYSTEM
#include "party.h"
#endif

#include <cctype>
#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

// #define ENABLE_LOCALECHECK_CHANGENAME
// #define ENABLE_PC_OPENSHOP

#ifdef ENABLE_PC_OPENSHOP
#include "shop.h"
#endif

namespace quest
{
	//
	// "pc" Lua functions
	//
	ALUA(pc_has_master_skill)
	{
		// migrated from CHARACTER::GetSkillMasterType
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* skills = ECS_TryGet<ecs::SkillLevels>(e))
		{
			bool bHasMasterSkill = false;
			if (skills->loaded)
			{
				for (int i = 0; i < SKILL_MAX_NUM; ++i)
				{
					if (skills->levels[i].bMasterType >= SKILL_MASTER && skills->levels[i].bLevel >= 21)
					{
						bHasMasterSkill = true;
						break;
					}
				}
			}
			lua_pushboolean(L, bHasMasterSkill ? 1 : 0);
			return 1;
		}
		lua_pushboolean(L, 0);
		return 1;
	}

	ALUA(pc_remove_skill_book_no_delay)
	{
        // migrated from CHARACTER::RemoveAffect
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		AffectSystem::RemoveAffect(chEntity, AFFECT_SKILL_NO_BOOK_DELAY);
		return 0;
	}

	ALUA(pc_is_skill_book_no_delay)
	{
		// migrated from CHARACTER::FindAffect
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* affects = ECS_TryGet<ecs::AffectList>(e))
		{
			bool found = false;
			for (const auto& slot : affects->affects)
			{
				if (slot.id && slot.value.dwType == AFFECT_SKILL_NO_BOOK_DELAY)
				{
					found = true;
					break;
				}
			}
			lua_pushboolean(L, found ? 1 : 0);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, AffectSystem::FindAffect(chEntity, AFFECT_SKILL_NO_BOOK_DELAY) ? 1 : 0);
		return 1;
	}

    ALUA(pc_get_warrior_expansion_skills)
    {
        lua_newtable(L);
#ifdef ENABLE_WARRIOR_SKILL_EXPANSION
        const auto player = CQuestManager::instance().GetCurrentPCEntity();
        int index = 1;
        for (uint32_t id = WarriorExpansion::FirstSkill; id <= WarriorExpansion::LastSkill; ++id)
            if (WarriorExpansion::IsChosen(player, id) && SkillSystem::GetSkillLevel(player, id) > 0)
            {
                lua_pushinteger(L, id);
                lua_rawseti(L, -2, index++);
            }
#endif
        return 1;
    }

	ALUA(pc_learn_grand_master_skill)
	{
        // migrated from CHARACTER::LearnGrandMasterSkill
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (!lua_isnumber(L, 1))
		{
			sys_err("wrong skill index");
			return 0;
		}

		lua_pushboolean(L, SkillSystem::LearnGrandMasterSkill(chEntity, static_cast<uint32_t>(lua_tonumber(L, 1))));
		return 1;
	}

    ALUA(pc_set_warp_location)
    {
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        if (!lua_isnumber(L, 1))
        {
            sys_err("wrong map index");
            return 0;
        }
        if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        {
            sys_err("wrong coodinate");
            return 0;
        }
        const int32_t mapIndex = static_cast<int32_t>(lua_tonumber(L, 1));
        const int32_t x = static_cast<int32_t>(lua_tonumber(L, 2));
        const int32_t y = static_cast<int32_t>(lua_tonumber(L, 3));
		ecs::MovementSystem::SetWarpLocation(chEntity, mapIndex, x, y);
        return 0;
    }

    ALUA(pc_set_warp_location_local)
    {
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        if (!lua_isnumber(L, 1))
        {
            sys_err("wrong map index");
            return 0;
        }
        if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        {
            sys_err("wrong coodinate");
            return 0;
        }
        int32_t lMapIndex = static_cast<int32_t>(lua_tonumber(L, 1));
        const TMapRegion * region = SECTREE_MANAGER::instance().GetMapRegion(lMapIndex);
        if (!region)
        {
            sys_err("invalid map index {}", lMapIndex);
            return 0;
        }
        int32_t x = static_cast<int32_t>(lua_tonumber(L, 2));
        int32_t y = static_cast<int32_t>(lua_tonumber(L, 3));
        if (x > region->ex - region->sx)
        {
            sys_err("x coordinate overflow max: {} input: {}", region->ex - region->sx, x);
            return 0;
        }
        if (y > region->ey - region->sy)
        {
            sys_err("y coordinate overflow max: {} input: {}", region->ey - region->sy, y);
            return 0;
        }
		ecs::MovementSystem::SetWarpLocation(chEntity, lMapIndex, x, y);
        return 0;
    }

	ALUA(pc_get_start_location)
	{
		// migrated from CHARACTER::GetEmpire
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* empire = ECS_TryGet<ecs::EmpireComponent>(e))
		{
			const uint8_t empireIndex = empire->value;
			lua_pushnumber(L, g_start_map[empireIndex]);
			lua_pushnumber(L, g_start_position[empireIndex][0] / 100);
			lua_pushnumber(L, g_start_position[empireIndex][1] / 100);
			return 3;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint8_t empireIndex = ecs::PlayerRuntime::GetEmpire(chEntity);
		lua_pushnumber(L, g_start_map[empireIndex]);
		lua_pushnumber(L, g_start_position[empireIndex][0] / 100);
		lua_pushnumber(L, g_start_position[empireIndex][1] / 100);
		return 3;
	}

    ALUA(pc_warp)
    {
        // migrated from CHARACTER::WarpSet
        // DUAL-PATH: ECS component update + legacy warp packet
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

        if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
        {
            lua_pushboolean(L, false);
            return 1;
        }
        int32_t map_index = 0;
        if (lua_isnumber(L, 3))
            map_index = static_cast<int32_t>(lua_tonumber(L, 3));
        if (chEntity == entt::null)
        {
            lua_pushboolean(L, false);
            return 1;
        }
        if (ecs::PlayerRuntime::IsHack(chEntity))
        {
            lua_pushboolean(L, false);
            return 1;
        }
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* warpPos = ECS_TryGet<ecs::WarpPosition>(e))
        {
            warpPos->x = static_cast<int32_t>(lua_tonumber(L, 1));
            warpPos->y = static_cast<int32_t>(lua_tonumber(L, 2));
            warpPos->mapIndex = map_index ? map_index : ecs::PlayerRuntime::GetMapIndex(chEntity);
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if ( test_server )
            ecs::ChatSystem::Send(chEntity,  CHAT_TYPE_INFO, "pc_warp %d %d %d",static_cast<int>(lua_tonumber(L, 1)), static_cast<int>(lua_tonumber(L, 2)),map_index );
        ecs::MovementSystem::WarpSet(chEntity, static_cast<int32_t>(lua_tonumber(L, 1)), static_cast<int32_t>(lua_tonumber(L, 2)), map_index);
        lua_pushboolean(L, true);
        return 1;
    }

    ALUA(pc_warp_local)
    {
        // migrated from CHARACTER::WarpSet
        // DUAL-PATH: ECS component update + legacy warp packet
        if (!lua_isnumber(L, 1))
        {
            sys_err("no map index argument");
            return 0;
        }
        if (!lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        {
            sys_err("no coodinate argument");
            return 0;
        }
        int32_t lMapIndex = static_cast<int32_t>(lua_tonumber(L, 1));
        const TMapRegion * region = SECTREE_MANAGER::instance().GetMapRegion(lMapIndex);
        if (!region)
        {
            sys_err("invalid map index {}", lMapIndex);
            return 0;
        }
        int x = (int) lua_tonumber(L, 2);
        int y = (int) lua_tonumber(L, 3);
        if (x > region->ex - region->sx)
        {
            sys_err("x coordinate overflow max: {} input: {}", region->ex - region->sx, x);
            return 0;
        }
        if (y > region->ey - region->sy)
        {
            sys_err("y coordinate overflow max: {} input: {}", region->ey - region->sy, y);
            return 0;
        }
        const int32_t warpX = region->sx + x;
        const int32_t warpY = region->sy + y;
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* warpPos = ECS_TryGet<ecs::WarpPosition>(e))
        {
            warpPos->x = warpX;
            warpPos->y = warpY;
            warpPos->mapIndex = lMapIndex;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        if (chEntity != entt::null && g_registry.valid(chEntity))
            ecs::MovementSystem::WarpSet(chEntity, warpX, warpY);
        return 0;
    }

    ALUA(pc_warp_exit)
    {
        // migrated from CHARACTER::ExitToSavedLocation
        // DUAL-PATH: ECS component update + legacy warp packet
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* warpPos = ECS_TryGet<ecs::WarpPosition>(e))
        {
            if (const auto* exitPos = ECS_TryGet<ecs::ExitPosition>(e))
            {
                warpPos->x = exitPos->x;
                warpPos->y = exitPos->y;
                warpPos->mapIndex = exitPos->mapIndex;
                g_registry.emplace_or_replace<ecs::DirtyTag>(e);
            }
        }
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        if (chEntity != entt::null && g_registry.valid(chEntity))
            ecs::MovementSystem::ExitToSavedLocation(chEntity);
        return 0;
    }

	ALUA(pc_in_dungeon)
	{
		// migrated from CHARACTER::GetDungeon
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* dungeon = ECS_TryGet<ecs::DungeonMembership>(e))
		{
			lua_pushboolean(L, dungeon->dungeon != entt::null);
			return 1;
		}
		lua_pushboolean(L, 0);
		return 1;
	}

	ALUA(pc_hasguild)
	{
		// migrated from CHARACTER::GetGuild
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		lua_pushboolean(L, ecs::SocialSystem::GetGuild(e) ? 1 : 0);
		return 1;
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, ecs::SocialSystem::GetGuild(chEntity) ? 1 : 0);
		return 1;
	}

	ALUA(pc_getguild)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		CGuild* guild = ecs::SocialSystem::GetGuild(chEntity);
		lua_pushnumber(L, guild ? guild->GetID() : 0);
		return 1;
	}

	ALUA(pc_isguildmaster)
	{
		// migrated from CHARACTER::GetGuild
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const CGuild* guild = ecs::SocialSystem::GetGuild(e))
		{
			const auto* playerId = ECS_TryGet<ecs::PlayerID>(e);
			lua_pushboolean(L, (playerId && playerId->pid == guild->GetMasterPID()) ? 1 : 0);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		CGuild * g = ecs::SocialSystem::GetGuild(chEntity);
		lua_pushboolean(L, (g && ecs::PlayerRuntime::GetPlayerID(chEntity) == g->GetMasterPID()) ? 1 : 0);
		return 1;
	}

	ALUA(pc_destroy_guild)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		CGuild * g = ecs::SocialSystem::GetGuild(chEntity);

		if (g)
			g->RequestDisband(ecs::PlayerRuntime::GetPlayerID(chEntity));

		return 0;
	}

	ALUA(pc_remove_from_guild)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		CGuild * g = ecs::SocialSystem::GetGuild(chEntity);

		if (g)
			g->RequestRemoveMember(ecs::PlayerRuntime::GetPlayerID(chEntity));

		return 0;
	}

    ALUA(pc_give_gold)
    {
        // migrated from CHARACTER::PointChange
        // DUAL-PATH: ECS update + legacy call during migration window
        if (!lua_isnumber(L, 1))
        {
            sys_err("QUEST : wrong argument");
            return 0;
        }
        int64_t iAmount = (int64_t)lua_tonumber(L, 1);
        if (iAmount <= 0)
        {
            sys_err("QUEST : gold amount less then zero");
            return 0;
        }
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* gold = ECS_TryGet<ecs::GoldAmount>(e))
        {
            gold->amount += iAmount;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        if (chEntity != entt::null && g_registry.valid(chEntity))
        {
            DBManager::instance().SendMoneyLog(
                MONEY_LOG_QUEST, ecs::PlayerRuntime::GetPlayerID(chEntity), iAmount);
            ecs::PointSystem::Change(chEntity, POINT_GOLD, iAmount, true);
        }
        return 0;
    }

    ALUA(pc_warp_to_guild_war_observer_position)
    {
        // migrated from CHARACTER::WarpSet
        // DUAL-PATH: ECS component update + legacy warp packet
        luaL_checknumber(L, 1);
        luaL_checknumber(L, 2);
        uint32_t gid1 = static_cast<uint32_t>(lua_tonumber(L, 1));
        uint32_t gid2 = static_cast<uint32_t>(lua_tonumber(L, 2));
        CGuild* g1 = CGuildManager::instance().FindGuild(gid1);
        CGuild* g2 = CGuildManager::instance().FindGuild(gid2);
        if (!g1 || !g2)
            luaL_error(L, "no such guild with id %d %d", gid1, gid2);
        PIXEL_POSITION pos;
        uint32_t dwMapIndex = g1->GetGuildWarMapIndex(gid2);
        if (!CWarMapManager::instance().GetStartPosition(dwMapIndex, 2, pos))
        {
            luaL_error(L, "not under warp guild war between guild %d %d", gid1, gid2);
            return 0;
        }
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

        if (chEntity == entt::null)
            return 0;
        if (ecs::PlayerRuntime::IsHack(chEntity))
            return 0;
        ecs::QuestSystem::SetFlag(chEntity, "war.is_war_member", 0);
        ecs::MovementSystem::SaveExitLocation(chEntity);
        if (auto* warpPos = ECS_TryGet<ecs::WarpPosition>(e))
        {
            warpPos->x = pos.x;
            warpPos->y = pos.y;
            warpPos->mapIndex = dwMapIndex;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        ecs::MovementSystem::WarpSet(chEntity, pos.x, pos.y, dwMapIndex);
        return 0;
    }

#ifdef ENABLE_DICE_SYSTEM

#endif

	ALUA(pc_get_current_map_index)
	{
		// migrated from CHARACTER::GetMapIndex
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* mapIndex = ECS_TryGet<ecs::MapIndex>(e))
		{
			lua_pushnumber(L, mapIndex->value);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PlayerRuntime::GetMapIndex(chEntity));
		return 1;
	}

	ALUA(pc_get_x)
	{
		// migrated from CHARACTER::GetX
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* pos = ECS_TryGet<ecs::Position>(e))
		{
			lua_pushnumber(L, pos->x / 100);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PlayerRuntime::GetX(chEntity) / 100);
		return 1;
	}

	ALUA(pc_get_y)
	{
		// migrated from CHARACTER::GetY
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* pos = ECS_TryGet<ecs::Position>(e))
		{
			lua_pushnumber(L, pos->y / 100);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PlayerRuntime::GetY(chEntity) / 100);
		return 1;
	}

	ALUA(pc_get_local_x)
	{
		// migrated from CHARACTER::GetX
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* pos = ECS_TryGet<ecs::Position>(e))
		{
			const auto* mapIndex = ECS_TryGet<ecs::MapIndex>(e);
			LPSECTREE_MAP pMap = mapIndex ? SECTREE_MANAGER::instance().GetMap(mapIndex->value) : nullptr;
			lua_pushnumber(L, pMap ? ((pos->x - pMap->m_setting.iBaseX) / 100) : (pos->x / 100));
			return 1;
		}
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(ecs::PlayerRuntime::GetMapIndex(character));
		const int32_t x = ecs::PlayerRuntime::GetX(character);
		lua_pushnumber(L, pMap ? ((x - pMap->m_setting.iBaseX) / 100) : (x / 100));
		return 1;
	}

	ALUA(pc_get_local_y)
	{
		// migrated from CHARACTER::GetY
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* pos = ECS_TryGet<ecs::Position>(e))
		{
			const auto* mapIndex = ECS_TryGet<ecs::MapIndex>(e);
			LPSECTREE_MAP pMap = mapIndex ? SECTREE_MANAGER::instance().GetMap(mapIndex->value) : nullptr;
			lua_pushnumber(L, pMap ? ((pos->y - pMap->m_setting.iBaseY) / 100) : (pos->y / 100));
			return 1;
		}
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(ecs::PlayerRuntime::GetMapIndex(character));
		const int32_t y = ecs::PlayerRuntime::GetY(character);
		lua_pushnumber(L, pMap ? ((y - pMap->m_setting.iBaseY) / 100) : (y / 100));
		return 1;
	}

	ALUA(pc_get_leadership)
	{
		// migrated from CHARACTER::GetLeadershipSkillLevel
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		lua_pushnumber(L, SkillSystem::GetSkillLevel(e, SKILL_LEADERSHIP));
		return 1;
	}

	ALUA(pc_reset_point)
	{
        // migrated from CHARACTER::ResetPoint
        // TODO Phase 8: CharacterPoints decomposition
        // DUAL-PATH: legacy only during migration window
		ecs::PointSystem::ResetAllPoints(CQuestManager::instance().GetCurrentPCEntity(), ecs::PointSystem::GetLevel(CQuestManager::instance().GetCurrentCharacter()));
		return 0;
	}

	ALUA(pc_get_playtime)
	{
		// migrated from CHARACTER::GetRealPoint
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
		{
			lua_pushnumber(L, points->base.points[POINT_PLAYTIME]);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PointSystem::GetReal(chEntity, POINT_PLAYTIME));
		return 1;
	}

	ALUA(pc_get_vid)
	{
		// migrated from CHARACTER::GetVID
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* vid = ECS_TryGet<ecs::VIDComponent>(e))
		{
			lua_pushnumber(L, vid->value);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PlayerRuntime::GetPacketVID(chEntity));
		return 1;
	}

	ALUA(pc_get_name)
	{
		// migrated from CHARACTER::GetName
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* name = ECS_TryGet<ecs::PlayerName>(e))
		{
			lua_pushstring(L, name->value.c_str());
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushstring(L, ecs::PlayerRuntime::GetName(chEntity).data());
		return 1;
	}
	ALUA(pc_get_next_exp)
	{
		// migrated from CHARACTER::GetNextExp
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* exp = ECS_TryGet<ecs::Experience>(e))
		{
			lua_pushnumber(L, exp->next);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	ALUA(pc_get_exp)
	{
		// migrated from CHARACTER::GetExp
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* exp = ECS_TryGet<ecs::Experience>(e))
		{
			lua_pushnumber(L, exp->current);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	ALUA(pc_get_race)
	{
		// migrated from CHARACTER::GetRaceNum
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* race = ECS_TryGet<ecs::RaceComponent>(e))
		{
			lua_pushnumber(L, race->value);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PlayerRuntime::GetRaceNum(chEntity));
		return 1;
	}

    ALUA(pc_change_sex)
    {
        namespace item = auriga::item;
        const auto character = CQuestManager::instance().GetPCEntity(L);
        const auto* inventory = g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
        if (!inventory || g_registry.all_of<ecs::TypedItemBusy>(character)) {
            lua_pushinteger(L, 0);
            return 1;
        }
        bool body = false, hair = false;
        for (const auto& instance : inventory->state.items) {
            const auto& placement = instance.placement;
            if (instance.destroyed || instance.owner != inventory->state.fence.owner || !placement ||
                placement->kind != item::ItemStorageKind::Equipment) continue;
            body |= item::FitsEquipmentSlot(item::EquipmentSlot::CostumeBody, placement->anchor);
            hair |= item::FitsEquipmentSlot(item::EquipmentSlot::CostumeHair, placement->anchor);
        }
        if (body || hair)
            lua_pushinteger(L, body ? 2 : 3);
        else
            lua_pushinteger(L, ecs::PlayerRuntime::ChangeSex(character) ? 1 : 0);
        return 1;
    }

	ALUA(pc_get_job)
	{
		// migrated from CHARACTER::GetJob
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
		{
			lua_pushnumber(L, points->base.job);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	ALUA(pc_get_max_sp)
	{
		// migrated from CHARACTER::GetMaxSP
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* mana = ECS_TryGet<ecs::Mana>(e))
		{
			lua_pushnumber(L, mana->max);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	ALUA(pc_get_sp)
	{
		// migrated from CHARACTER::GetSP
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* mana = ECS_TryGet<ecs::Mana>(e))
		{
			lua_pushnumber(L, mana->current);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	ALUA(pc_change_sp)
	{
		// migrated from CHARACTER::PointChange(POINT_SP, ...)
		// DUAL-PATH: ECS update + legacy call during migration window
		if (!lua_isnumber(L, 1))
		{
			sys_err("invalid argument");
			lua_pushboolean(L, 0);
			return 1;
		}
		const int64_t val = static_cast<int64_t>(lua_tonumber(L, 1));
		if (val == 0)
		{
			lua_pushboolean(L, 0);
			return 1;
		}
		const entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e == entt::null || !g_registry.valid(e))
		{
			lua_pushboolean(L, 0);
			return 1;
		}
		if (val > 0)
		{
			ecs::PointSystem::Change(e, POINT_SP, val);
		}
		else if (val < 0)
		{
			if (ecs::PointSystem::Get(e, POINT_SP) < -val)
			{
				lua_pushboolean(L, 0);
				return 1;
			}
			ecs::PointSystem::Change(e, POINT_SP, val);
		}
		lua_pushboolean(L, 1);
		return 1;
	}

	ALUA(pc_get_max_hp)
	{
		// migrated from CHARACTER::GetMaxHP
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* health = ECS_TryGet<ecs::Health>(e))
		{
			lua_pushnumber(L, health->max);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	ALUA(pc_get_hp)
	{
		// migrated from CHARACTER::GetHP
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* health = ECS_TryGet<ecs::Health>(e))
		{
			lua_pushnumber(L, health->current);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	ALUA(pc_get_level)
	{
		// migrated from CHARACTER::GetLevel
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* level = ECS_TryGet<ecs::LevelComponent>(e))
		{
			lua_pushnumber(L, level->value);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

    ALUA(pc_set_level)
    {
        // migrated from CHARACTER::SetLevel
        // DUAL-PATH: ECS update + legacy call during migration window
        if (!lua_isnumber(L, 1))
        {
            sys_err("invalid argument");
            return 0;
        }
        const int newLevel = static_cast<int>(lua_tonumber(L, 1));
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (chEntity == entt::null || !g_registry.valid(chEntity))
            return 0;
		LOG_INFO("QUEST [LEVEL] {} jumping to level {}",
			ecs::PlayerRuntime::GetName(chEntity), newLevel);
        PC* pPC = CQuestManager::instance().GetCurrentPC();
		LogManager::instance().QuestRewardLog(pPC->GetCurrentQuestName().c_str(),
			ecs::PlayerRuntime::GetPlayerID(chEntity), ecs::PointSystem::GetLevel(chEntity),
			newLevel, 0);
		ecs::PointSystem::SetLevelFromQuest(chEntity, newLevel);
        return 0;
    }

	ALUA(pc_get_money)
    {
        const auto e=CQuestManager::instance().GetPCEntity(L);
        if(const auto* typed=g_registry.valid(e)?g_registry.try_get<ecs::TypedItems>(e):nullptr){for(const auto& w:typed->state.wallets)if(w.owner==typed->state.fence.owner&&w.currency==auriga::item::WalletCurrency::Gold){lua_pushinteger(L,static_cast<lua_Integer>(w.balance));return 1;}return luaL_error(L,"Native gold wallet unavailable");}
        lua_pushinteger(L,ecs::PointSystem::GetGold(e));return 1;
    }

	ALUA(pc_get_real_alignment)
	{
		// migrated from CHARACTER::GetRealAlignment
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* combat = ECS_TryGet<ecs::CombatStats>(e))
		{
			lua_pushnumber(L, static_cast<int32_t>(combat->realAlignment) / 10);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}
	ALUA(pc_get_alignment)
	{
		// migrated from CHARACTER::GetAlignment
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* combat = ECS_TryGet<ecs::CombatStats>(e))
		{
			lua_pushnumber(L, static_cast<int32_t>(combat->alignment) / 10);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

    ALUA(pc_change_alignment)
    {
        if (!lua_isnumber(L, 1)) return 0;
        const double delta = lua_tonumber(L, 1) * 10.0;
        if (!std::isfinite(delta)) return 0;
        const int64_t alignment = static_cast<int64_t>(std::clamp(delta,
            -static_cast<double>(CombatSystem::MAX_ALIGNMENT), static_cast<double>(CombatSystem::MAX_ALIGNMENT)));
        CombatSystem::UpdateAlignment(CQuestManager::instance().GetPCEntity(L), alignment);
        return 0;
    }

	ALUA(pc_change_money)
	{
        const auto nativeEntity=CQuestManager::instance().GetPCEntity(L);
        if(g_registry.valid(nativeEntity)&&g_registry.all_of<ecs::TypedItems>(nativeEntity)){
            int valid=0;const auto delta=lua_tointegerx(L,-1,&valid);if(!valid||delta==INT64_MIN)return luaL_error(L,"Native gold change requires an exact signed integer");if(!delta)return 0;
            auto& q=CQuestManager::instance();const auto* state=q.GetCurrentState();if(!lua_isyieldable(L)||q.IsInOtherPCBlock()||!state||state->co!=L)return luaL_error(L,"Native gold change requires the authenticated player's quest coroutine");
            const auto pid=ecs::PlayerRuntime::GetPlayerID(nativeEntity);
            if(!RequestTypedQuestGoldChange(nativeEntity,delta,[pid,co=L](bool ok){CQuestManager::instance().ResumeTypedItems(pid,co,ok);}))return luaL_error(L,"Native gold transaction unavailable or insufficient funds");
            lua_settop(L,0);lua_pushliteral(L,"typed_items");return lua_yieldk(L,1,0,ResumeNativeQuestMoney);
        }

		// migrated from CHARACTER::PointChange(POINT_GOLD, ...)
		// DUAL-PATH: ECS update + legacy call during migration window
		const int64_t gold = static_cast<int64_t>(lua_tonumber(L, -1));
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const int64_t currentGold = ecs::PointSystem::GetGold(character);
		if (gold + currentGold < 0)
			sys_err("QUEST wrong ChangeGold {} (now {})", gold, currentGold);
		else if (character != entt::null && g_registry.valid(character))
		{
			DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST, ecs::PlayerRuntime::GetPlayerID(character), gold);
			ecs::PointSystem::Change(character, POINT_GOLD, gold, true);
		}
		return 0;
	}

    ALUA(pc_set_another_quest_flag)
    {
        // migrated from CHARACTER quest flag system
        // TODO Phase 8: dedicated QuestFlags component
        // DUAL-PATH: legacy only during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        (void)e;
        if (!lua_isstring(L, 1) || !lua_isstring(L, 2) || !lua_isnumber(L, 3))
        {
            sys_err("QUEST wrong set flag");
            return 0;
        }
        const char * sz = lua_tostring(L, 1);
        const char * sz2 = lua_tostring(L, 2);
        CQuestManager & q = CQuestManager::Instance();
        PC * pPC = q.GetCurrentPC();
        pPC->SetFlag(string(sz)+"."+sz2, LuaIntegerValue(L, 3));
        return 0;
    }

	ALUA(pc_get_another_quest_flag)
	{
		if (!lua_isstring(L,1) || !lua_isstring(L,2))
		{
			sys_err("QUEST wrong get flag");
			return 0;
		}
		else
		{
			const char* sz = lua_tostring(L,1);
			const char* sz2 = lua_tostring(L,2);
			CQuestManager& q = CQuestManager::Instance();
			PC* pPC = q.GetCurrentPC();
			if (!pPC)
			{
				return 0;
			}
			lua_pushinteger(L,pPC->GetFlag(string(sz)+"."+sz2));
			return 1;
		}
	}

	ALUA(pc_get_flag)
	{
		if (!lua_isstring(L,-1))
		{
			sys_err("QUEST wrong get flag");
			return 0;
		}
		else
		{
			const char* sz = lua_tostring(L,-1);
			CQuestManager& q = CQuestManager::Instance();
			PC* pPC = q.GetCurrentPC();
			lua_pushinteger(L,pPC->GetFlag(sz));
			return 1;
		}
	}

	ALUA(pc_get_quest_flag)
	{
		if (!lua_isstring(L,-1))
		{
			sys_err("QUEST wrong get flag");
			return 0;
		}
		else
		{
			const char* sz = lua_tostring(L,-1);
			CQuestManager& q = CQuestManager::Instance();
			PC* pPC = q.GetCurrentPC();
			lua_pushinteger(L,pPC->GetFlag(pPC->GetCurrentQuestName() + "."+sz));
			if ( test_server )
				LOG_INFO("GetQF ( {} . {} )", pPC->GetCurrentQuestName().c_str(), sz);
		}
		return 1;
	}

    ALUA(pc_set_flag)
    {
        // migrated from CHARACTER quest flag system
        // TODO Phase 8: dedicated QuestFlags component
        // DUAL-PATH: legacy only during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        (void)e;
        if (!lua_isstring(L,1) || !lua_isnumber(L,2))
        {
            sys_err("QUEST wrong set flag");
        }
        else
        {
            const char* sz = lua_tostring(L,1);
            CQuestManager& q = CQuestManager::Instance();
            PC* pPC = q.GetCurrentPC();
            pPC->SetFlag(sz, LuaIntegerValue(L, 2));
        }
        return 0;
    }

    ALUA(pc_set_quest_flag)
    {
        // migrated from CHARACTER quest flag system
        // TODO Phase 8: dedicated QuestFlags component
        // DUAL-PATH: legacy only during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        (void)e;
        if (!lua_isstring(L,1) || !lua_isnumber(L,2))
        {
            sys_err("QUEST wrong set flag");
        }
        else
        {
            const char* sz = lua_tostring(L,1);
            CQuestManager& q = CQuestManager::Instance();
            PC* pPC = q.GetCurrentPC();
            pPC->SetFlag(pPC->GetCurrentQuestName()+"."+sz, LuaIntegerValue(L, 2));
        }
        return 0;
    }

    ALUA(pc_del_quest_flag)
    {
        // migrated from CHARACTER quest flag system
        // TODO Phase 8: dedicated QuestFlags component
        // DUAL-PATH: legacy only during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        (void)e;
        if (!lua_isstring(L, 1))
        {
            sys_err("argument error");
            return 0;
        }
        const char * sz = lua_tostring(L, 1);
        PC * pPC = CQuestManager::instance().GetCurrentPC();
        pPC->DeleteFlag(pPC->GetCurrentQuestName()+"."+sz);
        return 0;
    }

    ALUA(pc_give_exp2)
    {
        // migrated from CHARACTER::PointChange(POINT_EXP, ...)
        // DUAL-PATH: ECS update + legacy call during migration window
        CQuestManager& q = CQuestManager::instance();
		const entt::entity character = q.GetCurrentPCEntity();
		if (!lua_isnumber(L,1) || character == entt::null || !g_registry.valid(character))
            return 0;
		LOG_INFO("QUEST [REWARD] {} give exp2 {}", ecs::PlayerRuntime::GetName(character).data(), (int)rint(lua_tonumber(L,1)));
        uint32_t exp = (uint32_t)rint(lua_tonumber(L,1));
        PC* pPC = CQuestManager::instance().GetCurrentPC();
		LogManager::instance().QuestRewardLog(pPC->GetCurrentQuestName().c_str(), ecs::PlayerRuntime::GetPlayerID(character), ecs::PointSystem::GetLevel(character), exp, 0);
		ecs::PointSystem::Change(character, POINT_EXP, exp);
        return 0;
    }

    ALUA(pc_give_exp)
    {
        // migrated from CHARACTER::PointChange(POINT_EXP, ...)
        // DUAL-PATH: ECS update + legacy call during migration window
        if (!lua_isstring(L,1) || !lua_isnumber(L,2))
            return 0;
        CQuestManager& q = CQuestManager::instance();
		const entt::entity character = q.GetCurrentPCEntity();
		if (character == entt::null || !g_registry.valid(character))
            return 0;
		LOG_INFO("QUEST [REWARD] {} give exp {} {}", ecs::PlayerRuntime::GetName(character).data(), lua_tostring(L,1), (int)rint(lua_tonumber(L,2)));
        uint32_t exp = (uint32_t)rint(lua_tonumber(L,2));
        PC* pPC = CQuestManager::instance().GetCurrentPC();
		LogManager::instance().QuestRewardLog(pPC->GetCurrentQuestName().c_str(), ecs::PlayerRuntime::GetPlayerID(character), ecs::PointSystem::GetLevel(character), exp, 0);
        pPC->GiveExp(lua_tostring(L,1), exp);
        return 0;
    }

    ALUA(pc_give_exp_perc)
    {
        // migrated from CHARACTER::PointChange(POINT_EXP, ...)
        // DUAL-PATH: ECS update + legacy call during migration window
        CQuestManager & q = CQuestManager::instance();
		const entt::entity character = q.GetCurrentPCEntity();
		if (character == entt::null || !g_registry.valid(character) || !lua_isstring(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
            return 0;
        int lev = (int)rint(lua_tonumber(L,2));
        double proc = (lua_tonumber(L,3));
		LOG_INFO("QUEST [REWARD] {} give exp {} lev {} percent {:g}%", ecs::PlayerRuntime::GetName(character).data(), lua_tostring(L, 1), lev, proc);
        uint32_t exp = (uint32_t)((exp_table[MINMAX(0, lev, PLAYER_MAX_LEVEL_CONST)] * proc) / 100);
        PC * pPC = CQuestManager::instance().GetCurrentPC();
		LogManager::instance().QuestRewardLog(pPC->GetCurrentQuestName().c_str(), ecs::PlayerRuntime::GetPlayerID(character), ecs::PointSystem::GetLevel(character), exp, 0);
        pPC->GiveExp(lua_tostring(L, 1), exp);
        return 0;
    }

	ALUA(pc_get_empire)
	{
		// migrated from CHARACTER::GetEmpire
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* empire = ECS_TryGet<ecs::EmpireComponent>(e))
		{
			lua_pushnumber(L, empire->value);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PlayerRuntime::GetEmpire(chEntity));
		return 1;
	}

	ALUA(pc_get_part)
	{
		// migrated from CHARACTER::GetPart
		if (!lua_isnumber(L, 1))
		{
			lua_pushnumber(L, 0);
			return 1;
		}
		const int part_idx = static_cast<int>(lua_tonumber(L, 1));
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		const auto* appearance = ECS_TryGet<ecs::AppearancePartsComponent>(character);
		lua_pushnumber(L, appearance && part_idx >= 0 && part_idx < PART_MAX_NUM ? appearance->parts[part_idx] : 0);
		return 1;
	}

    ALUA(pc_set_part)
    {
        // migrated from CHARACTER::SetPart
        CQuestManager& q = CQuestManager::instance();
		const entt::entity character = q.GetCurrentPCEntity();
		if (character == entt::null || !g_registry.valid(character) || !lua_isnumber(L,1) || !lua_isnumber(L,2))
        {
            return 0;
        }
        int part_idx = (int)lua_tonumber(L, 1);
        int part_value = (int)lua_tonumber(L, 2);
		if (part_idx < 0 || part_idx >= PART_MAX_NUM)
			return 0;
		auto& appearance = g_registry.get_or_emplace<ecs::AppearancePartsComponent>(character);
		appearance.parts[part_idx] = static_cast<uint16_t>(part_value);
		g_registry.emplace_or_replace<ecs::DirtyTag>(character);
		NetworkSyncSystem::UpdatePacket(character);
        return 0;
    }

	ALUA(pc_get_skillgroup)
	{
		// migrated from CHARACTER::GetSkillGroup
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
		{
			lua_pushnumber(L, points->base.skill_group);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

    ALUA(pc_set_skillgroup)
    {
        // migrated from CHARACTER::SetSkillGroup
        // DUAL-PATH: ECS update + legacy call during migration window
        if (!lua_isnumber(L, 1))
        {
            sys_err("QUEST wrong skillgroup number");
            return 0;
        }
        CQuestManager & q = CQuestManager::Instance();
        entt::entity e = q.GetPCEntity(L);
        const entt::entity chEntity = q.GetCurrentPCEntity();

#ifdef ENABLE_BUG_FIXES
        AffectSystem::RemoveGoodAffects(chEntity);
#endif
        SkillSystem::SetSkillGroup(chEntity, (uint8_t) rint(lua_tonumber(L, 1)));
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
        {
            points->base.skill_group = SkillSystem::GetSkillGroup(chEntity);
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        return 0;
    }

	ALUA(pc_is_polymorphed)
	{
		// migrated from CHARACTER::IsPolymorphed
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
		{
			lua_pushboolean(L, sf->isPolymorph ? 1 : 0);
			return 1;
		}
		lua_pushboolean(L, 0);
		return 1;
	}

    ALUA(pc_remove_polymorph)
    {
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		AffectSystem::RemoveAffect(chEntity, AFFECT_POLYMORPH);
		AffectSystem::SetPolymorph(chEntity, 0);
        return 0;
    }

    ALUA(pc_polymorph)
    {
		const entt::entity e = CQuestManager::instance().GetCurrentPCEntity();
        uint32_t dwVnum = (uint32_t) lua_tonumber(L, 1);
        int iDuration = (int) lua_tonumber(L, 2);
		if (e != entt::null && g_registry.valid(e))
			AffectSystem::AddAffect(e, AFFECT_POLYMORPH, POINT_POLYMORPH, dwVnum, AFF_POLYMORPH, iDuration, 0, true);
        return 0;
    }

    ALUA(pc_is_mount)
    {
        const auto rider = CQuestManager::instance().GetPCEntity(L);
        const auto* flags = ECS_TryGet<ecs::StatusFlags>(rider);
        lua_pushboolean(L, (flags && flags->isMountActive) || MountSystem::GetMountVnum(rider) != 0);
        return 1;
    }

    ALUA(pc_mount)
    {
        // migrated from CHARACTER::MountVnum
        // DUAL-PATH: ECS MountState + legacy call
        if (!lua_isnumber(L, 1))
            return 0;
        int length = 60;
        if (lua_isnumber(L, 2))
            length = (int)lua_tonumber(L, 2);
        uint32_t mount_vnum = (uint32_t)lua_tonumber(L, 1);
        if (length < 0)
            length = 60;
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

#ifdef ENABLE_PVP_ADVANCED
        if (ecs::PlayerRuntime::GetDuelOption(chEntity, "BlockRide"))
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 516, "");
#endif
            lua_pushnumber(L, 0);
            return 1;
        }
#endif
        AffectSystem::RemoveAffect(chEntity, AFFECT_MOUNT);
        AffectSystem::RemoveAffect(chEntity, AFFECT_MOUNT_BONUS);
#ifdef ENABLE_NEWSTUFF
        if (g_NoMountAtGuildWar && ecs::SocialSystem::GetWarMap(chEntity))
        {
            if (MountSystem::IsRiding(chEntity))
                MountSystem::StopRiding(chEntity);
            return 0;
        }
#endif
        if (MountSystem::IsSummoned(chEntity))
            MountSystem::SummonHorse(chEntity, false);
        if (mount_vnum)
        {
            AffectSystem::AddAffect(chEntity, AFFECT_MOUNT, POINT_MOUNT, mount_vnum, AFF_NONE, length, 0, true);
            switch (mount_vnum)
            {
            case 20201:
            case 20202:
            case 20203:
            case 20204:
            case 20213:
            case 20216:
                AffectSystem::AddAffect(chEntity, AFFECT_MOUNT, POINT_MOV_SPEED, 30, AFF_NONE, length, 0, true, true);
                break;
            case 20205:
            case 20206:
            case 20207:
            case 20208:
            case 20214:
            case 20217:
                AffectSystem::AddAffect(chEntity, AFFECT_MOUNT, POINT_MOV_SPEED, 40, AFF_NONE, length, 0, true, true);
                break;
            case 20209:
            case 20210:
            case 20211:
            case 20212:
            case 20215:
            case 20218:
                AffectSystem::AddAffect(chEntity, AFFECT_MOUNT, POINT_MOV_SPEED, 50, AFF_NONE, length, 0, true, true);
                break;
            }
        }
        if (auto* ms = ECS_TryGet<ecs::MountState>(e))
        {
            ms->mountVnum = mount_vnum;
            ms->mountTime = get_dword_time();
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
        {
            // The rider bit. isMount belongs to the creature - see
            // ecs::StatusFlags - and setting it here made the native
            // character-insert builder treat riding players as mounts.
            sf->isMountActive = (mount_vnum != 0);
        }
        return 0;
    }

	ALUA(pc_mount_bonus)
	{
        // migrated from CHARACTER::AddAffect
        // DUAL-PATH: legacy only during migration window
		uint8_t applyOn = static_cast<uint8_t>(lua_tonumber(L, 1));
		int32_t value = static_cast<int32_t>(lua_tonumber(L, 2));
		int32_t duration = static_cast<int32_t>(lua_tonumber(L, 3));

		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		if (!MountSystem::GetMountVnum(character))
			return 0;
		AffectSystem::RemoveAffect(character, AFFECT_MOUNT_BONUS);
		AffectSystem::AddAffect(character, AFFECT_MOUNT_BONUS, aApplyInfo[applyOn].bPointType, value, AFF_NONE, duration, 0, false);

		return 0;
	}

    ALUA(pc_unmount)
    {
        // migrated from CHARACTER::StopRiding
        // DUAL-PATH: ECS MountState clear + legacy call
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* ms = ECS_TryGet<ecs::MountState>(e))
        {
            ms->mountVnum = 0;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
        {
            sf->isMountActive = false;
        }
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

            AffectSystem::RemoveAffect(chEntity, AFFECT_MOUNT);
            AffectSystem::RemoveAffect(chEntity, AFFECT_MOUNT_BONUS);
        if (MountSystem::IsRiding(chEntity))
            MountSystem::StopRiding(chEntity);
        return 0;
    }

	ALUA(pc_get_horse_level)
	{
        // migrated from CHARACTER::GetHorseLevel
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		lua_pushnumber(L, MountSystem::GetHorseLevel(chEntity));
		return 1;
	}

	ALUA(pc_get_horse_hp)
	{
        // migrated from CHARACTER::GetHorseHealth
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (MountSystem::GetHorseLevel(chEntity))
			lua_pushnumber(L, MountSystem::GetHorseHealth(chEntity));
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(pc_get_horse_stamina)
	{
        // migrated from CHARACTER::GetHorseStamina
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (MountSystem::GetHorseLevel(chEntity))
			lua_pushnumber(L, MountSystem::GetHorseStamina(chEntity));
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(pc_is_horse_alive)
	{
		// migrated from CHARACTER::GetHorseHealth
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* mount = ECS_TryGet<ecs::MountState>(e))
		{
			lua_pushboolean(L, (mount->sendHorseLevel > 0 && mount->sendHorseHealthGrade > 0) ? 1 : 0);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		lua_pushboolean(L, (MountSystem::GetHorseLevel(chEntity) > 0 && MountSystem::GetHorseHealth(chEntity) > 0) ? 1 : 0);
		return 1;
	}

	ALUA(pc_revive_horse)
	{
        // migrated from CHARACTER::ReviveHorse
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		MountSystem::ReviveHorse(chEntity);
		return 0;
	}

	ALUA(pc_get_war_map)
	{
		// migrated from CHARACTER::GetWarMap
		const entt::entity e = CQuestManager::instance().GetPCEntity(L);
		CWarMap* pWarMap = ecs::SocialSystem::GetWarMap(e);
		lua_pushnumber(L, pWarMap ? pWarMap->GetMapIndex() : 0);
		return 1;
	}

    ALUA(pc_get_skill_level)
    {
        // migrated from CHARACTER::GetSkillLevel
        if (!lua_isnumber(L, 1))
        {
            sys_err("invalid argument");
            lua_pushnumber(L, 0);
            return 1;
        }
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        uint32_t dwVnum = (uint32_t) lua_tonumber(L, 1);
        if (const auto* sl = ECS_TryGet<ecs::SkillLevels>(e))
        {
            if (sl->loaded && dwVnum < SKILL_MAX_NUM)
            {
                lua_pushnumber(L, sl->levels[dwVnum].bLevel);
                return 1;
            }
        }
		lua_pushnumber(L, 0);
        return 1;
    }

	ALUA(pc_aggregate_monster)
	{
		CombatSystem::AggregateMonster(CQuestManager::instance().GetPCEntity(L));
		return 0;
	}

	ALUA(pc_forget_my_attacker)
	{
		CombatSystem::ForgetMyAttacker(CQuestManager::instance().GetPCEntity(L));
		return 0;
	}

	ALUA(pc_attract_ranger)
	{
		CombatSystem::AttractRanger(CQuestManager::instance().GetPCEntity(L));
		return 0;
	}

	ALUA(pc_select_pid)
	{
		const uint32_t pid = static_cast<uint32_t>(lua_tonumber(L, 1));
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity selected = ecs::PlayerRuntime::FindByPlayerID(pid);
		if (selected != entt::null)
		{
			CQuestManager::instance().GetPC(pid);
			lua_pushnumber(L, ecs::PlayerRuntime::GetPlayerID(chEntity));
		}
		else
		{
			lua_pushnumber(L, 0);
		}

		return 1;
	}

	ALUA(pc_select_vid)
	{
		const uint32_t vid = static_cast<uint32_t>(lua_tonumber(L, 1));
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity selected = CVIDRegistry::Instance().Find(vid);
		const uint32_t selectedPID = ecs::PlayerRuntime::GetPlayerID(selected);
		if (selectedPID != 0)
		{
			CQuestManager::instance().GetPC(selectedPID);
			lua_pushnumber(L, ecs::PlayerRuntime::GetPacketVID(chEntity));
		}
		else
		{
			lua_pushnumber(L, 0);
		}

		return 1;
	}

	ALUA(pc_get_sex)
	{
		lua_pushnumber(L, ecs::PlayerRuntime::GetSex(CQuestManager::instance().GetCurrentPCEntity()));
		return 1;
	}

	ALUA(pc_is_engaged)
	{
		// migrated from CHARACTER::GetPlayerID
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		// MarriageState::partner is who is standing next to you, not who you
		// are engaged to, and the branch that read it shadowed this one for
		// everybody, because the component is emplaced on every PC.
		lua_pushboolean(L, marriage::CManager::instance().IsEngaged(ecs::PlayerRuntime::GetPlayerID(e)) ? 1 : 0);
		return 1;
	}

	ALUA(pc_is_married)
	{
		// migrated from CHARACTER::GetPlayerID
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		// Likewise weddingMap is the map you are standing on during the
		// ceremony, not a marriage.
		lua_pushboolean(L, marriage::CManager::instance().IsMarried(ecs::PlayerRuntime::GetPlayerID(e)) ? 1 : 0);
		return 1;
	}

	ALUA(pc_is_engaged_or_married)
	{
		// migrated from CHARACTER::GetPlayerID
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		lua_pushboolean(L, marriage::CManager::instance().IsEngagedOrMarried(ecs::PlayerRuntime::GetPlayerID(e)) ? 1 : 0);
		return 1;
	}

	ALUA(pc_is_gm)
	{
		// migrated from CHARACTER::GetGMLevel
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
		{
			lua_pushboolean(L, sf->isGM ? 1 : 0);
			return 1;
		}
		if (const auto* gm = ECS_TryGet<ecs::GMLevel>(e))
		{
			lua_pushboolean(L, (gm->level >= GM_HIGH_WIZARD) ? 1 : 0);
			return 1;
		}
		lua_pushboolean(L, 0);
		return 1;
	}

	ALUA(pc_get_gm_level)
	{
		// migrated from CHARACTER::GetGMLevel
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* gm = ECS_TryGet<ecs::GMLevel>(e))
		{
			lua_pushnumber(L, gm->level);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	ALUA(pc_mining)
	{
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity load = CQuestManager::instance().GetCurrentNPCEntity();
		ActivitySystem::StartMining(character, load);
		return 0;
	}

	ALUA(pc_clear_skill)
	{
		SkillSystem::ClearSkill(CQuestManager::instance().GetCurrentPCEntity());

		return 0;
	}

	ALUA(pc_clear_sub_skill)
	{
		SkillSystem::ClearSubSkill(CQuestManager::instance().GetCurrentPCEntity());

		return 0;
	}

    ALUA(pc_set_skill_point)
    {
        if (!lua_isnumber(L, 1))
            return 0;
		const entt::entity e = CQuestManager::instance().GetCurrentPCEntity();
		if (e == entt::null || !g_registry.valid(e))
            return 0;
        int newPoint = (int) lua_tonumber(L, 1);
		ecs::PointSystem::SetReal(e, POINT_SKILL, newPoint);
		ecs::PointSystem::Set(e, POINT_SKILL, ecs::PointSystem::GetReal(e, POINT_SKILL));
		ecs::PointSystem::Change(e, POINT_SKILL, 0);
		ecs::PointSystem::Compute(e);
		NetworkSyncSystem::PointsPacket(e);
        return 0;
    }

	// RESET_ONE_SKILL
	ALUA(pc_clear_one_skill)
	{
		int vnum = (int)lua_tonumber(L, 1);
		LOG_INFO("{} skill clear", vnum);

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (chEntity == entt::null || !g_registry.valid(chEntity))
		{
			LOG_INFO("skill clear fail");
			lua_pushnumber(L, 0);
			return 1;
		}

		LOG_INFO("{} skill clear", vnum);

		SkillSystem::ResetOneSkill(chEntity, vnum);

		return 0;
	}
	// END_RESET_ONE_SKILL

	ALUA(pc_is_clear_skill_group)
	{
		// migrated from CHARACTER::GetQuestFlag
		// TODO Phase 8: dedicated QuestFlag component
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		(void)e;
		lua_pushboolean(L, ecs::QuestSystem::GetFlag(e, "skill_group_clear.clear") == 1 ? 1 : 0);
		return 1;
	}

    ALUA(pc_save_exit_location)
    {
        // migrated from CHARACTER::SaveExitLocation
        // DUAL-PATH: ECS update + legacy call during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e == entt::null || !g_registry.valid(e))
            return 0;
        if (auto* exitPos = ECS_TryGet<ecs::ExitPosition>(e))
        {
            if (const auto* position = ECS_TryGet<ecs::Position>(e))
            {
                exitPos->x = position->x;
                exitPos->y = position->y;
            }
            else
            {
				exitPos->x = ecs::PlayerRuntime::GetX(e);
				exitPos->y = ecs::PlayerRuntime::GetY(e);
            }
            if (const auto* mapIndex = ECS_TryGet<ecs::MapIndex>(e))
                exitPos->mapIndex = mapIndex->value;
            else
				exitPos->mapIndex = ecs::PlayerRuntime::GetMapIndex(e);
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
		return 0;
    }

	ALUA(pc_teleport)
	{
		// migrated from CHARACTER::WarpSet
		// DUAL-PATH: ECS component update + legacy warp packet
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e == entt::null || !g_registry.valid(e))
		{
			lua_pushnumber(L, 0 );
			return 1;
		}
		int x=0,y=0;
		if ( lua_isnumber(L, 1) )
		{
			const int TOWN_NUM = 10;
			struct warp_by_town_name
			{
				const char* name;
				uint32_t x;
				uint32_t y;
			} ws[TOWN_NUM] =
			{
				{"��������",	4743,	9548},
				{"������",		3235,	9086},
				{"�ھ���",		3531,	8829},
				{"��������",	638,	1664},
				{"�·��",		1745,	1909},
				{"������",		1455,	2400},
				{"������",	9599,	2692},
				{"����",		8036,	2984},
				{"�ڶ���",		8639,	2460},
				{"���ѻ�",		4350,	2143},
			};
			int idx  = (int)lua_tonumber(L, 1);

			x = ws[idx].x;
			y = ws[idx].y;
			goto teleport_area;
		}

		else
		{
			const char * arg1 = lua_tostring(L, 1);

			const entt::entity target = ecs::PlayerRuntime::FindByPlayerName(arg1);

			if (target == entt::null)
			{
				const CCI* pkCCI = P2P_MANAGER::instance().Find(arg1);

				if (pkCCI)
				{
					if (pkCCI->bChannel == g_bChannel)
					{

						PIXEL_POSITION pos;

						if (SECTREE_MANAGER::instance().GetCenterPositionOfMap(pkCCI->lMapIndex, pos)) {
#ifdef TEXTS_IMPROVEMENT
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 737, "%d#%d", pos.x, pos.y);
#endif
								if (auto* warpPos = ECS_TryGet<ecs::WarpPosition>(e))
							{
								warpPos->x = pos.x;
								warpPos->y = pos.y;
								warpPos->mapIndex = pkCCI->lMapIndex;
								g_registry.emplace_or_replace<ecs::DirtyTag>(e);
							}
							ecs::MovementSystem::WarpSet(e, pos.x, pos.y);
							lua_pushnumber(L, 1 );
						}
					}
#ifdef TEXTS_IMPROVEMENT
					else {
						ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 367, "");
					}
#endif
				}
#ifdef TEXTS_IMPROVEMENT
				else
				{
					ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 723, "");
				}
#endif
				lua_pushnumber(L, 0 );

				return 1;
			}
			else
			{
				x = ecs::PlayerRuntime::GetX(target) / 100;
				y = ecs::PlayerRuntime::GetY(target) / 100;
			}
		}

teleport_area:

		x *= 100;
		y *= 100;
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(e, CHAT_TYPE_INFO, 737, "%d#%d", x, y);
#endif
		if (auto* warpPos = ECS_TryGet<ecs::WarpPosition>(e))
		{
			warpPos->x = x;
			warpPos->y = y;
			warpPos->mapIndex = ecs::PlayerRuntime::GetMapIndex(e);
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}
		ecs::MovementSystem::WarpSet(e, x,y);
		ecs::MovementSystem::Stop(e);
		lua_pushnumber(L, 1 );
		return 1;
	}

    ALUA(pc_set_skill_level)
    {
        // migrated from CHARACTER::SetSkillLevel
        // DUAL-PATH: ECS SkillLevels + legacy call
        uint32_t dwVnum = (uint32_t)lua_tonumber(L, 1);
        uint8_t byLev = (uint8_t)lua_tonumber(L, 2);
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		SkillSystem::SetSkillLevel(character, dwVnum, byLev);
		SkillSystem::SendSkillLevelPacket(character);
        return 0;
    }

	ALUA(pc_get_premium_remain_sec)
	{
		int	remain_seconds	= 0;
		int	premium_type	= 0;
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (!lua_isnumber(L, 1))
		{
			sys_err("wrong premium index (is not number)");
			return 0;
		}

		premium_type = (int)lua_tonumber(L,1);
		switch (premium_type)
		{
			case PREMIUM_EXP:
			case PREMIUM_ITEM:
			case PREMIUM_SAFEBOX:
			case PREMIUM_AUTOLOOT:
			case PREMIUM_FISH_MIND:
			case PREMIUM_MARRIAGE_FAST:
			case PREMIUM_GOLD:
				break;

			default:
				sys_err("wrong premium index {}", premium_type);
				return 0;
		}

		remain_seconds = ecs::PlayerRuntime::GetPremiumRemainSeconds(chEntity, premium_type);

		lua_pushnumber(L, remain_seconds);
		return 1;
	}

	ALUA(pc_send_block_mode)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		ecs::PlayerRuntime::SetBlockModeForce(chEntity, (uint8_t)lua_tonumber(L, 1));

		return 0;
	}

    ALUA(pc_change_empire)
    {
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
#ifdef ENABLE_BUG_FIXES
		if (chEntity == entt::null || !g_registry.valid(chEntity)) {
            return 0;
		} else if (ecs::SocialSystem::GetParty(chEntity) != entt::null) {
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 1245, "");
#endif
            return 0;
        }
#endif
		const int result = ecs::PlayerRuntime::ChangeEmpire(
			chEntity, (uint8_t)lua_tonumber(L, 1));
        lua_pushnumber(L, result);
        return 1;
    }

	ALUA(pc_get_change_empire_count)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PlayerRuntime::GetChangeEmpireCount(chEntity));

		return 1;
	}

	ALUA(pc_set_change_empire_count)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		ecs::PlayerRuntime::IncrementChangeEmpireCount(chEntity);
		return 0;
	}

    ALUA(pc_change_name)
    {
        if (!lua_isstring(L, 1))
        {
            lua_pushnumber(L, 1);
            return 1;
        }

		lua_pushnumber(L, ecs::PlayerRuntime::RequestNameChange(
			CQuestManager::instance().GetCurrentPCEntity(), lua_tostring(L, 1)));
        return 1;
    }

	ALUA(pc_is_dead)
	{
		// migrated from CHARACTER::IsDead
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		lua_pushboolean(L, CombatSystem::IsDead(e) ? 1 : 0);
		return 1;
	}

	ALUA(pc_reset_status)
	{
		const bool validArgument = lua_isnumber(L, 1) != 0;
		const int statIndex = validArgument ? static_cast<int>(lua_tonumber(L, 1)) : -1;
		lua_pushboolean(L, validArgument && ecs::PointSystem::ResetStat(
			CQuestManager::instance().GetCurrentPCEntity(), statIndex));
		return 1;
	}

	ALUA(pc_get_ht)
	{
		// migrated from CHARACTER::GetRealPoint
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
		{
			lua_pushnumber(L, points->base.points[POINT_HT]);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

    ALUA(pc_set_ht)
    {
        if ( lua_isnumber(L, 1) == false )
            return 1;
        const int64_t newPoint = static_cast<int64_t>(lua_tonumber(L, 1));
        const entt::entity character = CQuestManager::instance().GetPCEntity(L);
        if (character == entt::null || !g_registry.valid(character))
            return 1;
        const int64_t usedPoint = newPoint - ecs::PointSystem::GetReal(character, POINT_HT);
        ecs::PointSystem::SetReal(character, POINT_HT, newPoint);
        ecs::PointSystem::Change(character, POINT_HT, 0);
        ecs::PointSystem::Change(character, POINT_STAT, -usedPoint);
        ecs::PointSystem::Compute(character);
        NetworkSyncSystem::PointsPacket(character);
        return 1;
    }

	ALUA(pc_get_iq)
	{
		// migrated from CHARACTER::GetRealPoint
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
		{
			lua_pushnumber(L, points->base.points[POINT_IQ]);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

    ALUA(pc_set_iq)
    {
        if ( lua_isnumber(L, 1) == false )
            return 1;
        const int64_t newPoint = static_cast<int64_t>(lua_tonumber(L, 1));
        const entt::entity character = CQuestManager::instance().GetPCEntity(L);
        if (character == entt::null || !g_registry.valid(character))
            return 1;
        const int64_t usedPoint = newPoint - ecs::PointSystem::GetReal(character, POINT_IQ);
        ecs::PointSystem::SetReal(character, POINT_IQ, newPoint);
        ecs::PointSystem::Change(character, POINT_IQ, 0);
        ecs::PointSystem::Change(character, POINT_STAT, -usedPoint);
        ecs::PointSystem::Compute(character);
        NetworkSyncSystem::PointsPacket(character);
        return 1;
    }

	ALUA(pc_get_st)
	{
		// migrated from CHARACTER::GetRealPoint
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
		{
			lua_pushnumber(L, points->base.points[POINT_ST]);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

    ALUA(pc_set_st)
    {
        if ( lua_isnumber(L, 1) == false )
            return 1;
        const int64_t newPoint = static_cast<int64_t>(lua_tonumber(L, 1));
        const entt::entity character = CQuestManager::instance().GetPCEntity(L);
        if (character == entt::null || !g_registry.valid(character))
            return 1;
        const int64_t usedPoint = newPoint - ecs::PointSystem::GetReal(character, POINT_ST);
        ecs::PointSystem::SetReal(character, POINT_ST, newPoint);
        ecs::PointSystem::Change(character, POINT_ST, 0);
        ecs::PointSystem::Change(character, POINT_STAT, -usedPoint);
        ecs::PointSystem::Compute(character);
        NetworkSyncSystem::PointsPacket(character);
        return 1;
    }

	ALUA(pc_get_dx)
	{
		// migrated from CHARACTER::GetRealPoint
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
		{
			lua_pushnumber(L, points->base.points[POINT_DX]);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

    ALUA(pc_set_dx)
    {
        if ( lua_isnumber(L, 1) == false )
            return 1;
        const int64_t newPoint = static_cast<int64_t>(lua_tonumber(L, 1));
        const entt::entity character = CQuestManager::instance().GetPCEntity(L);
        if (character == entt::null || !g_registry.valid(character))
            return 1;
        const int64_t usedPoint = newPoint - ecs::PointSystem::GetReal(character, POINT_DX);
        ecs::PointSystem::SetReal(character, POINT_DX, newPoint);
        ecs::PointSystem::Change(character, POINT_DX, 0);
        ecs::PointSystem::Change(character, POINT_STAT, -usedPoint);
        ecs::PointSystem::Compute(character);
        NetworkSyncSystem::PointsPacket(character);
        return 1;
    }

	ALUA(pc_is_near_vid)
	{
		// migrated from CHARACTER::GetX
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			lua_pushboolean(L, false);
			return 1;
		}
		const uint32_t vid = static_cast<uint32_t>(lua_tonumber(L, 1));
		const int range = static_cast<int>(lua_tonumber(L, 2)) * 100;
		entt::entity meEntity = CQuestManager::instance().GetPCEntity(L);
		entt::entity otherEntity = CVIDRegistry::Instance().Find(vid);
		const auto* mePos = ECS_TryGet<ecs::Position>(meEntity);
		const auto* otherPos = ECS_TryGet<ecs::Position>(otherEntity);
		if (mePos && otherPos)
		{
			lua_pushboolean(L, (DISTANCE_APPROX(mePos->x - otherPos->x, mePos->y - otherPos->y) < range) ? 1 : 0);
			return 1;
		}
		lua_pushboolean(L, 0);
		return 1;
	}

    ALUA(pc_get_empty_inventory_count)
    {
        namespace item = auriga::item;
        const auto character = CQuestManager::instance().GetPCEntity(L);
        const auto* inventory = g_registry.valid(character) ? g_registry.try_get<ecs::TypedItems>(character) : nullptr;
        const auto catalog = item::runtime::CurrentCatalog();
        if (!inventory || !catalog) {
            lua_pushinteger(L, 0);
            return 1;
        }
        const auto& state = inventory->state;
        const item::ItemStorage bag{state.fence.owner, item::ItemStorageKind::Inventory, 1};
        const auto layout = std::ranges::find(state.storages, bag, &item::ItemStorageLayout::storage);
        if (layout == state.storages.end()) {
            lua_pushinteger(L, 0);
            return 1;
        }
        lua_Integer free = static_cast<lua_Integer>(layout->columns) * layout->rows * layout->pages;
        for (const auto& instance : state.items) {
            const auto& placement = instance.placement;
            if (instance.destroyed || instance.owner != bag.owner || !placement ||
                placement->kind != bag.kind || placement->storageId != bag.id) continue;
            const auto cells = item::OccupiedItemCells(*catalog, instance, *layout);
            if (!cells || cells->size() > static_cast<std::uint64_t>(free)) {
                free = 0;
                break;
            }
            free -= static_cast<lua_Integer>(cells->size());
        }
        lua_pushinteger(L, free);
        return 1;
    }

	ALUA(pc_get_logoff_interval)
	{
		// migrated from CHARACTER::GetLogOffInterval
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* loginInfo = ECS_TryGet<ecs::LoginInfo>(e))
		{
			lua_pushnumber(L, loginInfo->logOffInterval);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	ALUA(pc_get_player_id)
	{
		// migrated from CHARACTER::GetPlayerID
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* playerId = ECS_TryGet<ecs::PlayerID>(e))
		{
			lua_pushnumber(L, playerId->pid);
			return 1;
		}
		const entt::entity pCharEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PlayerRuntime::GetPlayerID(pCharEntity));
		return 1;
	}

	ALUA(pc_get_account_id)
	{
		// migrated from CHARACTER::GetDesc
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* accountId = ECS_TryGet<ecs::AccountID>(e))
		{
			lua_pushnumber(L, accountId->aid);
			return 1;
		}
		const entt::entity pCharEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (LPDESC desc = ecs::PlayerRuntime::GetDesc(pCharEntity))
		{
			lua_pushnumber(L, desc->GetAccountTable().id);
			return 1;
		}
		lua_pushnumber(L, 0);
		return 1;
	}

	ALUA(pc_get_account)
	{
		// migrated from CHARACTER::GetDesc
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* loginInfo = ECS_TryGet<ecs::LoginInfo>(e))
		{
			lua_pushstring(L, loginInfo->login.c_str());
			return 1;
		}
		const entt::entity pCharEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (LPDESC desc = ecs::PlayerRuntime::GetDesc(pCharEntity))
		{
			lua_pushstring(L, desc->GetAccountTable().login);
			return 1;
		}
		lua_pushstring(L, "");
		return 1;
	}

	ALUA(pc_is_riding)
	{
		// migrated from CHARACTER::IsRiding
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* mount = ECS_TryGet<ecs::MountState>(e))
		{
			lua_pushboolean(L, (mount->mountVnum != 0) ? 1 : 0);
			return 1;
		}
		lua_pushboolean(L, MountSystem::IsRiding(e) ? 1 : 0);
		return 1;
	}

	ALUA(pc_can_warp)
	{
		// migrated from CHARACTER::CanWarp
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e != entt::null && g_registry.valid(e))
		{
			const bool blocked = g_registry.all_of<ecs::DeadTag>(e) || g_registry.all_of<ecs::StunTag>(e);
			if (blocked)
			{
				lua_pushboolean(L, 0);
				return 1;
			}
		}
		const entt::entity pCharEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, ecs::PlayerRuntime::CanWarp(pCharEntity) ? 1 : 0);
		return 1;
	}

    ALUA(pc_dec_skill_point)
    {
        // migrated from CHARACTER::PointChange(POINT_SKILL, ...)
        // DUAL-PATH: ECS update + legacy call during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
        {
            points->base.points[POINT_SKILL] -= 1;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        const entt::entity pCharEntity = CQuestManager::instance().GetCurrentPCEntity();
        if (pCharEntity != entt::null && g_registry.valid(pCharEntity))
            ecs::PointSystem::Change(pCharEntity, POINT_SKILL, -1);
        return 0;
    }

	ALUA(pc_get_skill_point)
	{
		// migrated from CHARACTER::GetPoint
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
		{
			lua_pushnumber(L, points->base.points[POINT_SKILL]);
			return 1;
		}
		// TODO Phase 8: decompose CharacterPoints
		const entt::entity pCharEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PointSystem::Get(pCharEntity, POINT_SKILL));
		return 1;
	}

	ALUA(pc_get_channel_id)
	{
		lua_pushnumber(L, g_bChannel);

		return 1;
	}

	ALUA(pc_charge_cash)
	{
        // migrated from CHARACTER::GetDesc
        // DUAL-PATH: legacy only during migration window
		if (lua_gettop(L) < 1 || !lua_isnumber(L, 1))
		{
			sys_err("not enough arguments.");
			lua_pushboolean(L, 0);
			return 1;
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const auto* session = ECS_TryGet<ecs::NetworkSession>(chEntity);
		if (!session || !session->desc)
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		int32_t amount = (int32_t)lua_tonumber(L, 1);
		if (amount < 1 || amount > 1000)
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		TRequestChargeCash packet;
		packet.aid = session->desc->GetAccountTable().id;
		packet.amount = amount;

		db_clientdesc->DBPacketHeader(HEADER_GD_REQUEST_CHARGE_CASH, 0, sizeof(TRequestChargeCash));
		db_clientdesc->Packet(&packet, sizeof(packet));
		lua_pushboolean(L, 1);
		return 1;
	}

	ALUA(pc_get_informer_type)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const auto* award = ECS_TryGet<ecs::ItemAward>(character);
		lua_pushstring(L, award ? award->command.c_str() : "");
		return 1;
	}

	ALUA(pc_get_informer_item)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const auto* award = ECS_TryGet<ecs::ItemAward>(character);
		lua_pushnumber(L, award ? award->vnum : 0);
		return 1;
	}

#ifdef ENABLE_NEWSTUFF

    ALUA(pc_set_race0)
    {
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (chEntity == entt::null || !g_registry.valid(chEntity))
            return 0;
        int amount = MINMAX(0, lua_tonumber(L, 1), JOB_MAX_NUM);
		const ESex mySex = static_cast<ESex>(ecs::PlayerRuntime::GetSex(chEntity));
        uint32_t dwRace = MAIN_RACE_WARRIOR_M;
        switch (amount)
        {
            case JOB_WARRIOR:
                dwRace = (mySex==SEX_MALE)?MAIN_RACE_WARRIOR_M:MAIN_RACE_WARRIOR_W;
                break;
            case JOB_ASSASSIN:
                dwRace = (mySex==SEX_MALE)?MAIN_RACE_ASSASSIN_M:MAIN_RACE_ASSASSIN_W;
                break;
            case JOB_SURA:
                dwRace = (mySex==SEX_MALE)?MAIN_RACE_SURA_M:MAIN_RACE_SURA_W;
                break;
            case JOB_SHAMAN:
                dwRace = (mySex==SEX_MALE)?MAIN_RACE_SHAMAN_M:MAIN_RACE_SHAMAN_W;
                break;
        }
		if (dwRace != ecs::PlayerRuntime::GetRaceNum(chEntity) &&
			ecs::PlayerRuntime::SaveRaceChange(chEntity, static_cast<uint8_t>(dwRace)))
        {
			SkillSystem::ClearSkill(chEntity);
			SkillSystem::SetSkillGroup(chEntity, 0);
			AffectSystem::SetPolymorph(chEntity, 101);
			AffectSystem::SetPolymorph(chEntity, 0);
        }
        return 0;
    }

    ALUA(pc_del_another_quest_flag)
    {
        // migrated from CHARACTER quest flag system
        // TODO Phase 8: dedicated QuestFlags component
        // DUAL-PATH: legacy only during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        (void)e;
        if (!lua_isstring(L, 1) || !lua_isstring(L, 2))
        {
            sys_err("QUEST wrong del flag");
            return 0;
        }
        const char * sz = lua_tostring(L, 1);
        const char * sz2 = lua_tostring(L, 2);
        CQuestManager & q = CQuestManager::Instance();
        PC * pPC = q.GetCurrentPC();
        lua_pushboolean(L, pPC->DeleteFlag(string(sz)+"."+sz2));
        return 1;
    }

	ALUA(pc_pointchange)
	{
		// migrated from CHARACTER::PointChange
		// DUAL-PATH: ECS update + legacy call during migration window
		const int type = static_cast<int>(lua_tonumber(L, 1));
		const int amount = static_cast<int>(lua_tonumber(L, 2));
		const bool broadcast = lua_toboolean(L, 3) != 0;
		const bool ignoreMax = lua_toboolean(L, 4) != 0;
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		if (character != entt::null && g_registry.valid(character))
			ecs::PointSystem::Change(character, type, amount, broadcast, ignoreMax);
		return 0;
	}

	ALUA(pc_pullmob)
	{
		CombatSystem::PullMonster(CQuestManager::instance().GetPCEntity(L));
		return 0;
	}

    ALUA(pc_set_level0)
    {
        const int level = static_cast<int>(lua_tonumber(L, 1));
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (ecs::PointSystem::ResetAllPoints(chEntity, level))
		{
			SkillSystem::ClearSkill(chEntity);
			SkillSystem::ClearSubSkill(chEntity);
		}
        return 0;
    }

    ALUA(pc_set_gm_level)
    {
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		ecs::PlayerRuntime::RefreshGMLevel(chEntity);
        return 0;
    }


	ALUA(pc_if_fire)
	{
		// migrated from CHARACTER::IsAffectFlag(AFF_FIRE)
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e != entt::null && g_registry.valid(e))
		{
			lua_pushboolean(L, g_registry.all_of<ecs::FireTag>(e) ? 1 : 0);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, AffectSystem::IsAffectFlag(chEntity, AFF_FIRE) ? 1 : 0);
		return 1;
	}

	ALUA(pc_if_invisible)
	{
		// migrated from CHARACTER::IsAffectFlag(AFF_INVISIBILITY)
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
		{
			lua_pushboolean(L, sf->isInvisible ? 1 : 0);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, AffectSystem::IsAffectFlag(chEntity, AFF_INVISIBILITY) ? 1 : 0);
		return 1;
	}
	ALUA(pc_if_poison)
	{
		// migrated from CHARACTER::IsAffectFlag(AFF_POISON)
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
		{
			lua_pushboolean(L, sf->hasPoisoned ? 1 : 0);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, AffectSystem::IsAffectFlag(chEntity, AFF_POISON) ? 1 : 0);
		return 1;
	}
	ALUA(pc_if_slow)
	{
		// migrated from CHARACTER::IsAffectFlag(AFF_SLOW)
		// TODO Phase 8: dedicated SlowTag
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		(void)e;
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, AffectSystem::IsAffectFlag(chEntity, AFF_SLOW) ? 1 : 0);
		return 1;
	}
	ALUA(pc_if_stun)
	{
		// migrated from CHARACTER::IsAffectFlag(AFF_STUN)
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e != entt::null && g_registry.valid(e))
		{
			const auto* sf = ECS_TryGet<ecs::StatusFlags>(e);
			lua_pushboolean(L, (g_registry.all_of<ecs::StunTag>(e) || (sf && sf->isStunned)) ? 1 : 0);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, AffectSystem::IsAffectFlag(chEntity, AFF_STUN) ? 1 : 0);
		return 1;
	}
    ALUA(pc_sf_fire)
    {
        // migrated from CHARACTER::AddAffect
        // DUAL-PATH: ECS update + legacy call during migration window
        const bool enabled = lua_toboolean(L, 1) != 0;
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (e != entt::null && g_registry.valid(e))
        {
            if (enabled)
                g_registry.emplace_or_replace<ecs::FireTag>(e);
            else if (g_registry.all_of<ecs::FireTag>(e))
                g_registry.remove<ecs::FireTag>(e);
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (e != entt::null && g_registry.valid(e))
        {
            if(enabled)
                AffectSystem::AddAffect(e, AFFECT_FIRE, 0, 0, AFF_FIRE, 3 * 5 + 1, 0, 1, 0);
            else
                AffectSystem::RemoveAffect(e, AFFECT_FIRE);
        }
        return 0;
    }
    ALUA(pc_sf_invisible)
    {
        // migrated from CHARACTER::SetInvisibility
        // DUAL-PATH: ECS StatusFlags + legacy call
        const bool enabled = lua_toboolean(L, 1) != 0;
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
        {
            sf->isInvisible = enabled;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (e != entt::null && g_registry.valid(e))
        {
            if(enabled)
                AffectSystem::AddAffect(e, AFFECT_INVISIBILITY, 0, 0, AFF_INVISIBILITY, 60*60*24*365*60+1, 0, 1, 0);
            else
                AffectSystem::RemoveAffect(e, AFFECT_INVISIBILITY);
        }
        return 0;
    }
    ALUA(pc_sf_poison)
    {
        // migrated from CHARACTER::AddAffect
        // DUAL-PATH: ECS StatusFlags + legacy call
        const bool enabled = lua_toboolean(L, 1) != 0;
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
        {
            sf->hasPoisoned = enabled;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (e != entt::null && g_registry.valid(e))
        {
            if(enabled)
                AffectSystem::AddAffect(e, AFFECT_POISON, 0, 0, AFF_POISON, 30+1, 0, 1, 0);
            else
                AffectSystem::RemoveAffect(e, AFFECT_POISON);
        }
        return 0;
    }
    ALUA(pc_sf_slow)
    {
        // migrated from CHARACTER::AddAffect
        // TODO Phase 8: dedicated SlowTag
        // DUAL-PATH: ECS update + legacy call during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (e != entt::null && g_registry.valid(e))
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        if (e != entt::null && g_registry.valid(e))
        {
            if(lua_toboolean(L, 1))
                AffectSystem::AddAffect(e, AFFECT_SLOW, 19, -30, AFF_SLOW, 30, 0, 1, 0);
            else
                AffectSystem::RemoveAffect(e, AFFECT_SLOW);
        }
        return 0;
    }
    ALUA(pc_sf_stun)
    {
        // migrated from CHARACTER::Stun
        // DUAL-PATH: ECS StunTag + legacy call
        const bool enabled = lua_toboolean(L, 1) != 0;
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (e != entt::null && g_registry.valid(e))
        {
            if (enabled)
                g_registry.emplace_or_replace<ecs::StunTag>(e);
            else if (g_registry.all_of<ecs::StunTag>(e))
                g_registry.remove<ecs::StunTag>(e);
            if (auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
                sf->isStunned = enabled;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (e != entt::null && g_registry.valid(e))
        {
            if(enabled)
                AffectSystem::AddAffect(e, AFFECT_STUN, 0, 0, AFF_STUN, 30, 0, 1, 0);
            else
                AffectSystem::RemoveAffect(e, AFFECT_STUN);
        }
        return 0;
    }

	ALUA(pc_sf_kill)
	{
		CombatSystem::Dead(ecs::PlayerRuntime::FindByPlayerName(lua_tostring(L, 1)));
		return 0;
	}

    ALUA(pc_sf_dead)
    {
        // migrated from CHARACTER::Dead
        // DUAL-PATH: ECS DeadTag + legacy call
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (e != entt::null && g_registry.valid(e))
        {
            g_registry.emplace_or_replace<ecs::DeadTag>(e);
            if (auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
                sf->isDead = true;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
            g_dispatcher.trigger(ecs::EvEntityDied{entt::null, e});
        }
        CombatSystem::Dead(e);
        return 0;
    }

	ALUA(pc_get_exp_level)
	{
		if (!lua_isnumber(L, 1))
		{
			sys_err("arg1 must be a number");
			return 0;
		}
		lua_pushnumber(L, (uint32_t)(exp_table[MINMAX(0, lua_tonumber(L, 1), PLAYER_MAX_LEVEL_CONST)] / 100));
		return 1;
	}

	ALUA(pc_get_exp_level0)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			sys_err("arg1 and arg2 must be numbers");
			return 0;
		}
		lua_pushnumber(L, (uint32_t)((exp_table[MINMAX(0, lua_tonumber(L, 1), PLAYER_MAX_LEVEL_CONST)] / 100) * MINMAX(1, lua_tonumber(L, 2), 100)));
		return 1;
	}

    ALUA(pc_set_max_health)
    {
        const entt::entity character = CQuestManager::instance().GetPCEntity(L);
        if (character == entt::null || !g_registry.valid(character))
            return 0;
        ecs::PointSystem::Change(character, POINT_HP,
            ecs::PointSystem::GetMaxHP(character) - ecs::PointSystem::Get(character, POINT_HP));
        ecs::PointSystem::Change(character, POINT_SP,
            ecs::PointSystem::GetMaxSP(character) - ecs::PointSystem::Get(character, POINT_SP));
        return 0;
    }

	ALUA(pc_get_ip0)
	{
		// migrated from CHARACTER::GetDesc
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* session = ECS_TryGet<ecs::NetworkSession>(e))
		{
			lua_pushstring(L, session->desc ? session->desc->GetHostName() : "");
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		LPDESC desc = ecs::PlayerRuntime::GetDesc(chEntity);
		lua_pushstring(L, desc ? desc->GetHostName() : "");
		return 1;
	}

	ALUA(pc_get_client_version0)
	{
		// migrated from CHARACTER::GetDesc
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* session = ECS_TryGet<ecs::NetworkSession>(e))
		{
			lua_pushstring(L, (session->desc && session->desc->GetClientVersion()) ? session->desc->GetClientVersion() : "");
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		LPDESC desc = ecs::PlayerRuntime::GetDesc(chEntity);
		lua_pushstring(L, (desc && desc->GetClientVersion()) ? desc->GetClientVersion() : "");
		return 1;
	}

	ALUA(pc_dc_delayed0)
	{
        // migrated from CHARACTER::GetDesc
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		LPDESC desc = ecs::PlayerRuntime::GetDesc(chEntity);
		bool bRet = desc && desc->DelayedDisconnect(MINMAX(0, lua_tonumber(L, 1), 60*5));
		lua_pushboolean(L, bRet);
		return 1;
	}

	ALUA(pc_dc_direct0)
	{
        // migrated from CHARACTER::Disconnect
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		ecs::SessionSystem::Disconnect(chEntity, lua_tostring(L, 1));
		return 0;
	}

	ALUA(pc_is_trade0)
	{
		// migrated from CHARACTER::GetExchange
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* shop = ECS_TryGet<ecs::ShopState>(e))
		{
			if (shop->currentShop != entt::null || shop->myShop != entt::null)
			{
				lua_pushboolean(L, 1);
				return 1;
			}
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, ecs::SocialSystem::HasExchange(chEntity) ? 1 : 0);
		return 1;
	}

	ALUA(pc_is_busy0)
	{
		// migrated from CHARACTER busy-state checks
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e != entt::null && g_registry.valid(e))
		{
			const auto* shop = ECS_TryGet<ecs::ShopState>(e);
			const auto* safebox = ECS_TryGet<ecs::SafeboxRef>(e);
				if (ecs::SocialSystem::HasExchange(e) ||
				(shop && (shop->currentShop != entt::null || shop->myShop != entt::null || shop->shopOwner != entt::null || shop->underRefine)) ||
				(safebox && safebox->isOpening) || ecs::SessionSystem::IsCubeOpen(e))
			{
				lua_pushboolean(L, 1);
				return 1;
			}
		}
		lua_pushboolean(L, ecs::SocialSystem::HasExchange(e) ? 1 : 0);
		return 1;
	}

	ALUA(pc_is_arena0)
	{
		// migrated from CHARACTER::GetArena
		const entt::entity e = CQuestManager::instance().GetPCEntity(L);
		lua_pushboolean(L, ecs::SocialSystem::GetWarMap(e) != nullptr ? 1 : 0);
		return 1;
	}

	ALUA(pc_is_arena_observer0)
	{
		lua_pushboolean(L, ecs::PlayerRuntime::IsArenaObserverMode(
			CQuestManager::instance().GetPCEntity(L)) ? 1 : 0);
		return 1;
	}

	ALUA(pc_is_available0)
	{
		// migrated from CHARACTER availability check
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e != entt::null && g_registry.valid(e))
		{
			lua_pushboolean(L, 1);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, chEntity != entt::null && g_registry.valid(chEntity));
		return 1;
	}

	ALUA(pc_is_pvp0)
	{
		// migrated from CHARACTER::GetPlayerID
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* playerId = ECS_TryGet<ecs::PlayerID>(e))
		{
			lua_pushboolean(L, CPVPManager::instance().IsFighting(playerId->pid) ? 1 : 0);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, CPVPManager::instance().IsFighting(
			ecs::PlayerRuntime::GetPlayerID(chEntity)) ? 1 : 0);
		return 1;
	}

#endif

#ifdef ENABLE_PC_OPENSHOP
	ALUA(pc_open_shop0)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (chEntity == entt::null || !g_registry.valid(chEntity))
			return 0;

		const auto* safebox = g_registry.try_get<ecs::SafeboxRef>(chEntity);
		//PREVENT_TRADE_WINDOW
		if ((safebox && safebox->isOpening) || ecs::SocialSystem::HasExchange(chEntity) ||
			ecs::SocialSystem::GetMyShop(chEntity) || ecs::SessionSystem::IsCubeOpen(chEntity))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 294, "");
#endif
			return 0;
		}
		//END_PREVENT_TRADE_WINDOW

		TryOpenTypedShop(chEntity, CQuestManager::instance().GetCurrentNPCEntity(), static_cast<uint32_t>(lua_tonumber(L, 1)));
		return 0;
	}
#endif

#ifdef ENABLE_NEWGUILDMAKE
	enum MKGLD {MKGLD_INVALID_NAME_LENGTH=-2, MKGLD_INVALID_NAME_INPUT=-1, MKGLD_GUILD_NOT_CREATED=0, MKGLD_GUILD_CREATED=1, MKGLD_ALREADY_GUILDED=2, MKGLD_ALREADY_MASTER_GUILD=3};
	ALUA(pc_make_guild0)
	{
		// -2 guild name is invalid (strlen <2 or >11!)
		// -1 guild name is invalid (special chars found!)
		// 0 guild not created (guild name already present or already member of a guild)
		// 1 guild created
		// 2 player already part of a guild
		// 3 player already guild master
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (chEntity == entt::null || !g_registry.valid(chEntity))
		{
			lua_pushnumber(L, MKGLD_GUILD_NOT_CREATED);
			return 1;
		}

		if (CGuild* guild = ecs::SocialSystem::GetGuild(chEntity))
		{
			lua_pushnumber(L, ecs::PlayerRuntime::GetPlayerID(chEntity) == guild->GetMasterPID()
				? MKGLD_ALREADY_MASTER_GUILD : MKGLD_ALREADY_GUILDED);
			return 1;
		}
		const char* guild_name = lua_tostring(L, 1);
		size_t guild_lname = strlen(guild_name);
		if (guild_lname<2 || 11<guild_lname)
		{
			lua_pushnumber(L, MKGLD_INVALID_NAME_LENGTH);
			return 1;
		}

		TGuildCreateParameter cp;
		memset(&cp, 0, sizeof(cp));

		cp.master = chEntity;
		strlcpy(cp.name, guild_name, sizeof(cp.name));

		int ret_type = MKGLD_GUILD_NOT_CREATED;
		if (check_name(cp.name))
		{
			if(CGuildManager::instance().CreateGuild(cp))
				ret_type = MKGLD_GUILD_CREATED;
			else
				ret_type = MKGLD_GUILD_NOT_CREATED;
		}
		else ret_type = MKGLD_INVALID_NAME_INPUT;
		lua_pushnumber(L, ret_type);
		return 1;
	}
#endif




#ifdef ENABLE_MULTI_LANGUAGE
	ALUA(pc_get_language)
	{
		// migrated from CHARACTER::GetDesc
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* session = ECS_TryGet<ecs::NetworkSession>(e))
		{
			if (session->desc)
			{
				lua_pushstring(L, LC_CONVERT(session->desc->GetLanguage()));
				return 1;
			}
		}

		lua_pushstring(L, "ro");
		return 1;
	}
#endif

#ifdef __ENABLE_BLOCK_EXP__
	ALUA(_Block_Exp)
	{
		ecs::PointSystem::SetExperienceBlocked(
			CQuestManager::instance().GetCurrentPCEntity(), true);
		return 0;
	}

	ALUA(_Unblock_Exp)
	{
		// migrated from CHARACTER::Block_Exp
		// DUAL-PATH: ECS update + legacy call during migration window
		ecs::PointSystem::SetExperienceBlocked(
			CQuestManager::instance().GetPCEntity(L), false);
		return 0;
	}
#endif

#ifdef ENABLE_GAYA_SYSTEM
    static int NativeQuestGaya(lua_State* L,entt::entity character,std::int64_t delta) {
        if(!delta)return 0;
        auto& q=CQuestManager::instance();const auto* state=q.GetCurrentState();
        if(delta==INT64_MIN || !lua_isyieldable(L) || q.IsInOtherPCBlock() || !state || state->co!=L)return luaL_error(L,"Native Gaya requires the authenticated quest coroutine");
        const auto pid=ecs::PlayerRuntime::GetPlayerID(character);
        if(!RequestTypedQuestGayaChange(character,delta,[pid,co=L](bool ok){CQuestManager::instance().ResumeTypedItems(pid,co,ok);}))return luaL_error(L,"Native Gaya transaction unavailable or insufficient funds");
        lua_settop(L,0);lua_pushliteral(L,"typed_items");return lua_yieldk(L,1,0,ResumeNativeQuestMoney);
    }
	ALUA(pc_get_gaya)
	{
		lua_pushnumber(L, ecs::PointSystem::Get(CQuestManager::instance().GetPCEntity(L), POINT_GAYA));
		return 1;
	}

	ALUA(pc_give_gaya)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
        if(g_registry.valid(character) && g_registry.all_of<ecs::TypedItems>(character)) {
            int valid=0;const auto delta=lua_tointegerx(L,1,&valid);if(!valid || delta<=0)return luaL_error(L,"Native Gaya award requires a positive exact integer");return NativeQuestGaya(L,character,delta);
        }
		if (!lua_isnumber(L, 1))
		{
			sys_err("QUEST : wrong argument");
			return 0;
		}

		int iAmount = (int)lua_tonumber(L, 1);

		if (iAmount <= 0)
		{
			sys_err("QUEST : gaya amount less then zero");
			return 0;
		}

		DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST,
			ecs::PlayerRuntime::GetPlayerID(character), iAmount);
		ecs::PointSystem::Change(character, POINT_GAYA, iAmount, true);
		return 0;
	}

	ALUA(pc_change_gaya)
	{
        const auto native=CQuestManager::instance().GetPCEntity(L);
        if(g_registry.valid(native) && g_registry.all_of<ecs::TypedItems>(native)) {
            int valid=0;const auto delta=lua_tointegerx(L,-1,&valid);if(!valid)return luaL_error(L,"Native Gaya change requires an exact signed integer");return NativeQuestGaya(L,native,delta);
        }
		const int gaya = static_cast<int>(lua_tonumber(L, -1));
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const int64_t currentGaya = ecs::PointSystem::Get(character, POINT_GAYA);
		if (gaya + currentGaya < 0)
			sys_err("QUEST wrong ChangeGaya {} (now {})", gaya, currentGaya);
		else
		{
			DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST,
				ecs::PlayerRuntime::GetPlayerID(character), gaya);
			ecs::PointSystem::Change(character, POINT_GAYA, gaya, true);
		}
		return 0;
	}
#endif

#ifdef __HIDE_COSTUME_SYSTEM__
	ALUA(pc_hide_costume)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (!lua_isnumber(L, 1))
		{
			sys_err("pc_hide_costume::wrong part value (1-4)");
			return 0;
		}

		if (!lua_isboolean(L, 2))
		{
			sys_err("Invalid argument: arg2 must be boolean.");
			return 0;
		}

		/*
		* 1 : WEAR_COSTUME_BODY
		* 2 : WEAR_COSTUME_HAIR
		* 3 : WEAR_COSTUME_ACCE
		* 4 : WEAR_COSTUME_WEAPON
		*/
		uint8_t bPartPos = (uint8_t)lua_tonumber(L, 1);
		bool hidePart = lua_toboolean(L, 2);

		if (!ecs::PlayerRuntime::SetCostumeHidden(chEntity, bPartPos, hidePart))
		{
			sys_err("Invalid part");
			return 0;
		}

		NetworkSyncSystem::UpdatePacket(chEntity);
		return 0;
	}
#endif


#if defined(__DUNGEON_INFO_SYSTEM__)
	ALUA(pc_get_last_damage)
	{
		if (!lua_isnumber(L, 1)) {
			sys_err("invalid argument.");
			return 0;
		}

		int race = lua_tonumber(L, 1);

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		if (chEntity == entt::null) {
			sys_err("current character entity is null");
			return 0;
		}

		lua_pushnumber(L, ecs::PlayerRuntime::GetQuestDamage(chEntity, race));
		return 1;
	}
#endif

#ifdef ENABLE_RANKING
	ALUA(pc_get_rank_dungeon)
	{
		lua_pushnumber(L, ecs::PlayerRuntime::GetRankPoints(
			CQuestManager::instance().GetCurrentPCEntity(), 16));
		return 1;
	}

	ALUA(pc_set_rank_dungeon)
	{
		// migrated from CHARACTER::SetRankPoints
		// DUAL-PATH: ECS update + legacy call during migration window
		if (lua_isnumber(L, 1)) {
			long long lPoints = lua_tonumber(L, 1);
			ecs::PlayerRuntime::SetRankPoints(
				CQuestManager::instance().GetPCEntity(L), 16, lPoints);
		}
		return 0;
	}
#endif

#ifdef ENABLE_BLOCK_MULTIFARM
	ALUA(pc_can_drop)
	{
		// migrated from CHARACTER::FindAffect(AFFECT_DROP_BLOCK)
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e != entt::null && g_registry.valid(e) && g_registry.all_of<ecs::SafeZoneTag>(e))
		{
			lua_pushboolean(L, 0);
			return 1;
		}
		const bool ret = !AffectSystem::FindAffect(e, AFFECT_DROP_BLOCK, APPLY_NONE);
		lua_pushboolean(L, ret ? 1 : 0);
		return 1;
	}
#endif

#ifdef ENABLE_BIOLOGIST_UI
	ALUA(pc_open_biologist_change)
	{
        // migrated from CHARACTER::ChatPacket
        // DUAL-PATH: legacy only during migration window
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		if (character != entt::null && g_registry.valid(character)) {
			ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologistch_clear");
			int type = APPLY_NONE, last = 0;
			for (int i = 0; i < 16; i++) {
				if (static_cast<int>(biologistMissionInfo[i][10]) == 1) {
					if (ecs::QuestSystem::GetFlag(character, "biologist.stat") > i) {
						for (int j = 0; j < 4; j++) {
							type = biologistMissionInfo[i][2 + (j * 2)];
							if (type != APPLY_NONE) {
								CAffect * pkAff = AffectSystem::FindAffect(character, static_cast<int>(biologistMissionInfo[i][13]), aApplyInfo[type].bPointType);
								if (pkAff) {
									continue;
								}

								if (last == 0) {
									last = 1;
								}

								ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologistch_append %d#%d#%.3f#%d", i, type, biologistMissionInfo[i][3 + (j * 2)], j);
							}
						}
					}
				}
			}

			if (last == 0) {
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 872, "");
#endif
			} else {
				ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologistch_open");
			}
		}

		return 0;
	}
#endif

#ifdef ENABLE_VOTE_FOR_BONUS
	ALUA(pc_can_get_bonus_vote) {
        // migrated from CHARACTER::FindAffect
        // DUAL-PATH: legacy only during migration window
		int32_t ret = 0;

		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		if (character != entt::null && g_registry.valid(character)) {
			if (!AffectSystem::FindAffect(character, AFFECT_VOTEFORBONUS)) {
				const auto* session = ECS_TryGet<ecs::NetworkSession>(character);
				LPDESC d = session ? session->desc : nullptr;
				if (d) {
					std::unique_ptr<SQLMsg> msg(DBManager::instance().DirectQuery("SELECT UNIX_TIMESTAMP(vote_time)-UNIX_TIMESTAMP(NOW()) FROM account.account WHERE id=%u", d->GetAccountTable().id));
					if (msg->Get()->uiNumRows > 0) {
						MYSQL_ROW row = msg->Get()->FetchRow();
						int32_t expire = atoi(row[0]);
						if (expire > 0)
						{
							ret = 3;
						}
						else
						{
							ret = 2;
						}
					}
					else
					{
						ret = 2;
					}
				}
			}
			else
			{
				ret = 1;
			}
		}

		lua_pushnumber(L, ret);
		return 1;
	}

    ALUA(pc_set_bonus_for_vote) {
        // migrated from CHARACTER::AddAffect
        // DUAL-PATH: ECS update + legacy call during migration window
        int32_t ret = 0;
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (lua_isnumber(L, 1)) {
            int32_t type = (int32_t)lua_tonumber(L, 1);
            if (type >= 1 && type <= 3) {
                if (e != entt::null && g_registry.valid(e)) {
                    if (!AffectSystem::FindAffect(e, AFFECT_VOTEFORBONUS))
                    {
                        switch (type)
                        {
                            case 1:
                            {
                                AffectSystem::AddAffect(e, AFFECT_VOTEFORBONUS, POINT_ATTBONUS_MONSTER, 10, AFF_NONE, get_global_time() + 86400, 0, false);
                                ret = 2;
                            }
                            break;
                            case 2:
                            {
                                AffectSystem::AddAffect(e, AFFECT_VOTEFORBONUS, POINT_EXP_DOUBLE_BONUS, 20, AFF_NONE, get_global_time() + 86400, 0, false);
                                ret = 3;
                            }
                            break;
                            case 3:
                            {
                                AffectSystem::AddAffect(e, AFFECT_VOTEFORBONUS, POINT_DOUBLE_DROP_ITEM, 20, AFF_NONE, get_global_time() + 86400, 0, false);
                                ret = 4;
                            }
                            break;
                            default:
                            {
                                ret = 5;
                            }
                            break;
                        }
                        if (ret >= 2 && e != entt::null && g_registry.valid(e))
                            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
                    }
                    else
                    {
                        ret = 1;
                    }
                }
            }
        }

        lua_pushnumber(L, ret);
        return 1;
    }
#endif

#ifdef ENABLE_VOTE4BUFF
	ALUA(pc_get_vote_coin)
	{
		lua_pushnumber(L, ecs::PlayerRuntime::GetVoteCoin(
			CQuestManager::instance().GetCurrentPCEntity()));
		return 1;
	}
	ALUA(pc_set_vote_coin)
	{
		// migrated from CHARACTER::SetVoteCoin
		// DUAL-PATH: ECS update + legacy call during migration window
		if (lua_isnumber(L, 1))
		{
			const long long amount = static_cast<long long>(lua_tonumber(L, 1));
			ecs::PlayerRuntime::SetVoteCoin(
				CQuestManager::instance().GetPCEntity(L), amount);
		}
		return 0;
	}
#endif


    namespace
    {
        namespace item = auriga::item;

        std::optional<item::MissionItemAmount> ReadQuestItem(lua_State* L, int first, bool withCount)
        {
            int valid = 0;
            const auto category = lua_tointegerx(L, first, &valid);
            if (!valid || category < 1 || category > UINT16_MAX)
                return std::nullopt;
            const auto local = lua_tointegerx(L, first + 1, &valid);
            if (!valid || local < 1 || local > UINT32_MAX)
                return std::nullopt;
            const auto level = lua_tointegerx(L, first + 2, &valid);
            if (!valid || level < 0 || level > 255)
                return std::nullopt;
            const auto key = item::ItemKey::Create(static_cast<uint16_t>(category), static_cast<uint32_t>(local));
            if (!key)
                return std::nullopt;
            const auto count = withCount ? lua_tointegerx(L, first + 3, &valid) : 1;
            if (!valid || count < 1 || count > 50000)
                return std::nullopt;
            return item::MissionItemAmount{*key, static_cast<uint16_t>(level), static_cast<uint32_t>(count)};
        }

        bool ReadQuestItems(lua_State* L, int index, std::vector<item::MissionItemAmount>& result)
        {
            if (!lua_istable(L, index) || lua_rawlen(L, index) > item::MaxMissionItemEntries)
                return false;
            const auto count = static_cast<int>(lua_rawlen(L, index));
            for (int i = 1; i <= count; ++i)
            {
                lua_rawgeti(L, index, i);
                if (!lua_istable(L, -1) || lua_rawlen(L, -1) != 4)
                {
                    lua_pop(L, 1);
                    return false;
                }
                const int row = lua_gettop(L);
                for (int field = 1; field <= 4; ++field)
                    lua_rawgeti(L, row, field);
                const auto value = ReadQuestItem(L, row + 1, true);
                lua_pop(L, 5);
                if (!value)
                    return false;
                result.push_back(*value);
            }
            return true;
        }
    }

    static bool IncludeSpecialInventory(lua_State* L)
    {
        return lua_isboolean(L, 4) && lua_toboolean(L, 4);
    }

    ALUA(pc_typed_item_count)
    {
        const auto value = ReadQuestItem(L, 1, false);
        const auto entity = CQuestManager::instance().GetCurrentCharacter();
        const auto* typed = g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItems>(entity) : nullptr;
        if (!value || !typed || g_registry.all_of<ecs::TypedItemBusy>(entity))
            lua_pushnil(L);
        else
        {
            const auto count = IncludeSpecialInventory(L) ? item::CountGameplayExchangeItems : item::CountMissionItems;
            lua_pushinteger(L, count(typed->state.items, typed->state.fence.owner, value->key, value->level));
        }
        return 1;
    }

    ALUA(pc_typed_pet_egg_key)
    {
        const std::string_view code = luaL_checkstring(L, 1);
        const auto catalog = item::runtime::CurrentCatalog();
        if (!catalog) return 0;
        const auto& entries = catalog->Entries();
        const auto egg = std::ranges::find_if(entries, [&](const auto& entry) {
            return entry.second.base.contentCode == code;
        });
        if (egg == entries.end() || !catalog->FindPetHatchFor(egg->first)) return 0;
        lua_pushinteger(L, static_cast<std::uint16_t>(egg->first.Category()));
        lua_pushinteger(L, egg->first.LocalId());
        return 2;
    }

    ALUA(pc_typed_has_equipped)
    {
        const auto entity = CQuestManager::instance().GetCurrentCharacter();
        const auto* typed = g_registry.valid(entity) ? g_registry.try_get<ecs::TypedItems>(entity) : nullptr;
        if (!typed || g_registry.all_of<ecs::TypedItemBusy>(entity))
        {
            lua_pushnil(L);
            return 1;
        }

        std::array<bool, item::EquipmentSlotCount> slots{};
        for (int index = 1; index <= lua_gettop(L); ++index)
        {
            const auto slot = luaL_checkinteger(L, index);
            luaL_argcheck(L, slot >= 1 && slot <= item::EquipmentSlotCount, index, "invalid typed equipment slot");
            slots[static_cast<std::size_t>(slot - 1)] = true;
        }
        const bool equipped = std::ranges::any_of(typed->state.items, [&](const auto& instance) {
            return !instance.destroyed && instance.owner == typed->state.fence.owner && instance.placement &&
                instance.placement->kind == item::ItemStorageKind::Equipment &&
                instance.placement->anchor < slots.size() && slots[instance.placement->anchor];
        });
        lua_pushboolean(L, equipped);
        return 1;
    }

    ALUA(pc_typed_item_grant)
    {
        const auto amount = ReadQuestItem(L, 1, true);
        const auto catalog = item::runtime::CurrentCatalog();
        const auto* definition = catalog && amount ? catalog->Find(amount->key) : nullptr;
        if (!definition || definition->base.retired || !item::SupportsPersistentGrant(*catalog, *definition) ||
            !catalog->RefinedBaseStats(amount->key, amount->level))
        {
            sys_err("Invalid typed quest reward");
            return 0;
        }
        QueueTypedItemGrant(CQuestManager::instance().GetCurrentPCEntity(), {amount->key, amount->level, amount->count});
        return 0;
    }

    static int RunTypedItemExchange(lua_State* L, bool useSelectedItem, bool inDungeon = false)
    {
        const auto refused = [L](const char* reason) {
            lua_pushboolean(L, false);
            lua_pushstring(L, reason);
            return 2;
        };
        auto& quest = CQuestManager::instance();
        auto* state = quest.GetCurrentState();
        if (!lua_isyieldable(L) || quest.IsInOtherPCBlock() || !state || state->co != L)
            return refused("invalid_context");

        int valid = 0;
        std::vector<item::MissionItemAmount> costs, rewards;
        lua_Integer goldCost = 0;
        lua_Integer goldReward = 0;
        if (useSelectedItem)
        {
            if (!state->typedItemUse.id)
                return refused("invalid_context");
            goldReward = lua_tointegerx(L, 2, &valid);
            if (!valid || goldReward < 0 || !ReadQuestItems(L, 1, rewards))
                return refused("invalid");
        }
        else
        {
            goldCost = lua_tointegerx(L, 3, &valid);
            if (!valid || goldCost < 0 || !ReadQuestItems(L, 1, costs) || !ReadQuestItems(L, 2, rewards))
                return refused("invalid");
        }

        const auto entity = quest.GetCurrentCharacter();
        const auto pid = ecs::PlayerRuntime::GetPlayerID(entity);
        const auto map = ecs::PlayerRuntime::GetMapIndex(entity);
        const auto dungeon = inDungeon ? CDungeonManager::instance().FindByMapIndex(map) : entt::null;
        auto dungeonAction = inDungeon ? AcquireTypedDungeonAction(dungeon) : nullptr;
        if (inDungeon && !dungeonAction)
            return refused("unavailable");
        const auto target = useSelectedItem && dungeonAction ? quest.GetCurrentNPCEntity() : entt::null;
        const auto result = RequestTypedQuestItemExchange(entity, std::move(costs), std::move(rewards),
            static_cast<uint64_t>(goldCost),
            [entity, pid, coroutine = L, map, dungeon, target, dungeonAction = std::move(dungeonAction)](bool ok) {
                if (ok && dungeonAction && (!g_registry.valid(entity) ||
                    ecs::PlayerRuntime::GetMapIndex(entity) != map ||
                    CDungeonManager::instance().FindByMapIndex(map) != dungeon))
                    ok = false;
                if (ok && target != entt::null && (!g_registry.valid(target) || CombatSystem::IsDead(target) ||
                    ecs::PlayerRuntime::GetMapIndex(target) != map || !ecs::PlayerRuntime::SetQuestNPC(entity, target)))
                    ok = false;
                CQuestManager::instance().ResumeTypedItems(pid, coroutine, ok);
            },
            useSelectedItem ? state->typedItemUse : TypedItemUseSource{}, static_cast<uint64_t>(goldReward),
            !useSelectedItem && IncludeSpecialInventory(L));
        if (!result)
        {
            switch (result.error())
            {
            case item::ItemMissionError::Requirements: return refused("materials");
            case item::ItemMissionError::NoMoney: return refused("yang");
            case item::ItemMissionError::NoRoom: return refused("inventory_full");
            case item::ItemMissionError::Limit:
                return refused(useSelectedItem && goldReward > 0 ? "yang_limit" : "unavailable");
            default: return refused("unavailable");
            }
        }
        if (useSelectedItem)
            state->typedItemUse = {};
        lua_settop(L, 0);
        lua_pushliteral(L, "typed_items");
        return lua_yieldk(L, 1, 0, [](lua_State* thread, int, lua_KContext) { return lua_gettop(thread); });
    }
    ALUA(pc_typed_mission_status)
    {
        const char* code=lua_tostring(L,1);lua_pushinteger(L,code?TypedMissionQuestStatus(CQuestManager::instance().GetCurrentCharacter(),code):-1);return 1;
    }
    ALUA(pc_typed_mission_action)
    {
        auto& q=CQuestManager::instance();auto* state=q.GetCurrentState();
        const char* code=lua_tostring(L,1);const char* action=lua_tostring(L,2);
        const unsigned command=!action?0:!std::strcmp(action,"start")?2:!std::strcmp(action,"claim")?3:!std::strcmp(action,"talk")?4:!std::strcmp(action,"abandon")?5:0;
        if(!code || !command || !lua_isyieldable(L) || q.IsInOtherPCBlock() || !state || state->co!=L){lua_pushboolean(L,false);lua_pushliteral(L,"invalid_context");return 2;}
        const auto entity=q.GetCurrentCharacter();const auto pid=ecs::PlayerRuntime::GetPlayerID(entity);
        if(!TypedMissionQuestAction(entity,code,command,[pid,coroutine=L](bool ok){CQuestManager::instance().ResumeTypedItems(pid,coroutine,ok);})) {lua_pushboolean(L,false);lua_pushliteral(L,"unavailable");return 2;}
        lua_settop(L,0);lua_pushliteral(L,"typed_items");
        return lua_yieldk(L,1,0,[](lua_State* thread,int,lua_KContext){return lua_gettop(thread);});
    }

    // Exchange inventory requirements, or redeem exactly the item used to start this handler.
    ALUA(pc_typed_item_exchange) { return RunTypedItemExchange(L, false); }
    ALUA(pc_typed_item_redeem) { return RunTypedItemExchange(L, true); }
    ALUA(pc_typed_dungeon_item_exchange) { return RunTypedItemExchange(L, false, true); }
    ALUA(pc_typed_dungeon_item_redeem) { return RunTypedItemExchange(L, true, true); }

	void RegisterPCFunctionTable()
	{
		luaL_reg pc_functions[] =
		{
			{ "get_player_id",	pc_get_player_id	},
			{ "get_account_id", pc_get_account_id	},
			{ "get_account",	pc_get_account		},
			{ "get_level",		pc_get_level		},
			{ "set_level",		pc_set_level		},
			{ "get_next_exp",		pc_get_next_exp		},
			{ "get_exp",		pc_get_exp		},
			{ "get_job",		pc_get_job		},
			{ "get_race",		pc_get_race		},
			{ "change_sex",		pc_change_sex	},
			{ "typed_change_sex",	pc_typed_change_sex },
			{ "gethp",			pc_get_hp		},
			{ "get_hp",			pc_get_hp		},
			{ "getmaxhp",		pc_get_max_hp		},
			{ "get_max_hp",		pc_get_max_hp		},
			{ "getsp",			pc_get_sp		},
			{ "get_sp",			pc_get_sp		},
			{ "getmaxsp",		pc_get_max_sp		},
			{ "get_max_sp",		pc_get_max_sp		},
			{ "change_sp",		pc_change_sp		},
			{ "getmoney",		pc_get_money		},
			{ "get_money",		pc_get_money		},


			{ "get_real_alignment",	pc_get_real_alignment	},
			{ "get_alignment",		pc_get_alignment	},
			{ "getgold",		pc_get_money		},
			{ "get_gold",		pc_get_money		},
			{ "changegold",		pc_change_money		},
			{ "changemoney",		pc_change_money		},
			{ "changealignment",	pc_change_alignment	},
			{ "change_gold",		pc_change_money		},
			{ "change_money",		pc_change_money		},
			{ "change_alignment",	pc_change_alignment	},
			{ "getname",		pc_get_name		},
			{ "get_name",		pc_get_name		},
			{ "get_vid",		pc_get_vid		},
			{ "getplaytime",		pc_get_playtime		},
			{ "get_playtime",		pc_get_playtime		},
			{ "getleadership",		pc_get_leadership	},
			{ "get_leadership",		pc_get_leadership	},
			{ "getqf",			pc_get_quest_flag	},
			{ "setqf",			pc_set_quest_flag	},
			{ "delqf",			pc_del_quest_flag	},
			{ "getf",			pc_get_another_quest_flag},
			{ "setf",			pc_set_another_quest_flag},
			{ "get_x",			pc_get_x		},
			{ "get_y",			pc_get_y		},
			{ "getx",			pc_get_x		},
			{ "gety",			pc_get_y		},
			{ "get_local_x",		pc_get_local_x		},
			{ "get_local_y",		pc_get_local_y		},
			{ "getcurrentmapindex",	pc_get_current_map_index},
			{ "get_map_index",		pc_get_current_map_index},
			{ "give_exp",		pc_give_exp		},
			{ "give_exp_perc",		pc_give_exp_perc	},
			{ "give_exp2",		pc_give_exp2		},

#ifdef ENABLE_DICE_SYSTEM

#endif

			{ "give_gold",		pc_give_gold		},
			{ "typed_item_count", pc_typed_item_count },
            { "typed_pet_egg_key", pc_typed_pet_egg_key },
            { "typed_has_equipped", pc_typed_has_equipped },
            { "typed_item_grant", pc_typed_item_grant },
            { "typed_item_exchange", pc_typed_item_exchange },
            { "typed_item_redeem", pc_typed_item_redeem },
            { "typed_dungeon_item_exchange", pc_typed_dungeon_item_exchange },
            { "typed_dungeon_item_redeem", pc_typed_dungeon_item_redeem },
            { "typed_mission_status", pc_typed_mission_status },
            { "typed_mission_action", pc_typed_mission_action },


			{ "reset_point",		pc_reset_point		},
			{ "has_guild",		pc_hasguild		},
			{ "hasguild",		pc_hasguild		},
			{ "get_guild",		pc_getguild		},
			{ "getguild",		pc_getguild		},
			{ "isguildmaster",		pc_isguildmaster	},
			{ "is_guild_master",	pc_isguildmaster	},
			{ "destroy_guild",		pc_destroy_guild	},
			{ "remove_from_guild",	pc_remove_from_guild	},
			{ "in_dungeon",		pc_in_dungeon		},
			{ "getempire",		pc_get_empire		},
			{ "get_empire",		pc_get_empire		},
			{ "get_skill_group",	pc_get_skillgroup	},
			{ "set_skill_group",	pc_set_skillgroup	},
			{ "warp",			pc_warp			},
			{ "warp_local",		pc_warp_local		},
			{ "warp_exit",		pc_warp_exit		},
			{ "set_warp_location",	pc_set_warp_location	},
			{ "set_warp_location_local",pc_set_warp_location_local },
			{ "get_start_location",	pc_get_start_location	},
			{ "has_master_skill",	pc_has_master_skill	},
			{ "set_part",		pc_set_part		},
			{ "get_part",		pc_get_part		},
			{ "is_polymorphed",		pc_is_polymorphed	},
			{ "remove_polymorph",	pc_remove_polymorph	},
			{ "is_mount",		pc_is_mount		},
			{ "polymorph",		pc_polymorph		},
			{ "mount",			pc_mount		},
			{ "mount_bonus",	pc_mount_bonus	},
			{ "unmount",		pc_unmount		},
			{ "warp_to_guild_war_observer_position", pc_warp_to_guild_war_observer_position	},

			{ "get_warrior_expansion_skills", pc_get_warrior_expansion_skills },
            { "learn_grand_master_skill", pc_learn_grand_master_skill	},
			{ "is_skill_book_no_delay",	pc_is_skill_book_no_delay},
			{ "remove_skill_book_no_delay",	pc_remove_skill_book_no_delay},

			{ "get_horse_level",	pc_get_horse_level	}, // TO BE DELETED XXX
			{ "is_horse_alive",		pc_is_horse_alive	}, // TO BE DELETED XXX
			{ "revive_horse",		pc_revive_horse		}, // TO BE DELETED XXX
			{ "get_war_map",		pc_get_war_map		},
			{ "get_skill_level",	pc_get_skill_level	},
			{ "aggregate_monster",	pc_aggregate_monster	},
			{ "forget_my_attacker",	pc_forget_my_attacker	},
			{ "pc_attract_ranger",	pc_attract_ranger	},
			{ "select",			pc_select_vid		},
			{ "get_sex",		pc_get_sex		},
			{ "is_married",		pc_is_married		},
			{ "is_engaged",		pc_is_engaged		},
			{ "is_engaged_or_married",	pc_is_engaged_or_married},
			{ "is_gm",			pc_is_gm		},
			{ "get_gm_level",		pc_get_gm_level		},
			{ "mining",			pc_mining		},

			// RESET_ONE_SKILL
			{ "clear_one_skill",        pc_clear_one_skill      },
			// END_RESET_ONE_SKILL

			{ "clear_skill",                pc_clear_skill          },
			{ "clear_sub_skill",    pc_clear_sub_skill      },
			{ "set_skill_point",    pc_set_skill_point      },

			{ "is_clear_skill_group",	pc_is_clear_skill_group		},

			{ "save_exit_location",		pc_save_exit_location		},
			{ "teleport",				pc_teleport },

			{ "set_skill_level",        pc_set_skill_level      },

            { "get_premium_remain_sec", pc_get_premium_remain_sec },

			{ "send_block_mode",		pc_send_block_mode	},

			{ "change_empire",			pc_change_empire	},
			{ "typed_change_empire",	pc_typed_change_empire },
			{ "get_change_empire_count",	pc_get_change_empire_count	},
			{ "set_change_empire_count",	pc_set_change_empire_count	},

			{ "change_name",			pc_change_name },
			{ "typed_change_name",	pc_typed_change_name },
			{ "typed_change_guild_master", pc_typed_change_guild_master },
			{ "typed_reset_dungeon_cooldown", pc_typed_reset_dungeon_cooldown },
			{ "typed_reset_level", pc_typed_reset_level },
			{ "typed_perfect_soul_stone", pc_typed_perfect_soul_stone },

			{ "is_dead",				pc_is_dead	},

			{ "reset_status",		pc_reset_status	},
			{ "get_ht",				pc_get_ht	},
			{ "set_ht",				pc_set_ht	},
			{ "get_iq",				pc_get_iq	},
			{ "set_iq",				pc_set_iq	},
			{ "get_st",				pc_get_st	},
			{ "set_st",				pc_set_st	},
			{ "get_dx",				pc_get_dx	},
			{ "set_dx",				pc_set_dx	},

			{ "is_near_vid",		pc_is_near_vid	},

			{ "get_empty_inventory_count",	pc_get_empty_inventory_count	},

			{ "get_logoff_interval",	pc_get_logoff_interval	},

			{ "is_riding",			pc_is_riding	},

			{ "can_warp",			pc_can_warp		},

			{ "dec_skill_point",	pc_dec_skill_point	},
			{ "get_skill_point",	pc_get_skill_point	},

			{ "get_channel_id",		pc_get_channel_id	},

			{ "charge_cash",		pc_charge_cash		},

			{ "get_informer_type",	pc_get_informer_type	},
			{ "get_informer_item",  pc_get_informer_item	},



#ifdef ENABLE_NEWSTUFF
			//pc.set_race0(race=[0. Warrior, 1. Ninja, 2. Sura, 3. Shaman, 4. Lycan])
			{ "set_race0",			pc_set_race0			},
			//if pc.delf("game_option", "block_cocks") then syschat("now you are unsafe") end
			{ "delf",				pc_del_another_quest_flag},	// delete quest flag [return lua boolean: successfulness]
			//pc.pointchange(uint type, int amount, bool bAmount, bool bBroadcast)
			{ "pointchange",		pc_pointchange		},	// [return nothing]
			//pc.pullmob()
			{ "pullmob",			pc_pullmob			},	// [return nothing]
			//pc.select_pid(pc.get_player_id())
			{ "select_pid",			pc_select_pid		},	// [return lua number: old pid]
			//pc.select renamed in pc.select_vid
			{ "select_vid",			pc_select_vid		},	// [return lua number: old vid]
			//pc.set_level0(level)
			{ "set_level0",			pc_set_level0		},	// [return nothing]
			//pc.set_gm_level(); instead of `/reload a` (note: this only refresh gm privileges if you already are gm on common.gm[host|list])
			{ "set_gm_level",		pc_set_gm_level		},	// [return nothing]
			//is_flags (void) [return lua boolean]
			{ "is_flag_fire",			pc_if_fire			},
			{ "is_flag_invisible",		pc_if_invisible		},
			{ "is_flag_poison",			pc_if_poison		},
			{ "is_flag_slow",			pc_if_slow			},
			{ "is_flag_stun",			pc_if_stun			},
			//set_flags (bool) [return nothing]
			{ "set_flag_fire",			pc_sf_fire			},
			{ "set_flag_invisible",		pc_sf_invisible		},
			{ "set_flag_poison",		pc_sf_poison		},
			{ "set_flag_slow",			pc_sf_slow			},
			{ "set_flag_stun",			pc_sf_stun			},
			//pc.set_flag_kill(char_name) [return nothing]
			{ "set_flag_kill",			pc_sf_kill			},
			//pc.set_flag_dead()
			{ "set_flag_dead",			pc_sf_dead			},	// kill themselves [return nothing]
			//pc.get_exp_level(level) [return: lua number]
			{ "get_exp_level",		pc_get_exp_level	},	// get needed exp for <level> level [return: lua number]
			//pc.get_exp_level(level, perc)
			{ "get_exp_level0",		pc_get_exp_level0	},	// get needed exp for <level> level / 100 * perc [return: lua number]
			//pc.set_max_health()
			{ "set_max_health",		pc_set_max_health	},	// [return nothing]
			{ "get_ip0",			pc_get_ip0			},	// [return lua string]
			{ "get_client_version0",	pc_get_client_version0	},	// get player client version [return lua string]
			{ "dc_delayed0",		pc_dc_delayed0	},	// crash a player after x secs [return lua boolean: successfulness]
			{ "dc_direct0",			pc_dc_direct0	},	// crash the <nick> player [return nothing]
			{ "is_trade0",			pc_is_trade0	},	// get if player is trading [return lua boolean]
			{ "is_busy0",			pc_is_busy0		},	// get if player is "busy" (if trade, safebox, npc/myshop, cube are open) [return lua boolean]
			{ "is_arena0",			pc_is_arena0		},	// get if player is in arena [return lua boolean]
			{ "is_arena_observer0",	pc_is_arena_observer0		},	// get if player is in arena as observer [return lua boolean]
			{ "is_available0",		pc_is_available0	},	// [return lua boolean]
			{ "is_pvp0",			pc_is_pvp0			},	// [return lua boolean]
#endif
#ifdef ENABLE_PC_OPENSHOP
			//pc.open_shop0(id_shop)
			{ "open_shop0",			pc_open_shop0		},	// buy/sell won't work on it [return nothing]
#endif
#ifdef ENABLE_NEWGUILDMAKE
			//pc.make_guild0(guild_name)
			{ "make_guild0",			pc_make_guild0	},	// it returns few state values which you can manage via lua [return lua number]
#endif


#ifdef ENABLE_MULTI_LANGUAGE
			{ "get_language", 								pc_get_language	},
#endif

#ifdef __ENABLE_BLOCK_EXP__
			{ "Block_Exp",			_Block_Exp	},
			{ "Unblock_Exp",		_Unblock_Exp	},
#endif

#ifdef ENABLE_GAYA_SYSTEM
			{ "get_gaya",			pc_get_gaya },
			{ "change_gaya",		pc_change_gaya },
			{ "give_gaya",			pc_give_gaya },
#endif
#ifdef __HIDE_COSTUME_SYSTEM__
			{ "hide_costume",		pc_hide_costume },
#endif

#if defined(__DUNGEON_INFO_SYSTEM__)
			{ "get_last_damage", pc_get_last_damage },
#endif
#ifdef ENABLE_RANKING
			{"get_rank_dungeon", pc_get_rank_dungeon},
			{"set_rank_dungeon", pc_set_rank_dungeon},
#endif
#ifdef ENABLE_BLOCK_MULTIFARM
			{"can_drop", pc_can_drop},
#endif
#ifdef ENABLE_BIOLOGIST_UI
			{"open_biologist_change", pc_open_biologist_change},
#endif
#ifdef ENABLE_VOTE_FOR_BONUS
			{"can_get_bonus_vote", pc_can_get_bonus_vote},
			{"set_bonus_for_vote", pc_set_bonus_for_vote},
#endif
#ifdef ENABLE_VOTE4BUFF
			{ "get_vote_coin", pc_get_vote_coin },
			{ "set_vote_coin", pc_set_vote_coin },
#endif
			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("pc", pc_functions);
	}
};

