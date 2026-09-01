#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/CombatSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include "ecs/systems/ActivitySystem.hpp"
#include "ecs/systems/SkillSystem.hpp"
#include "ecs/AIHelpers.hpp"

#include "config.h"
#include "questmanager.h"
#include "sectree_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "affect.h"
#include "item.h"
#include "item_manager.h"
#include "guild_manager.h"
#include "war_map.h"
#include "start_position.h"
#include "marriage.h"
#include "mining.h"
#include "p2p.h"
#include "polymorph.h"
#include "desc_client.h"
#include "messenger_manager.h"
#include "log.h"
#include "utils.h"
#include "unique_item.h"
#include "mob_manager.h"
#include "dungeon.h"
#include "ecs/quest_helpers.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/events.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/components/appearance_components.hpp"


#ifdef ENABLE_NEWSTUFF
#include "pvp.h"
#endif

#ifdef ENABLE_BATTLE_PASS
#include "battle_pass.h"
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

const int ITEM_BROKEN_METIN_VNUM = 28960;

// #define ENABLE_LOCALECHECK_CHANGENAME
// #define ENABLE_PC_OPENSHOP

#ifdef ENABLE_PC_OPENSHOP
#include "shop.h"
#include "shop_manager.h"
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
			if (skills->levels != nullptr)
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
			for (const CAffect* affect : affects->affects)
			{
				if (affect && affect->dwType == AFFECT_SKILL_NO_BOOK_DELAY)
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

	ALUA(pc_learn_grand_master_skill)
	{
        // migrated from CHARACTER::LearnGrandMasterSkill
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!lua_isnumber(L, 1))
		{
			sys_err("wrong skill index");
			return 0;
		}

		lua_pushboolean(L, ch->LearnGrandMasterSkill(static_cast<uint32_t>(lua_tonumber(L, 1))));
		return 1;
	}

    ALUA(pc_set_warp_location)
    {
        // migrated from CHARACTER::SetWarpLocation
        // DUAL-PATH: ECS component update + legacy warp packet
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
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
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* warpPos = ECS_TryGet<ecs::WarpPosition>(e))
        {
            warpPos->x = x;
            warpPos->y = y;
            warpPos->mapIndex = mapIndex;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (ch)
            ch->SetWarpLocation(mapIndex, x, y);
        return 0;
    }

    ALUA(pc_set_warp_location_local)
    {
        // migrated from CHARACTER::SetWarpLocation
        // DUAL-PATH: ECS component update + legacy warp packet
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
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
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* warpPos = ECS_TryGet<ecs::WarpPosition>(e))
        {
            warpPos->x = region->sx + x;
            warpPos->y = region->sy + y;
            warpPos->mapIndex = lMapIndex;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (ch)
            ch->SetWarpLocation(lMapIndex, x, y);
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
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
        {
            lua_pushboolean(L, false);
            return 1;
        }
        int32_t map_index = 0;
        if (lua_isnumber(L, 3))
            map_index = static_cast<int32_t>(lua_tonumber(L, 3));
        if (!ch)
        {
            lua_pushboolean(L, false);
            return 1;
        }
        if ( ch->IsHack() )
        {
            lua_pushboolean(L, false);
            return 1;
        }
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* warpPos = ECS_TryGet<ecs::WarpPosition>(e))
        {
            warpPos->x = static_cast<int32_t>(lua_tonumber(L, 1));
            warpPos->y = static_cast<int32_t>(lua_tonumber(L, 2));
            warpPos->mapIndex = map_index ? map_index : ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(ch));
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if ( test_server )
            ecs::ChatSystem::Send(AIHelpers::EcsOf(ch),  CHAT_TYPE_INFO, "pc_warp %d %d %d",static_cast<int>(lua_tonumber(L, 1)), static_cast<int>(lua_tonumber(L, 2)),map_index );
        ecs::MovementSystem::WarpSet(AIHelpers::EcsOf(ch), static_cast<int32_t>(lua_tonumber(L, 1)), static_cast<int32_t>(lua_tonumber(L, 2)), map_index);
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
			lua_pushboolean(L, dungeon->dungeon ? 1 : 0);
			return 1;
		}
		lua_pushboolean(L, 0);
		return 1;
	}

	ALUA(pc_hasguild)
	{
		// migrated from CHARACTER::GetGuild
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* guild = ECS_TryGet<ecs::GuildMembership>(e))
		{
			lua_pushboolean(L, guild->guild ? 1 : 0);
			return 1;
		}
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
		if (const auto* guild = ECS_TryGet<ecs::GuildMembership>(e))
		{
			const auto* playerId = ECS_TryGet<ecs::PlayerID>(e);
			lua_pushboolean(L, (guild->guild && playerId && playerId->pid == guild->guild->GetMasterPID()) ? 1 : 0);
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
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 0;
        if ( ch->IsHack() )
            return 0;
        ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "war.is_war_member", 0);
        ch->SaveExitLocation();
        if (auto* warpPos = ECS_TryGet<ecs::WarpPosition>(e))
        {
            warpPos->x = pos.x;
            warpPos->y = pos.y;
            warpPos->mapIndex = dwMapIndex;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        ecs::MovementSystem::WarpSet(AIHelpers::EcsOf(ch), pos.x, pos.y, dwMapIndex);
        return 0;
    }

	ALUA(pc_give_item_from_special_item_group)
	{
		luaL_checknumber(L, 1);

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (chEntity == entt::null || !g_registry.valid(chEntity))
			return 0;

		const uint32_t dwGroupVnum = static_cast<uint32_t>(lua_tonumber(L, 1));
		const ItemSystem::SpecialItemGroupResult result =
			ItemSystem::GiveItemFromSpecialItemGroup(chEntity, dwGroupVnum);
#ifdef TEXTS_IMPROVEMENT
		for (int i = 0; i < result.count; ++i) {
			if (result.itemEntities[i] == entt::null) {
				ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 102, "%d", result.itemCounts[i]);
			}
		}
#endif
		return 0;
	}

	ALUA(pc_enough_inventory)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (!lua_isnumber(L, 1))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		const uint32_t itemVnum = static_cast<uint32_t>(lua_tonumber(L, 1));
		lua_pushboolean(L, ItemSystem::HasInventorySpaceForItemVnum(chEntity, itemVnum));
		return 1;
	}

	ALUA(pc_give_item)
	{
		PC* pPC = CQuestManager::instance().GetCurrentPC();
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (!lua_isstring(L, 1) || !(lua_isstring(L, 2)||lua_isnumber(L, 2)))
		{
			sys_err("QUEST : wrong argument");
			return 0;
		}

		uint32_t dwVnum;

		if (lua_isnumber(L,2))
			dwVnum = static_cast<uint32_t>(lua_tonumber(L, 2));
		else if (!ITEM_MANAGER::instance().GetVnum(lua_tostring(L, 2), dwVnum))
		{
			sys_err("QUEST Make item call error : wrong item name : {}", lua_tostring(L,1));
			return 0;
		}

		int icount = 1;

		if (lua_isnumber(L, 3) && lua_tonumber(L, 3) > 0)
		{
			icount = static_cast<int>(rint(lua_tonumber(L, 3)));

			if (icount <= 0)
			{
				sys_err("QUEST Make item call error : wrong item count : {:g}", lua_tonumber(L, 2));
				return 0;
			}
		}

		pPC->GiveItem(lua_tostring(L, 1), dwVnum, icount);

		LogManager::instance().QuestRewardLog(pPC->GetCurrentQuestName().c_str(), ecs::PlayerRuntime::GetPlayerID(chEntity), ecs::PointSystem::GetLevel(chEntity), dwVnum, icount);
		return 0;
	}

	ALUA(pc_give_or_drop_item)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (!lua_isstring(L, 1) && !lua_isnumber(L, 1))
		{
			sys_err("QUEST Make item call error : wrong argument");
			lua_pushnumber (L, 0);
			return 1;
		}

		uint32_t dwVnum;

		if (lua_isnumber(L, 1)) // ��ȣ�ΰ�� ��ȣ�� �ش�.
		{
			dwVnum = static_cast<uint32_t>(lua_tonumber(L, 1));
		}
		else if (!ITEM_MANAGER::instance().GetVnum(lua_tostring(L, 1), dwVnum))
		{
			sys_err("QUEST Make item call error : wrong item name : {}", lua_tostring(L,1));
			lua_pushnumber (L, 0);

			return 1;
		}

		int icount = 1;
		if (lua_isnumber(L,2) && lua_tonumber(L,2)>0)
		{
			icount = (int)rint(lua_tonumber(L,2));
			if (icount<=0)
			{
				sys_err("QUEST Make item call error : wrong item count : {:g}", lua_tonumber(L,2));
				lua_pushnumber (L, 0);
				return 1;
			}
		}

		LOG_INFO("QUEST [REWARD] item {} to {}", lua_tostring(L, 1), ecs::PlayerRuntime::GetName(chEntity).data());

		PC* pPC = CQuestManager::instance().GetCurrentPC();

		LogManager::instance().QuestRewardLog(pPC->GetCurrentQuestName().c_str(), ecs::PlayerRuntime::GetPlayerID(chEntity), ecs::PointSystem::GetLevel(chEntity), dwVnum, icount);

		const entt::entity item = ItemSystem::AutoGiveItemEcs(chEntity, dwVnum, icount);
		const uint32_t itemId = ItemSystem::GetItemID(item);

		if ( dwVnum >= 80003 && dwVnum <= 80007 && itemId != 0 )
		{
			LogManager::instance().GoldBarLog(ecs::PlayerRuntime::GetPlayerID(chEntity), itemId, QUEST, "quest: give_item2");
		}

		if (item != entt::null)
			lua_pushnumber (L, itemId);
		else
			lua_pushnumber (L, 0);
		return 1;
	}

#ifdef ENABLE_DICE_SYSTEM
	ALUA(pc_give_or_drop_item_with_dice)
	{
		if (!lua_isstring(L, 1) && !lua_isnumber(L, 1))
		{
			sys_err("QUEST Make item call error : wrong argument");
			lua_pushnumber (L, 0);
			return 1;
		}

		uint32_t dwVnum;

		if (lua_isnumber(L, 1)) // ��ȣ�ΰ�� ��ȣ�� �ش�.
		{
			dwVnum = static_cast<uint32_t>(lua_tonumber(L, 1));
		}
		else if (!ITEM_MANAGER::instance().GetVnum(lua_tostring(L, 1), dwVnum))
		{
			sys_err("QUEST Make item call error : wrong item name : {}", lua_tostring(L,1));
			lua_pushnumber (L, 0);

			return 1;
		}

		int icount = 1;
		if (lua_isnumber(L,2) && lua_tonumber(L,2)>0)
		{
			icount = static_cast<int>(rint(lua_tonumber(L, 2)));
			if (icount<=0)
			{
				sys_err("QUEST Make item call error : wrong item count : {:g}", lua_tonumber(L,2));
				lua_pushnumber(L, 0);
				return 1;
			}
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		const entt::entity item = ItemSystem::CreateItemEcs(dwVnum, icount);
		if (chEntity == entt::null || !g_registry.valid(chEntity) ||
			!ItemSystem::IsValidItem(item))
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		entt::entity recipient = chEntity;
		if (ecs::SocialSystem::GetParty(chEntity))
			recipient = ItemSystem::RollPartyDropOwnership(item, chEntity);
		ItemSystem::AutoGiveItem(recipient, item);

		LOG_INFO("QUEST [REWARD] item {} to {}", lua_tostring(L, 1), ecs::PlayerRuntime::GetName(recipient).data());

		LogManager::instance().QuestRewardLog(CQuestManager::instance().GetCurrentPC()->GetCurrentQuestName().c_str(), ecs::PlayerRuntime::GetPlayerID(recipient), ecs::PointSystem::GetLevel(recipient), dwVnum, icount);

		lua_pushnumber(L, ItemSystem::GetItemID(item));
		return 1;
	}
#endif

	ALUA(pc_give_or_drop_item_and_select)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (!lua_isstring(L, 1) && !lua_isnumber(L, 1))
		{
			sys_err("QUEST Make item call error : wrong argument");
			return 0;
		}

		uint32_t dwVnum;

		if (lua_isnumber(L, 1)) // ��ȣ�ΰ�� ��ȣ�� �ش�.
		{
			dwVnum = static_cast<uint32_t>(lua_tonumber(L, 1));
		}
		else if (!ITEM_MANAGER::instance().GetVnum(lua_tostring(L, 1), dwVnum))
		{
			sys_err("QUEST Make item call error : wrong item name : {}", lua_tostring(L,1));
			return 0;
		}

		int icount = 1;
		if (lua_isnumber(L,2) && lua_tonumber(L,2)>0)
		{
			icount = static_cast<int>(rint(lua_tonumber(L, 2)));
			if (icount<=0)
			{
				sys_err("QUEST Make item call error : wrong item count : {:g}", lua_tonumber(L,2));
				return 0;
			}
		}

		LOG_INFO("QUEST [REWARD] item {} to {}", lua_tostring(L, 1), ecs::PlayerRuntime::GetName(chEntity).data());

		PC* pPC = CQuestManager::instance().GetCurrentPC();

		LogManager::instance().QuestRewardLog(pPC->GetCurrentQuestName().c_str(), ecs::PlayerRuntime::GetPlayerID(chEntity), ecs::PointSystem::GetLevel(chEntity), dwVnum, icount);

		const entt::entity item = ItemSystem::AutoGiveItemEcs(chEntity, dwVnum, icount);
		const uint32_t itemId = ItemSystem::GetItemID(item);

		if (item != entt::null)
			CQuestManager::Instance().SetCurrentItem(item);

		if ( dwVnum >= 80003 && dwVnum <= 80007 && itemId != 0 )
		{
			LogManager::instance().GoldBarLog(ecs::PlayerRuntime::GetPlayerID(chEntity), itemId, QUEST, "quest: give_item2");
		}

		return 0;
	}

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

	ALUA(pc_count_item)
	{
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		if (lua_isnumber(L, -1))
			lua_pushnumber(L, ItemSystem::CountItem(character, static_cast<uint32_t>(lua_tonumber(L, -1))));
		else if (lua_isstring(L, -1))
		{
			uint32_t item_vnum;

			if (!ITEM_MANAGER::instance().GetVnum(lua_tostring(L,1), item_vnum))
			{
				sys_err("QUEST count_item call error : wrong item name : {}", lua_tostring(L,1));
				lua_pushnumber(L, 0);
			}
			else
			{
				lua_pushnumber(L, ItemSystem::CountItem(character, item_vnum));
			}
		}
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(pc_remove_item)
	{
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		if (lua_gettop(L) == 1)
		{
			uint32_t item_vnum;

			if (lua_isnumber(L,1))
			{
				item_vnum = (uint32_t)lua_tonumber(L, 1);
			}
			else if (lua_isstring(L,1))
			{
				if (!ITEM_MANAGER::instance().GetVnum(lua_tostring(L,1), item_vnum))
				{
					sys_err("QUEST remove_item call error : wrong item name : {}", lua_tostring(L,1));
					return 0;
				}
			}
			else
			{
				sys_err("QUEST remove_item wrong argument");
				return 0;
			}

			LOG_INFO("QUEST remove a item vnum {} of {}[{}]", item_vnum, ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetPlayerID(character));
			ItemSystem::RemoveSpecifyItemEcs(character, item_vnum);
		}
		else if (lua_gettop(L) == 2)
		{
			uint32_t item_vnum;

			if (lua_isnumber(L, 1))
			{
				item_vnum = static_cast<uint32_t>(lua_tonumber(L, 1));
			}
			else if (lua_isstring(L, 1))
			{
				if (!ITEM_MANAGER::instance().GetVnum(lua_tostring(L,1), item_vnum))
				{
					sys_err("QUEST remove_item call error : wrong item name : {}", lua_tostring(L,1));
					return 0;
				}
			}
			else
			{
				sys_err("QUEST remove_item wrong argument");
				return 0;
			}

			int32_t item_count = static_cast<int32_t>(lua_tonumber(L, 2));
			LOG_INFO("QUEST remove items(vnum {}) count {} of {}[{}]", item_vnum, item_count, ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetPlayerID(character));

			if (item_count > 0)
				ItemSystem::RemoveSpecifyItemEcs(character, item_vnum, static_cast<uint32_t>(item_count));
		}
		return 0;
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
		CQuestManager::instance().GetCurrentCharacterPtr()->ResetPoint(ecs::PointSystem::GetLevel(AIHelpers::EcsOf(CQuestManager::instance().GetCurrentCharacterPtr())));
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
        // migrated from CHARACTER::ChangeSex
        // DUAL-PATH: ECS update + legacy call during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (e == entt::null || !g_registry.valid(e)) {
            lua_pushnumber(L, 0);
            return 1;
        }
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(e, WEAR_COSTUME_BODY))) {
            lua_pushnumber(L, 2);
            return 1;
        }
        if (ItemSystem::IsValidItem(ItemSystem::GetWearItem(e, WEAR_COSTUME_HAIR))) {
            lua_pushnumber(L, 3);
            return 1;
        }
        const int result = ecs::PlayerRuntime::ChangeSex(e) ? 1 : 0;
        lua_pushnumber(L, result);
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
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 0;
        LOG_INFO("QUEST [LEVEL] {} jumpint to level {}", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), (int)rint(lua_tonumber(L,1)));
        PC* pPC = CQuestManager::instance().GetCurrentPC();
        LogManager::instance().QuestRewardLog(pPC->GetCurrentQuestName().c_str(), (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))), (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch))), newLevel, 0);
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_SKILL, newLevel - (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch))));
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_SUB_SKILL, newLevel < 10 ? 0 : newLevel - MAX((ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch))), 9));
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_STAT, ((MINMAX(1, newLevel, gPlayerMaxLevel) - (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)))) * 3) + ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_LEVEL_STEP));
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_LEVEL, newLevel - (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch))));
        ch->SetRandomHP((newLevel - 1) * number(JobInitialPoints[ch->GetJob()].hp_per_lv_begin, JobInitialPoints[ch->GetJob()].hp_per_lv_end));
        ch->SetRandomSP((newLevel - 1) * number(JobInitialPoints[ch->GetJob()].sp_per_lv_begin, JobInitialPoints[ch->GetJob()].sp_per_lv_end));
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_HP, ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) - ch->GetHP());
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_SP, ecs::PointSystem::GetMaxSP(AIHelpers::EcsOf(ch)) - ch->GetSP());
        ch->ComputePoints();
        NetworkSyncSystem::PointsPacket(AIHelpers::EcsOf(ch));
        ch->SkillLevelPacket();
        if (auto* lv = ECS_TryGet<ecs::LevelComponent>(e))
            lv->value = (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)));
        if (auto* health = ECS_TryGet<ecs::Health>(e))
        {
            health->current = ch->GetHP();
            health->max = ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch));
        }
        if (auto* mana = ECS_TryGet<ecs::Mana>(e))
        {
            mana->current = ch->GetSP();
            mana->max = ecs::PointSystem::GetMaxSP(AIHelpers::EcsOf(ch));
        }
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
        {
            points->base.points[POINT_SKILL] = ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_SKILL);
            points->base.points[POINT_SUB_SKILL] = ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_SUB_SKILL);
            points->base.points[POINT_STAT] = ecs::PointSystem::Get(AIHelpers::EcsOf(ch), POINT_STAT);
        }
        if (e != entt::null && g_registry.valid(e))
        {
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
            g_dispatcher.trigger(ecs::EvLevelUp{e, (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)))});
        }
        return 0;
    }

	ALUA(pc_get_weapon)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const entt::entity item = ItemSystem::GetWearItem(character, WEAR_WEAPON);
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemVnum(item) : 0);
		return 1;
	}

	ALUA(pc_get_armor)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const entt::entity item = ItemSystem::GetWearItem(character, WEAR_BODY);
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemVnum(item) : 0);
		return 1;
	}

	ALUA(pc_get_wear)
	{
		// migrated from CHARACTER::GetWear
		if (!lua_isnumber(L, 1))
		{
			sys_err("QUEST wrong set flag");
			return 0;
		}
		const uint8_t bCell = static_cast<uint8_t>(lua_tonumber(L, 1));
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const entt::entity item = bCell < WEAR_MAX_NUM
			? ItemSystem::GetWearItem(character, bCell)
			: entt::null;
		if (!ItemSystem::IsValidItem(item))
			lua_pushnil(L);
		else
			lua_pushnumber(L, ItemSystem::GetItemVnum(item));
		return 1;
	}

	ALUA(pc_get_money)
	{
		// migrated from CHARACTER::GetGold
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* gold = ECS_TryGet<ecs::GoldAmount>(e))
		{
			lua_pushnumber(L, gold->amount);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushnumber(L, ecs::PointSystem::GetGold(chEntity));
		return 1;
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
        // migrated from CHARACTER::UpdateAlignment
        // DUAL-PATH: ECS update + legacy call during migration window
        const int32_t alignment = static_cast<int32_t>(lua_tonumber(L, 1) * 10);
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (ch)
            ch->UpdateAlignment(alignment);
        if (auto* combat = ECS_TryGet<ecs::CombatStats>(e))
        {
            if (ch)
            {
                combat->alignment = ch->GetAlignment();
                combat->realAlignment = ch->GetRealAlignment();
            }
            else
            {
                combat->alignment += alignment;
            }
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        return 0;
    }

	ALUA(pc_change_money)
	{
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
        pPC->SetFlag(string(sz)+"."+sz2, int(rint(lua_tonumber(L,3))));
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
			lua_pushnumber(L,pPC->GetFlag(string(sz)+"."+sz2));
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
			lua_pushnumber(L,pPC->GetFlag(sz));
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
			lua_pushnumber(L,pPC->GetFlag(pPC->GetCurrentQuestName() + "."+sz));
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
            pPC->SetFlag(sz, int(rint(lua_tonumber(L,2))));
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
            pPC->SetFlag(pPC->GetCurrentQuestName()+"."+sz, int(rint(lua_tonumber(L,2))));
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
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 0;
#ifdef ENABLE_BUG_FIXES
        ch->RemoveGoodAffect();
#endif
        ch->SetSkillGroup((uint8_t) rint(lua_tonumber(L, 1)));
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
        {
            points->base.skill_group = ch->GetSkillGroup();
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
        // migrated from CHARACTER::SetPolymorph
        // DUAL-PATH: ECS PolymorphState clear + legacy call
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* poly = ECS_TryGet<ecs::PolymorphState>(e))
        {
            poly->raceVnum = 0;
            poly->maintainStat = false;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
            sf->isPolymorph = false;
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (ch)
        {
            AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), AFFECT_POLYMORPH);
            ch->SetPolymorph(0);
        }
        return 0;
    }

    ALUA(pc_polymorph)
    {
        // migrated from CHARACTER::SetPolymorph
        // DUAL-PATH: ECS PolymorphState + legacy call
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        uint32_t dwVnum = (uint32_t) lua_tonumber(L, 1);
        int iDuration = (int) lua_tonumber(L, 2);
        if (auto* poly = ECS_TryGet<ecs::PolymorphState>(e))
        {
            poly->raceVnum = dwVnum;
            poly->maintainStat = false;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
            sf->isPolymorph = (dwVnum != 0);
		if (e != entt::null && g_registry.valid(e))
			AffectSystem::AddAffect(e, AFFECT_POLYMORPH, POINT_POLYMORPH, dwVnum, AFF_POLYMORPH, iDuration, 0, true);
        return 0;
    }

	ALUA(pc_is_mount)
	{
		// migrated from CHARACTER::GetMountVnum
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		const auto* sf = ECS_TryGet<ecs::StatusFlags>(e);
		const auto* mount = ECS_TryGet<ecs::MountState>(e);
		if (sf || mount)
		{
			const bool ret = (sf && sf->isMount) || (mount && mount->mountVnum != 0);
			lua_pushboolean(L, ret ? 1 : 0);
			return 1;
		}
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		const bool ret = character != entt::null && g_registry.valid(character) &&
			(MountSystem::GetMountVnum(character) != 0 ||
			 ItemSystem::IsValidItem(ItemSystem::GetWearItem(character, WEAR_COSTUME_MOUNT)));
		lua_pushboolean(L, ret ? 1 : 0);
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
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 0;
#ifdef ENABLE_PVP_ADVANCED
        if ((ch->GetDuel("BlockRide")))
        {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 516, "");
#endif
            lua_pushnumber(L, 0);
            return 1;
        }
#endif
        AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), AFFECT_MOUNT);
        AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), AFFECT_MOUNT_BONUS);
#ifdef ENABLE_NEWSTUFF
        if (g_NoMountAtGuildWar && ch->GetWarMap())
        {
            if (ch->IsRiding())
                ch->StopRiding();
            return 0;
        }
#endif
        if (ch->GetHorse())
            ch->HorseSummon(false);
        if (mount_vnum)
        {
            AffectSystem::AddAffect(AIHelpers::EcsOf(ch), AFFECT_MOUNT, POINT_MOUNT, mount_vnum, AFF_NONE, length, 0, true);
            switch (mount_vnum)
            {
            case 20201:
            case 20202:
            case 20203:
            case 20204:
            case 20213:
            case 20216:
                AffectSystem::AddAffect(AIHelpers::EcsOf(ch), AFFECT_MOUNT, POINT_MOV_SPEED, 30, AFF_NONE, length, 0, true, true);
                break;
            case 20205:
            case 20206:
            case 20207:
            case 20208:
            case 20214:
            case 20217:
                AffectSystem::AddAffect(AIHelpers::EcsOf(ch), AFFECT_MOUNT, POINT_MOV_SPEED, 40, AFF_NONE, length, 0, true, true);
                break;
            case 20209:
            case 20210:
            case 20211:
            case 20212:
            case 20215:
            case 20218:
                AffectSystem::AddAffect(AIHelpers::EcsOf(ch), AFFECT_MOUNT, POINT_MOV_SPEED, 50, AFF_NONE, length, 0, true, true);
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
            sf->isMountActive = (mount_vnum != 0);
            sf->isMount = (mount_vnum != 0);
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
            sf->isMount = false;
        }
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (ch)
        {
            AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), AFFECT_MOUNT);
            AffectSystem::RemoveAffect(AIHelpers::EcsOf(ch), AFFECT_MOUNT_BONUS);
            if (ch->IsHorseRiding())
                ch->StopRiding();
        }
        return 0;
    }

	ALUA(pc_get_horse_level)
	{
        // migrated from CHARACTER::GetHorseLevel
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		lua_pushnumber(L, ch->GetHorseLevel());
		return 1;
	}

	ALUA(pc_get_horse_hp)
	{
        // migrated from CHARACTER::GetHorseHealth
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch->GetHorseLevel())
			lua_pushnumber(L, ch->GetHorseHealth());
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(pc_get_horse_stamina)
	{
        // migrated from CHARACTER::GetHorseStamina
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch->GetHorseLevel())
			lua_pushnumber(L, ch->GetHorseStamina());
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
		auto* ch = ecs::LegacyCharOf(chEntity);
		lua_pushboolean(L, (ch && ch->GetHorseLevel() > 0 && ch->GetHorseHealth() > 0) ? 1 : 0);
		return 1;
	}

	ALUA(pc_revive_horse)
	{
        // migrated from CHARACTER::ReviveHorse
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		ch->ReviveHorse();
		return 0;
	}

	ALUA(pc_have_map_scroll)
	{
        // migrated from CHARACTER::GetInventoryItem
        // DUAL-PATH: legacy only during migration window
		if (!lua_isstring(L, 1))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		const char * szMapName = lua_tostring(L, 1);
		const TMapRegion * region = SECTREE_MANAGER::instance().FindRegionByPartialName(szMapName);

		if (!region)
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		bool bFind = false;
		for (int iCell = 0; iCell < INVENTORY_MAX_NUM; iCell++)
		{
			const entt::entity item = ItemSystem::GetInventoryItem(chEntity, iCell);
			if (!ItemSystem::IsValidItem(item))
				continue;

			if (ItemSystem::GetItemType(item) == ITEM_USE &&
					ItemSystem::GetItemSubType(item) == USE_TALISMAN &&
					(ItemSystem::GetItemValue(item, 0) == 1 || ItemSystem::GetItemValue(item, 0) == 2))
			{
				int x = ItemSystem::GetItemSocket(item, 0);
				int y = ItemSystem::GetItemSocket(item, 1);
				//if ((x-item_x)*(x-item_x)+(y-item_y)*(y-item_y)<r*r)
				if (region->sx <=x && region->sy <= y && x <= region->ex && y <= region->ey)
				{
					bFind = true;
					break;
				}
			}
		}

		lua_pushboolean(L, bFind);
		return 1;
	}

	ALUA(pc_get_war_map)
	{
		// migrated from CHARACTER::GetWarMap
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* dungeon = ECS_TryGet<ecs::DungeonMembership>(e))
		{
			lua_pushnumber(L, dungeon->warMap ? dungeon->warMap->GetMapIndex() : 0);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		lua_pushnumber(L, (ch && ch->GetWarMap()) ? ch->GetWarMap()->GetMapIndex() : 0);
		return 1;
	}

	ALUA(pc_have_pos_scroll)
	{
        // migrated from CHARACTER::GetInventoryItem
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (!lua_isnumber(L,1) || !lua_isnumber(L,2))
		{
			sys_err("invalid x y position");
			lua_pushboolean(L, 0);
			return 1;
		}

		if (!lua_isnumber(L,3))
		{
			sys_err("invalid radius");
			lua_pushboolean(L, 0);
			return 1;
		}

		int x = (int)lua_tonumber(L, 1);
		int y = (int)lua_tonumber(L, 2);
		float r = (float)lua_tonumber(L, 3);

		bool bFind = false;
		for (int iCell = 0; iCell < INVENTORY_MAX_NUM; iCell++)
		{
			const entt::entity item = ItemSystem::GetInventoryItem(chEntity, iCell);
			if (!ItemSystem::IsValidItem(item))
				continue;

			if (ItemSystem::GetItemType(item) == ITEM_USE &&
					ItemSystem::GetItemSubType(item) == USE_TALISMAN &&
					(ItemSystem::GetItemValue(item, 0) == 1 || ItemSystem::GetItemValue(item, 0) == 2))
			{
				int item_x = ItemSystem::GetItemSocket(item, 0);
				int item_y = ItemSystem::GetItemSocket(item, 1);
				if ((x-item_x)*(x-item_x)+(y-item_y)*(y-item_y)<r*r)
				{
					bFind = true;
					break;
				}
			}
		}

		lua_pushboolean(L, bFind);
		return 1;
	}

	ALUA(pc_get_equip_refine_level)
	{
		// migrated from CHARACTER::GetWear
		const int cell = static_cast<int>(lua_tonumber(L, 1));
		if (cell < 0 || cell >= WEAR_MAX_NUM)
		{
			sys_err("invalid wear position {}", cell);
			lua_pushnumber(L, 0);
			return 1;
		}
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const entt::entity item = ItemSystem::GetWearItem(character, cell);
		lua_pushnumber(L, ItemSystem::IsValidItem(item) ? ItemSystem::GetItemRefineLevel(item) : 0);
		return 1;
	}

	ALUA(pc_refine_equip)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			sys_err("invalid argument");
			lua_pushboolean(L, 0);
			return 1;
		}

		const int cell = static_cast<int>(lua_tonumber(L, 1));
		const int levelLimit = static_cast<int>(lua_tonumber(L, 2));
		const int pct = lua_isnumber(L, 3) ? static_cast<int>(lua_tonumber(L, 3)) : 100;
		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		if (cell < 0 || cell >= WEAR_MAX_NUM ||
			character == entt::null || !g_registry.valid(character))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		const entt::entity source = ItemSystem::GetWearItem(character, cell);
		const uint32_t refinedVnum = ItemSystem::GetItemRefineVnum(source);
		if (!ItemSystem::IsValidItem(source) || refinedVnum == 0 ||
			ItemSystem::GetItemRefineLevel(source) > levelLimit ||
			(pct != 100 && number(1, 100) > pct))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		const entt::entity replacement = ItemSystem::CreateItemEcs(refinedVnum, 1, 0, false);
		if (!ItemSystem::IsValidItem(replacement))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		bool initialized = true;
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			if (ItemSystem::GetItemSocket(source, i) == 0)
				break;
			initialized = initialized && ItemSystem::SetItemSocket(replacement, i, 1);
		}

		int targetSocket = 0;
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			const uint32_t socket = ItemSystem::GetItemSocket(source, i);
			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM && targetSocket < ITEM_SOCKET_MAX_NUM)
				initialized = initialized && ItemSystem::SetItemSocket(replacement, targetSocket++, socket);
		}

		initialized = initialized && ItemSystem::CopyItemAttributesEcs(source, replacement);
		if (!initialized || !ItemSystem::RemoveItemEcs(source))
		{
			ItemSystem::DestroyItemEntityEcs(replacement, "QUEST_REFINE_INIT_ROLLBACK");
			lua_pushboolean(L, 0);
			return 1;
		}

		if (!ItemSystem::EquipItemEcs(character, replacement, cell))
		{
			ItemSystem::EquipItemEcs(character, source, cell);
			ItemSystem::DestroyItemEntityEcs(replacement, "QUEST_REFINE_EQUIP_ROLLBACK");
			lua_pushboolean(L, 0);
			return 1;
		}

		if (!ItemSystem::DestroyItemEntityEcs(source, "REMOVE (REFINE SUCCESS)"))
		{
			ItemSystem::RemoveItemEcs(replacement);
			ItemSystem::EquipItemEcs(character, source, cell);
			ItemSystem::DestroyItemEntityEcs(replacement, "QUEST_REFINE_SOURCE_ROLLBACK");
			lua_pushboolean(L, 0);
			return 1;
		}

		LogManager::instance().ItemLogEntity(
			character, replacement,
			"REFINE SUCCESS (QUEST)", ItemSystem::GetItemName(replacement));
		ItemSystem::FlushDelayedSaveEcs(replacement);
		lua_pushboolean(L, 1);
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
            if (sl->levels && dwVnum < SKILL_MAX_NUM)
            {
                lua_pushnumber(L, sl->levels[dwVnum].bLevel);
                return 1;
            }
        }
		lua_pushnumber(L, 0);
        return 1;
    }

	ALUA(pc_give_lotto)
	{
        // migrated from CHARACTER::GetPlayerID
        // DUAL-PATH: legacy only during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity character = q.GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(character);
		LOG_INFO("TRY GIVE LOTTO TO pid {}", playerID);

		uint32_t * pdw = M2_NEW uint32_t[3];

		pdw[0] = 50001;
		pdw[1] = 1;
		pdw[2] = q.GetEventFlag("lotto_round");

		// ��÷���� ������ �����Ѵ�
		DBManager::instance().ReturnQuery(QID_LOTTO, playerID, pdw,
				"INSERT INTO lotto_list VALUES(0, 'server%s', %u, NOW())",
				get_table_postfix(), playerID);

		return 0;
	}

	ALUA(pc_aggregate_monster)
	{
        // migrated from CHARACTER::AggregateMonster
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		ch->AggregateMonster();
		return 0;
	}

	ALUA(pc_forget_my_attacker)
	{
        // migrated from CHARACTER::ForgetMyAttacker
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		ch->ForgetMyAttacker();
		return 0;
	}

	ALUA(pc_attract_ranger)
	{
        // migrated from CHARACTER::AttractRanger
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		ch->AttractRanger();
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
		if (const auto* marriageState = ECS_TryGet<ecs::MarriageState>(e))
		{
			lua_pushboolean(L, marriageState->partner ? 1 : 0);
			return 1;
		}
		lua_pushboolean(L, marriage::CManager::instance().IsEngaged(ecs::PlayerRuntime::GetPlayerID(e)) ? 1 : 0);
		return 1;
	}

	ALUA(pc_is_married)
	{
		// migrated from CHARACTER::GetPlayerID
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* marriageState = ECS_TryGet<ecs::MarriageState>(e))
		{
			lua_pushboolean(L, marriageState->weddingMap != nullptr ? 1 : 0);
			return 1;
		}
		lua_pushboolean(L, marriage::CManager::instance().IsMarried(ecs::PlayerRuntime::GetPlayerID(e)) ? 1 : 0);
		return 1;
	}

	ALUA(pc_is_engaged_or_married)
	{
		// migrated from CHARACTER::GetPlayerID
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* marriageState = ECS_TryGet<ecs::MarriageState>(e))
		{
			lua_pushboolean(L, (marriageState->partner || marriageState->weddingMap) ? 1 : 0);
			return 1;
		}
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

	ALUA(pc_diamond_refine)
	{
		if (!lua_isnumber(L, 1))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		int cost = (int) lua_tonumber(L, 1);
		int pct = (int)lua_tonumber(L, 2);

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity npcEntity = CQuestManager::instance().GetCurrentNPCEntity();
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		if (ItemSystem::IsValidItem(item))
			lua_pushboolean(L, mining::OreRefine(
				chEntity,
				npcEntity,
				item,
				cost,
				pct,
				entt::null));
		else
			lua_pushboolean(L, 0);

		return 1;
	}

	ALUA(pc_ore_refine)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		int cost = (int)lua_tonumber(L, 1);
		int pct = (int)lua_tonumber(L, 2);
		int inv_type = (int)lua_tonumber(L, 3);
		int cell = (int)lua_tonumber(L, 4);

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity npcEntity = CQuestManager::instance().GetCurrentNPCEntity();
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();
		entt::entity metinstoneItem = entt::null;

		if (inv_type == EXTRA_INVENTORY)
			metinstoneItem = ItemSystem::GetExtraInventoryItem(chEntity, cell);
		else
			metinstoneItem = ItemSystem::GetInventoryItem(chEntity, cell);

		if (ItemSystem::IsValidItem(item) && ItemSystem::IsValidItem(metinstoneItem))
			lua_pushboolean(L, mining::OreRefine(
				chEntity,
				npcEntity,
				item,
				cost,
				pct,
				metinstoneItem));
		else
			lua_pushboolean(L, 0);

		return 1;
	}


	ALUA(pc_clear_skill)
	{
        // migrated from CHARACTER::ClearSkill
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if ( ch == nullptr) return 0;

		ch->ClearSkill();

		return 0;
	}

	ALUA(pc_clear_sub_skill)
	{
        // migrated from CHARACTER::ClearSubSkill
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if ( ch == nullptr) return 0;

		ch->ClearSubSkill();

		return 0;
	}

    ALUA(pc_set_skill_point)
    {
        // migrated from CHARACTER::SetRealPoint
        // DUAL-PATH: ECS update + legacy call during migration window
        if (!lua_isnumber(L, 1))
            return 0;
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 0;
        int newPoint = (int) lua_tonumber(L, 1);
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
        {
            points->base.points[POINT_SKILL] = newPoint;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        ch->SetRealPoint(POINT_SKILL, newPoint);
        ch->SetPoint(POINT_SKILL, ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), POINT_SKILL));
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_SKILL, 0);
        ch->ComputePoints();
        NetworkSyncSystem::PointsPacket(AIHelpers::EcsOf(ch));
        return 0;
    }

	// RESET_ONE_SKILL
	ALUA(pc_clear_one_skill)
	{
        // migrated from CHARACTER::ResetOneSkill
        // DUAL-PATH: legacy only during migration window
		int vnum = (int)lua_tonumber(L, 1);
		LOG_INFO("{} skill clear", vnum);

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if ( ch == nullptr)
		{
			LOG_INFO("skill clear fail");
			lua_pushnumber(L, 0);
			return 1;
		}

		LOG_INFO("{} skill clear", vnum);

		ch->ResetOneSkill(vnum);

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

	//�ڷ���Ʈ
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
			// ������ ����
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

	ALUA(pc_give_polymorph_book)
	{
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) ||
			!lua_isnumber(L, 3) || !lua_isnumber(L, 4))
		{
			sys_err("Wrong Quest Function Arguments: pc_give_polymorph_book");
			return 0;
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		CPolymorphUtils::instance().GiveBook(chEntity, (uint32_t)lua_tonumber(L, 1), (uint32_t)lua_tonumber(L, 2), (uint8_t)lua_tonumber(L, 3), (uint8_t)lua_tonumber(L, 4));

		return 0;
	}

	ALUA(pc_upgrade_polymorph_book)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity item = CQuestManager::instance().GetCurrentItemEntity();

		bool ret = CPolymorphUtils::instance().BookUpgrade(chEntity, item);

		lua_pushboolean(L, ret);

		return 1;
	}

	ALUA(pc_get_premium_remain_sec)
	{
		int	remain_seconds	= 0;
		int	premium_type	= 0;
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
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

		remain_seconds = ch->GetPremiumRemainSeconds(premium_type);

		lua_pushnumber(L, remain_seconds);
		return 1;
	}

	ALUA(pc_send_block_mode)
	{
        // migrated from CHARACTER::SetBlockModeForce
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		ch->SetBlockModeForce((uint8_t)lua_tonumber(L, 1));

		return 0;
	}

    ALUA(pc_change_empire)
    {
        // migrated from CHARACTER::ChangeEmpire
        // DUAL-PATH: ECS update + legacy call during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
#ifdef ENABLE_BUG_FIXES
        if (!ch) {
            return 0;
        } else if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))) {
#ifdef TEXTS_IMPROVEMENT
            ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 1245, "");
#endif
            return 0;
        }
#endif
        const int result = ch ? ch->ChangeEmpire((uint8_t)lua_tonumber(L, 1)) : 0;
        if (ch)
        {
            if (auto* empire = ECS_TryGet<ecs::EmpireComponent>(e))
                empire->value = static_cast<uint8_t>(ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(ch)));
            if (e != entt::null && g_registry.valid(e))
                g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        lua_pushnumber(L, result);
        return 1;
    }

	ALUA(pc_get_change_empire_count)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		lua_pushnumber(L, ch->GetChangeEmpireCount());

		return 1;
	}

	ALUA(pc_set_change_empire_count)
	{
		// migrated from CHARACTER::SetChangeEmpireCount
		// DUAL-PATH: ECS update + legacy call during migration window
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e != entt::null && g_registry.valid(e))
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch) ch->SetChangeEmpireCount();
		return 0;
	}

    ALUA(pc_change_name)
    {
        // migrated from CHARACTER::SetNewName
        // DUAL-PATH: ECS update + legacy call during migration window
        // return values:
        // 0: new name already set, waiting for logout
        // 1: invalid lua argument
        // 2: failed check_name
        // 3: duplicate name exists
        // 4: success
        // 5: feature not supported
#ifdef ENABLE_LOCALECHECK_CHANGENAME
        lua_pushnumber(L, 5);
        return 1;
#endif
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
        {
            lua_pushnumber(L, 1);
            return 1;
        }
        if ( ch->GetNewName().size() != 0 )
        {
            lua_pushnumber(L, 0);
            return 1;
        }
        if (!lua_isstring(L, 1))
        {
            lua_pushnumber(L, 1);
            return 1;
        }
        const char * szName = lua_tostring(L, 1);
        if ( check_name(szName) == false )
        {
            lua_pushnumber(L, 2);
            return 1;
        }
        char szQuery[1024];
        snprintf(szQuery, sizeof(szQuery), "SELECT COUNT(*) FROM player%s WHERE name='%s'", get_table_postfix(), szName);
        std::unique_ptr<SQLMsg> pmsg(DBManager::instance().DirectQuery(szQuery));
        if ( pmsg->Get()->uiNumRows > 0 )
        {
            MYSQL_ROW row = mysql_fetch_row(pmsg->Get()->pSQLResult);
            int count = 0;
            str_to_number(count, row[0]);
            if ( count != 0 )
            {
                lua_pushnumber(L, 3);
                return 1;
            }
        }
        uint32_t pid = (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
        db_clientdesc->DBPacketHeader(HEADER_GD_FLUSH_CACHE, 0, sizeof(uint32_t));
        db_clientdesc->Packet(&pid, sizeof(uint32_t));
        MessengerManager::instance().RemoveAllList(ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data());
        LogManager::instance().ChangeNameLog(pid, ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(ch)).data(), szName, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHostName());
        snprintf(szQuery, sizeof(szQuery), "UPDATE player%s SET name='%s' WHERE id=%u", get_table_postfix(), szName, pid);
        SQLMsg * msg = DBManager::instance().DirectQuery(szQuery);
        M2_DELETE(msg);
        ch->SetNewName(szName);
        if (auto* playerName = ECS_TryGet<ecs::PlayerName>(e))
            playerName->value = szName;
        if (e != entt::null && g_registry.valid(e))
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        lua_pushnumber(L, 4);
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
        // migrated from CHARACTER::ResetPoint
        // TODO Phase 8: CharacterPoints decomposition
        // DUAL-PATH: legacy only during migration window
		if (lua_isnumber(L, 1))
		{
			int idx = (int)lua_tonumber(L, 1);

			if ( idx >= 0 && idx < 4 )
			{
				const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
				auto* ch = ecs::LegacyCharOf(chEntity);
				uint8_t point = POINT_NONE;
				char buf[128];

				switch ( idx )
				{
					case 0 : point = POINT_HT; break;
					case 1 : point = POINT_IQ; break;
					case 2 : point = POINT_ST; break;
					case 3 : point = POINT_DX; break;
					default : lua_pushboolean(L, false); return 1;
				}

				int64_t old_val = ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), point);
				int64_t old_stat = ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), POINT_STAT);

				ch->SetRealPoint(point, 1);
				ch->SetPoint(point, ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), point));

				ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_STAT, old_val-1);

				if ( point == POINT_HT )
				{
					uint8_t job = ch->GetJob();
					ch->SetRandomHP(((ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)))-1) * number(JobInitialPoints[job].hp_per_lv_begin, JobInitialPoints[job].hp_per_lv_end));
				}
				else if ( point == POINT_IQ )
				{
					uint8_t job = ch->GetJob();
					ch->SetRandomSP(((ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)))-1) * number(JobInitialPoints[job].sp_per_lv_begin, JobInitialPoints[job].sp_per_lv_end));
				}

				ch->ComputePoints();
				NetworkSyncSystem::PointsPacket(AIHelpers::EcsOf(ch));

				if ( point == POINT_HT )
				{
					ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_HP, ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) - ch->GetHP());
				}
				else if ( point == POINT_IQ )
				{
					ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_SP, ecs::PointSystem::GetMaxSP(AIHelpers::EcsOf(ch)) - ch->GetSP());
				}

				switch ( idx )
				{
					case 0 :
						snprintf(buf, sizeof(buf), "reset ht(%lld)->1 stat_point(%lld)->(%lld)", old_val, old_stat, ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), POINT_STAT));
						break;
					case 1 :
						snprintf(buf, sizeof(buf), "reset iq(%lld)->1 stat_point(%lldd)->(%lld)", old_val, old_stat, ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), POINT_STAT));
						break;
					case 2 :
						snprintf(buf, sizeof(buf), "reset st(%lld)->1 stat_point(%lld)->(%lld)", old_val, old_stat, ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), POINT_STAT));
						break;
					case 3 :
						snprintf(buf, sizeof(buf), "reset dx(%lld)->1 stat_point(%lld)->(%lld)", old_val, old_stat, ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), POINT_STAT));
						break;
				}

				LogManager::instance().CharLog(ch, 0, "RESET_ONE_STATUS", buf);

				lua_pushboolean(L, true);
				return 1;
			}
		}

		lua_pushboolean(L, false);
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
        // migrated from CHARACTER::SetRealPoint
        // DUAL-PATH: ECS update + legacy call during migration window
        if ( lua_isnumber(L, 1) == false )
            return 1;
        int64_t newPoint = static_cast<int64_t>(lua_tonumber(L, 1));
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 1;
        int64_t usedPoint = newPoint - ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), POINT_HT);
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
        {
            points->base.points[POINT_HT] = newPoint;
            points->base.points[POINT_STAT] -= usedPoint;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        ch->SetRealPoint(POINT_HT, newPoint);
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_HT, 0);
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_STAT, -usedPoint);
        ch->ComputePoints();
        NetworkSyncSystem::PointsPacket(AIHelpers::EcsOf(ch));
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
        // migrated from CHARACTER::SetRealPoint
        // DUAL-PATH: ECS update + legacy call during migration window
        if ( lua_isnumber(L, 1) == false )
            return 1;
        int64_t newPoint = (int64_t)lua_tonumber(L, 1);
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 1;
        int64_t usedPoint = newPoint - ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), POINT_IQ);
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
        {
            points->base.points[POINT_IQ] = newPoint;
            points->base.points[POINT_STAT] -= usedPoint;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        ch->SetRealPoint(POINT_IQ, newPoint);
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_IQ, 0);
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_STAT, -usedPoint);
        ch->ComputePoints();
        NetworkSyncSystem::PointsPacket(AIHelpers::EcsOf(ch));
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
        // migrated from CHARACTER::SetRealPoint
        // DUAL-PATH: ECS update + legacy call during migration window
        if ( lua_isnumber(L, 1) == false )
            return 1;
        int64_t newPoint = (int64_t)lua_tonumber(L, 1);
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 1;
        int64_t usedPoint = newPoint - ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), POINT_ST);
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
        {
            points->base.points[POINT_ST] = newPoint;
            points->base.points[POINT_STAT] -= usedPoint;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        ch->SetRealPoint(POINT_ST, newPoint);
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_ST, 0);
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_STAT, -usedPoint);
        ch->ComputePoints();
        NetworkSyncSystem::PointsPacket(AIHelpers::EcsOf(ch));
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
        // migrated from CHARACTER::SetRealPoint
        // DUAL-PATH: ECS update + legacy call during migration window
        if ( lua_isnumber(L, 1) == false )
            return 1;
        int64_t newPoint = (int64_t)lua_tonumber(L, 1);
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 1;
        int64_t usedPoint = newPoint - ecs::PointSystem::GetReal(AIHelpers::EcsOf(ch), POINT_DX);
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
        {
            points->base.points[POINT_DX] = newPoint;
            points->base.points[POINT_STAT] -= usedPoint;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        ch->SetRealPoint(POINT_DX, newPoint);
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_DX, 0);
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_STAT, -usedPoint);
        ch->ComputePoints();
        NetworkSyncSystem::PointsPacket(AIHelpers::EcsOf(ch));
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

	ALUA(pc_get_socket_items)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		lua_newtable(L);
		int idx = 1;
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
		{
			const entt::entity item = ItemSystem::GetInventoryItem(character, i);
			if (!ItemSystem::IsValidItem(item))
				continue;
			int j = 0;
			for (; j < ITEM_SOCKET_MAX_NUM; ++j)
			{
				const int32_t socket = ItemSystem::GetItemSocket(item, j);
				if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				{
					TItemTable* pItemInfo = ITEM_MANAGER::instance().GetTable(socket);
					if (pItemInfo != nullptr && pItemInfo->bType == ITEM_METIN)
						break;
				}
			}
			if (j >= ITEM_SOCKET_MAX_NUM)
				continue;
			lua_newtable(L);
			lua_pushstring(L, ItemSystem::GetItemName(item));
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, i);
			lua_rawseti(L, -2, 2);
			lua_rawseti(L, -2, idx++);
		}
		return 1;
	}

	ALUA(pc_get_empty_inventory_count)
	{
		// migrated from CHARACTER::CountEmptyInventory
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* inventory = ECS_TryGet<ecs::MainInventoryRuntimeComponent>(e))
		{
			int emptyCount = 0;
			for (int cell = 0; cell < INVENTORY_MAX_NUM; ++cell)
			{
				if (inventory->items[cell] == entt::null)
				{
					++emptyCount;
				}
			}
			lua_pushnumber(L, emptyCount);
			return 1;
		}
		lua_pushnumber(L, 0);
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

	ALUA(pc_get_special_ride_vnum)
	{
		const entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (e != entt::null && g_registry.valid(e))
		{
			const entt::entity unique1 = ItemSystem::GetWearItem(e, WEAR_UNIQUE1);
			const entt::entity unique2 = ItemSystem::GetWearItem(e, WEAR_UNIQUE2);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
			const entt::entity mountCostume = ItemSystem::GetWearItem(e, WEAR_COSTUME_MOUNT);
#endif
			if (unique1 != entt::null && ItemSystem::GetItemSpecialGroup(unique1) == UNIQUE_GROUP_SPECIAL_RIDE)
			{
				lua_pushnumber(L, ItemSystem::GetItemVnum(unique1));
				lua_pushnumber(L, ItemSystem::GetItemSocket(unique1, 0));
				return 2;
			}
			if (unique2 != entt::null && ItemSystem::GetItemSpecialGroup(unique2) == UNIQUE_GROUP_SPECIAL_RIDE)
			{
				lua_pushnumber(L, ItemSystem::GetItemVnum(unique2));
				lua_pushnumber(L, ItemSystem::GetItemSocket(unique2, 0));
				return 2;
			}
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
			if (mountCostume != entt::null)
			{
				lua_pushnumber(L, ItemSystem::GetItemVnum(mountCostume));
				lua_pushnumber(L, ItemSystem::GetItemSocket(mountCostume, 0));
				return 2;
			}
#endif
		}
		lua_pushnumber(L, 0);
		lua_pushnumber(L, 0);
		return 2;
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

	ALUA(pc_give_poly_marble)
	{
		const int dwVnum = lua_tonumber(L, 1);

		const CMob* MobInfo = CMobManager::instance().Get(dwVnum);

		if (nullptr == MobInfo)
		{
			lua_pushboolean(L, false);
			return 1;
		}

		if (0 == MobInfo->m_table.dwPolymorphItemVnum)
		{
			lua_pushboolean(L, false);
			return 1;
		}

		const entt::entity item = ItemSystem::CreateItemEcs(MobInfo->m_table.dwPolymorphItemVnum);

		if (!ItemSystem::IsValidItem(item))
		{
			lua_pushboolean(L, false);
			return 1;
		}

		const entt::entity character = CQuestManager::instance().GetCurrentPCEntity();
		if (!ItemSystem::SetItemSocket(item, 0, dwVnum))
		{
			ItemSystem::DestroyItemEntityEcs(item, "QUEST_POLY_MARBLE_INIT_FAIL");
			lua_pushboolean(L, false);
			return 1;
		}

		const int iEmptyCell = ItemSystem::GetEmptyInventoryPositionEcs(character, item);

		if (-1 == iEmptyCell)
		{
			ItemSystem::DestroyItemEntityEcs(item, "QUEST_ITEM_FAIL");
			lua_pushboolean(L, false);
			return 1;
		}

		if (!ItemSystem::PlaceItemEcs(character, item, INVENTORY, iEmptyCell))
		{
			ItemSystem::DestroyItemEntityEcs(item, "QUEST_POLY_MARBLE_PLACE_FAIL");
			lua_pushboolean(L, false);
			return 1;
		}

		const PC* pPC = CQuestManager::instance().GetCurrentPC();

		LogManager::instance().QuestRewardLog(pPC->GetCurrentQuestName().c_str(), ecs::PlayerRuntime::GetPlayerID(character), ecs::PointSystem::GetLevel(character), MobInfo->m_table.dwPolymorphItemVnum, dwVnum);

		lua_pushboolean(L, true);

		return 1;
	}

	ALUA(pc_get_sig_items)
	{
		const uint32_t group_vnum = static_cast<uint32_t>(lua_tonumber(L, 1));
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		int count = 0;
		for (int i = 0; i < INVENTORY_MAX_NUM; ++i)
		{
			const entt::entity item = ItemSystem::GetInventoryItem(character, i);
			if (ItemSystem::IsValidItem(item) && ItemSystem::GetItemSIGVnum(item) == group_vnum)
			{
				lua_pushnumber(L, ItemSystem::GetItemID(item));
				++count;
			}
		}
		return count;
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

	ALUA(pc_give_award)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		LPDESC desc = ecs::PlayerRuntime::GetDesc(chEntity);
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isstring(L, 3) )
		{
			sys_err("QUEST give award call error : wrong argument");
			lua_pushnumber (L, 0);
			return 1;
		}
		if (!desc)
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		uint32_t dwVnum = (int) lua_tonumber(L, 1);

		int icount = (int) lua_tonumber(L, 2);

		LOG_INFO("QUEST [award] item {} to login {}", dwVnum, desc->GetAccountTable().login);

		DBManager::instance().Query("INSERT INTO item_award (login, vnum, count, given_time, why, mall)select '%s', %d, %d, now(), '%s', 1 from DUAL where not exists (select login, why from item_award where login = '%s' and why  = '%s') ;",
			desc->GetAccountTable().login,
			dwVnum,
			icount,
			lua_tostring(L,3),
			desc->GetAccountTable().login,
			lua_tostring(L,3));

		lua_pushnumber (L, 0);
		return 1;
	}
	ALUA(pc_give_award_socket)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		LPDESC desc = ecs::PlayerRuntime::GetDesc(chEntity);
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isstring(L, 3) || !lua_isstring(L, 4) || !lua_isstring(L, 5) || !lua_isstring(L, 6) )
		{
			sys_err("QUEST give award call error : wrong argument");
			lua_pushnumber (L, 0);
			return 1;
		}
		if (!desc)
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		uint32_t dwVnum = (int) lua_tonumber(L, 1);

		int icount = (int) lua_tonumber(L, 2);

		LOG_INFO("QUEST [award] item {} to login {}", dwVnum, desc->GetAccountTable().login);

		DBManager::instance().Query("INSERT INTO item_award (login, vnum, count, given_time, why, mall, socket0, socket1, socket2)select '%s', %d, %d, now(), '%s', 1, %s, %s, %s from DUAL where not exists (select login, why from item_award where login = '%s' and why  = '%s') ;",
			desc->GetAccountTable().login,
			dwVnum,
			icount,
			lua_tostring(L,3),
			lua_tostring(L,4),
			lua_tostring(L,5),
			lua_tostring(L,6),
			desc->GetAccountTable().login,
			lua_tostring(L,3));

		lua_pushnumber (L, 0);
		return 1;
	}

	ALUA(pc_get_informer_type)	//���� ���� ���
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

	ALUA(pc_get_killee_drop_pct)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		const entt::entity killee = ecs::PlayerRuntime::GetQuestNPC(character);
		auto* characterLegacy = ecs::LegacyCharOf(character);
		auto* killeeLegacy = ecs::LegacyCharOf(killee);
		int iDeltaPercent = -1;
		int iRandRange = -1;
		// Explicit legacy boundary: the drop-rate authority still lives in ITEM_MANAGER.
		if (!killeeLegacy || !characterLegacy ||
			!ITEM_MANAGER::instance().GetDropPct(killeeLegacy, characterLegacy, iDeltaPercent, iRandRange))
		{
			sys_err("killee is null");
			lua_pushnumber(L, -1);
			lua_pushnumber(L, -1);
			return 2;
		}
		lua_pushnumber(L, iDeltaPercent);
		lua_pushnumber(L, iRandRange);
		return 2;
	}

#ifdef ENABLE_NEWSTUFF

	#define PC_MI0L_ARG1	2		// 1: vnum or locale_name, 2: count
	#define PC_MI0L_ARG2	3		// socket 1-2-3
	#define PC_MI0L_ARG3	7*2		// (type, value)*7
	enum eMakeItemType{PCMI0_GIVE, PCMI0_DROP, PCMI0_DROPWP, PCMI0_MAX};
	ALUA(pc_make_item0)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		if (!lua_istable(L, 1) && !lua_istable(L, 2) && !lua_istable(L, 3) && !lua_isnumber(L, 4))
			return 0;

		int m_idx = 0;
		// config arg1
		uint32_t m_vnum = 0;
		int m_count = 0;
		// start arg1
		lua_pushnil(L);
		while (lua_next(L, 1))
		{
			switch(m_idx)
			{
				case 0:
					if (lua_isnumber(L, -1))
					{
						if ((m_vnum = lua_tonumber(L, -1))<=0)
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 738, "%d", m_vnum);
#endif
							return 0;
						}
					}
					else if (lua_isstring(L, -1))
					{
						if (!ITEM_MANAGER::instance().GetVnum(lua_tostring(L, -1), m_vnum))
						{
#ifdef TEXTS_IMPROVEMENT
							ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 739, "%d", m_vnum);
#endif
							return 0;
						}
					}
					else
					{
						return 0;
					}
					break;
				case 1:
					if (lua_isnumber(L, -1))
					{
						// if ((m_count = MINMAX(1, lua_tonumber(L, -1), ITEM_MAX_COUNT))<=0)
						if ((m_count = lua_tonumber(L, -1))<=0)
						{
							return 0;
						}
					}
					else
					{
						return 0;
					}
					break;
				default:
					break;
			}
			m_idx++;
			lua_pop(L, 1);
		}

		int m_socket[ITEM_SOCKET_MAX_NUM] = {0};
		m_idx = 0;
		lua_pushnil(L);
		while (lua_next(L, 2) && m_idx<ITEM_SOCKET_MAX_NUM)
		{
			if (!lua_isnumber(L, -1))
				return 0;
			m_socket[m_idx++] = lua_tonumber(L, -1);
			lua_pop(L, 1);
		}

		int m_attr[ITEM_ATTRIBUTE_MAX_NUM*2] = {0};
		m_idx = 0;
		lua_pushnil(L);
		while (lua_next(L, 3) && m_idx<(ITEM_ATTRIBUTE_MAX_NUM*2))
		{
			if (!lua_isnumber(L, -1))
				return 0;
			m_attr[m_idx++] = lua_tonumber(L, -1);
			lua_pop(L, 1);
		}

		uint32_t m_state = 0;
		if ((m_state = lua_tonumber(L, 4))>=PCMI0_MAX)
			return 0;

		const entt::entity newItem = ItemSystem::CreateItemEcs(m_vnum, m_count, 0, false);
		if (ItemSystem::IsValidItem(newItem))
		{
			// socket
			bool initialized = true;
			for (int i=0; i<ITEM_SOCKET_MAX_NUM; i++)
				initialized = initialized && ItemSystem::SetItemSocket(newItem, i, m_socket[i]);
			// attr
			for (int i=0; i<ITEM_ATTRIBUTE_MAX_NUM; i++)
				initialized = initialized && ItemSystem::SetItemForceAttributeEcs(newItem, i, m_attr[(i*2)+0], m_attr[(i*2)+1]);
			if (!initialized)
			{
				ItemSystem::DestroyItemEntityEcs(newItem, "QUEST_MAKE_ITEM_INIT_FAIL");
				lua_pushboolean(L, false);
				return 1;
			}
			// state
			int iEmptyCell = -1;
			int m_sec = 0;
			PIXEL_POSITION pos{};
			switch(m_state)
			{
				case PCMI0_GIVE:
					iEmptyCell = ItemSystem::GetEmptyInventoryPositionEcs(chEntity, newItem);
					if (-1 == iEmptyCell)
					{
						ItemSystem::DestroyItemEntityEcs(newItem, "QUEST_ITEM_FAIL");
						lua_pushboolean(L, false);
						return 1;
					}
					if (!ItemSystem::PlaceItemEcs(chEntity, newItem, INVENTORY, iEmptyCell))
					{
						ItemSystem::DestroyItemEntityEcs(newItem, "QUEST_ITEM_PLACE_FAIL");
						lua_pushboolean(L, false);
						return 1;
					}
					break;
				case PCMI0_DROPWP:
					if (lua_isnumber(L, 5) && (m_sec = lua_tonumber(L, 5)))
						ItemSystem::SetGroundOwnershipLegacyBoundary(newItem, chEntity, m_sec <= 0 ? 1 : m_sec);
					else
						ItemSystem::SetGroundOwnershipLegacyBoundary(newItem, chEntity);
				case PCMI0_DROP:
					pos.x = ecs::PlayerRuntime::GetX(chEntity) + number(-200, 200);
					pos.y = ecs::PlayerRuntime::GetY(chEntity) + number(-200, 200);

					if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
							newItem, ecs::PlayerRuntime::GetMapIndex(chEntity), pos))
					{
						ItemSystem::DestroyItemEntityEcs(newItem, "QUEST_ITEM_GROUND_FAIL");
						lua_pushboolean(L, false);
						return 1;
					}
					break;
				default:
					ItemSystem::DestroyItemEntityEcs(newItem, "QUEST_ITEM_BAD_STATE");
					lua_pushboolean(L, false);
					return 1;
			}
			lua_pushboolean(L, true);
		}
		else
			lua_pushboolean(L, false);

		return 1;
	}

    ALUA(pc_set_race0)
    {
        // migrated from CHARACTER::SetRace
        // DUAL-PATH: ECS update + legacy call during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 0;
        int amount = MINMAX(0, lua_tonumber(L, 1), JOB_MAX_NUM);
        ESex mySex = GET_SEX(ch);
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
#ifdef ENABLE_WOLFMAN_CHARACTER
            case JOB_WOLFMAN:
                dwRace = (mySex==SEX_MALE)?MAIN_RACE_WOLFMAN_M:MAIN_RACE_WOLFMAN_M;
                break;
#endif
        }
        if (dwRace!=(ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(ch))))
        {
            ch->SetRace(dwRace);
            ch->ClearSkill();
            ch->SetSkillGroup(0);
            ch->SetPolymorph(101);
            ch->SetPolymorph(0);
        }
        if (auto* race = ECS_TryGet<ecs::RaceComponent>(e))
            race->value = static_cast<uint16_t>((ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(ch))));
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
            points->base.skill_group = ch->GetSkillGroup();
        if (e != entt::null && g_registry.valid(e))
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
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
        // migrated from CHARACTER::PullMonster
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		ch->PullMonster();
		return 0;
	}

    ALUA(pc_set_level0)
    {
        // migrated from CHARACTER::ResetPoint
        // DUAL-PATH: ECS update + legacy call during migration window
        const int level = static_cast<int>(lua_tonumber(L, 1));
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 0;
        ch->ResetPoint(level);
        ch->ClearSkill();
        ch->ClearSubSkill();
        if (auto* lv = ECS_TryGet<ecs::LevelComponent>(e))
            lv->value = (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(ch)));
        if (auto* health = ECS_TryGet<ecs::Health>(e))
        {
            health->current = ch->GetHP();
            health->max = ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch));
        }
        if (auto* mana = ECS_TryGet<ecs::Mana>(e))
        {
            mana->current = ch->GetSP();
            mana->max = ecs::PointSystem::GetMaxSP(AIHelpers::EcsOf(ch));
        }
        if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
        if (e != entt::null && g_registry.valid(e))
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        return 0;
    }

    ALUA(pc_set_gm_level)
    {
        // migrated from CHARACTER::SetGMLevel
        // DUAL-PATH: ECS update + legacy call during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 0;
        ch->SetGMLevel();
        if (auto* gm = ECS_TryGet<ecs::GMLevel>(e))
            gm->level = ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch));
        if (auto* status = ECS_TryGet<ecs::StatusFlags>(e))
            status->isGM = (ecs::PlayerRuntime::GetGMLevel(AIHelpers::EcsOf(ch)) > 0);
        if (e != entt::null && g_registry.valid(e))
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
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
#ifdef ENABLE_WOLFMAN_CHARACTER
	ALUA(pc_if_bleeding)
	{
		// migrated from CHARACTER::IsAffectFlag(AFF_BLEEDING)
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
		{
			lua_pushboolean(L, sf->hasBled ? 1 : 0);
			return 1;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, AffectSystem::IsAffectFlag(chEntity, AFF_BLEEDING) ? 1 : 0);
		return 1;
	}
#endif
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
#ifdef ENABLE_WOLFMAN_CHARACTER
    ALUA(pc_sf_bleeding)
    {
        // migrated from CHARACTER::AddAffect
        // DUAL-PATH: ECS StatusFlags + legacy call
        const bool enabled = lua_toboolean(L, 1) != 0;
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* sf = ECS_TryGet<ecs::StatusFlags>(e))
        {
            sf->hasBled = enabled;
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
        }
        if (e != entt::null && g_registry.valid(e))
        {
            if(enabled)
                AffectSystem::AddAffect(e, AFFECT_BLEEDING, 0, 0, AFF_BLEEDING, 30+1, 0, 1, 0);
            else
                AffectSystem::RemoveAffect(e, AFFECT_BLEEDING);
        }
        return 0;
    }
#endif
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
		// Explicit legacy boundary: CHARACTER::Dead has not been ECS-migrated yet.
		if (LPCHARACTER victim = CHARACTER_MANAGER::instance().FindPC(lua_tostring(L, 1)))
			victim->Dead(nullptr, 0);
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
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (ch)
            ch->Dead(nullptr, 0);
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
        // migrated from CHARACTER::PointChange
        // DUAL-PATH: ECS update + legacy call during migration window
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
        auto* ch = ecs::LegacyCharOf(chEntity);
        if (!ch)
            return 0;
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_HP, ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch)) - ch->GetHP());
        ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_SP, ecs::PointSystem::GetMaxSP(AIHelpers::EcsOf(ch)) - ch->GetSP());
        if (auto* health = ECS_TryGet<ecs::Health>(e))
        {
            health->current = ch->GetHP();
            health->max = ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(ch));
        }
        if (auto* mana = ECS_TryGet<ecs::Mana>(e))
        {
            mana->current = ch->GetSP();
            mana->max = ecs::PointSystem::GetMaxSP(AIHelpers::EcsOf(ch));
        }
        if (e != entt::null && g_registry.valid(e))
            g_registry.emplace_or_replace<ecs::DirtyTag>(e);
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
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		ch->Disconnect(lua_tostring(L, 1));
		return 0;
	}

	ALUA(pc_is_trade0)
	{
		// migrated from CHARACTER::GetExchange
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* shop = ECS_TryGet<ecs::ShopState>(e))
		{
			if (shop->currentShop || shop->myShop)
			{
				lua_pushboolean(L, 1);
				return 1;
			}
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		lua_pushboolean(L, ecs::SocialSystem::GetExchange(chEntity) ? 1 : 0);
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
			const auto* cube = ECS_TryGet<ecs::CubeWindowComponent>(e);
			if (ecs::SocialSystem::GetExchange(e) ||
				(shop && (shop->currentShop || shop->myShop || shop->shopOwner || shop->underRefine)) ||
				(safebox && safebox->isOpening) || (cube && cube->pNpc))
			{
				lua_pushboolean(L, 1);
				return 1;
			}
		}
		lua_pushboolean(L, ecs::SocialSystem::GetExchange(e) ? 1 : 0);
		return 1;
	}

	ALUA(pc_is_arena0)
	{
		// migrated from CHARACTER::GetArena
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (const auto* dungeon = ECS_TryGet<ecs::DungeonMembership>(e))
		{
			if (dungeon->warMap != nullptr)
			{
				lua_pushboolean(L, 1);
				return 1;
			}
		}
		lua_pushboolean(L, 0);
		return 1;
	}

	ALUA(pc_is_arena_observer0)
	{
		lua_pushboolean(L, ecs::PlayerRuntime::IsArenaObserverMode(
			CQuestManager::instance().GetPCEntity(L)) ? 1 : 0);
		return 1;
	}

	ALUA(pc_equip_slot0)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const int cell = lua_isnumber(L, 1) ? static_cast<int>(lua_tonumber(L, 1)) : -1;
		const entt::entity item = cell >= 0 && cell < INVENTORY_MAX_NUM
			? ItemSystem::GetInventoryItem(chEntity, static_cast<uint16_t>(cell))
			: entt::null;
		lua_pushboolean(L, ItemSystem::IsValidItem(item) && ItemSystem::EquipItemEcs(chEntity, item));
		return 1;
	}

	ALUA(pc_unequip_slot0)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const int wearCell = static_cast<int>(lua_tonumber(L, 1));
		const entt::entity item = wearCell >= 0 && wearCell < WEAR_MAX_NUM
			? ItemSystem::GetWearItem(chEntity, static_cast<uint8_t>(wearCell))
			: entt::null;
		lua_pushboolean(L, ItemSystem::IsValidItem(item) && ItemSystem::UnequipItemEcs(chEntity, item));
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

	ALUA(pc_give_random_book0)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity item = ItemSystem::CreateItemEcs(50300);
		const uint32_t skillVnum = lua_isnumber(L, 1)
			? ::GetRandomSkillVnum(lua_tonumber(L, 1))
			: ::GetRandomSkillVnum();
		const bool initialized = ItemSystem::IsValidItem(item) &&
			ItemSystem::SetItemSocket(item, 0, skillVnum);
		if (initialized)
			ItemSystem::AutoGiveItem(chEntity, item);
		else if (ItemSystem::IsValidItem(item))
			ItemSystem::DestroyItemEntityEcs(item, "QUEST_RANDOM_BOOK_INIT_FAIL");
		lua_pushboolean(L, initialized);
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
        // migrated from CHARACTER::SetShopOwner
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		//PREVENT_TRADE_WINDOW
		if (ch->IsOpenSafebox() || ecs::SocialSystem::GetExchange(AIHelpers::EcsOf(ch)) || ch->GetMyShop() || ch->IsCubeOpen())
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(ch), CHAT_TYPE_INFO, 294, "");
#endif
			return 0;
		}
		//END_PREVENT_TRADE_WINDOW

		LPSHOP sh = CShopManager::instance().Get(lua_tonumber(L, 1));
		sh->AddGuest(ch, 0, false);
		ch->SetShopOwner(NULL);
		return 0;
	}
#endif

#ifdef ENABLE_NEWGUILDMAKE
	enum MKGLD {MKGLD_INVALID_NAME_LENGTH=-2, MKGLD_INVALID_NAME_INPUT=-1, MKGLD_GUILD_NOT_CREATED=0, MKGLD_GUILD_CREATED=1, MKGLD_ALREADY_GUILDED=2, MKGLD_ALREADY_MASTER_GUILD=3};
	ALUA(pc_make_guild0)
	{
        // migrated from CHARACTER::GetGuild
        // DUAL-PATH: legacy only during migration window
		// -2 guild name is invalid (strlen <2 or >11!)
		// -1 guild name is invalid (special chars found!)
		// 0 guild not created (guild name already present or already member of a guild)
		// 1 guild created
		// 2 player already part of a guild
		// 3 player already guild master
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch)))
		{
			lua_pushnumber(L, ((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))) == ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch))->GetMasterPID())?MKGLD_ALREADY_MASTER_GUILD:MKGLD_ALREADY_GUILDED);
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

		cp.master = ch;
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


#ifdef ENABLE_ACCE_SYSTEM
	int pc_open_acce(lua_State * L)
	{
		if (lua_isboolean(L, 1))
		{
			// Explicit legacy boundary: all accessory-window state must move together.
			if (LPCHARACTER character = CQuestManager::instance().GetCurrentCharacterPtr())
				character->OpenAcce(lua_toboolean(L, 1));
		}
		else
			sys_err("Invalid argument: arg1 must be boolean.");
		return 0;
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
	ALUA(pc_get_gaya)
	{
		lua_pushnumber(L, CQuestManager::instance().GetCurrentCharacterPtr()->GetGaya());
		return 1;
	}

	ALUA(pc_give_gaya)
	{
        // migrated from CHARACTER::PointChange(POINT_GAYA, ...)
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
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

		DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST, (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))), iAmount);
		ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GAYA, iAmount, true);
		return 0;
	}

	ALUA(pc_change_gaya)
	{
		// migrated from CHARACTER::PointChange(POINT_GAYA, ...)
		// DUAL-PATH: ECS update + legacy call during migration window
		int gaya = (int)lua_tonumber(L, -1);
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		if (auto* points = ECS_TryGet<ecs::CharacterPoints>(e))
		{
			points->base.points[POINT_GAYA] += gaya;
			g_registry.emplace_or_replace<ecs::DirtyTag>(e);
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch && gaya + ch->GetGaya() < 0)
			sys_err("QUEST wrong ChangeGaya {} (now {})", gaya, ch->GetGaya());
		else if (ch)
		{
			DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST, (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch))), gaya);
			ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GAYA, gaya, true);
		}
		return 0;
	}
#endif

#ifdef __HIDE_COSTUME_SYSTEM__
	ALUA(pc_hide_costume)
	{
        // migrated from CHARACTER m_bHide*Costume flags
        // TODO Phase 8: add HideCostumeFlags component
        // DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
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

		if (bPartPos == 1)
			ch->SetBodyCostumeHidden(hidePart);
		else if (bPartPos == 2)
			ch->SetHairCostumeHidden(hidePart);
		else if (bPartPos == 3)
			ch->SetAcceCostumeHidden(hidePart);
		else if (bPartPos == 4)
			ch->SetWeaponCostumeHidden(hidePart);
		else
		{
			sys_err("Invalid part");
			return 0;
		}

		NetworkSyncSystem::UpdatePacket(AIHelpers::EcsOf(ch));
		return 0;
	}
#endif

#ifdef ENABLE_BATTLE_PASS
	int pc_update_dungeon_progress(lua_State * L)
	{
		if (!lua_isnumber(L, 1))
		{
			sys_err("arg1 must be number");
			return 0;
		}

		uint8_t bDungeonType = (int) lua_tonumber(L, 1);
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch)
		{
			uint8_t bBattlePassId = ch->GetBattlePassId();
			if(bBattlePassId)
			{
				uint32_t dwDungeonID, dwCount;
				if(CBattlePass::instance().BattlePassMissionGetInfo(bBattlePassId, COMPLETE_DUNGEON, &dwDungeonID, &dwCount))
				{
					if(dwDungeonID == bDungeonType && ch->GetMissionProgress(COMPLETE_DUNGEON, bBattlePassId) < dwCount)
						ch->UpdateMissionProgress(COMPLETE_DUNGEON, bBattlePassId, 1, dwCount);
				}
			}
		}

		return 0;
	}

	int pc_open_battle_pass_ranking(lua_State * L)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ch)
		{
			uint32_t dwPlayerId = (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(ch)));
			uint8_t bIsGlobal = 1;

			db_clientdesc->DBPacketHeader(HEADER_GD_BATTLE_PASS_RANKING, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetHandle(), sizeof(uint32_t) + sizeof(uint8_t));
			db_clientdesc->Packet(&dwPlayerId, sizeof(uint32_t));
			db_clientdesc->Packet(&bIsGlobal, sizeof(uint8_t));
		}

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

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch) {
			sys_err("GetCurrentCharacterPtr is NULL.");
			return 0;
		}

		lua_pushnumber(L, ch->GetQuestDamage(race));
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
				if (biologistMissionInfo[i][11] == 1) {
					if (ecs::QuestSystem::GetFlag(character, "biologist.stat") > i) {
						for (int j = 0; j < 4; j++) {
							type = biologistMissionInfo[i][3 + (j * 2)];
							if (type != APPLY_NONE) {
								CAffect * pkAff = AffectSystem::FindAffect(character, biologistMissionInfo[i][14], aApplyInfo[type].bPointType);
								if (pkAff) {
									continue;
								}

								if (last == 0) {
									last = 1;
								}

								ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "biologistch_append %d#%d#%d#%d", i, type, biologistMissionInfo[i][4 + (j * 2)], j);
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
						MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
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



	void RegisterPCFunctionTable()
	{
		luaL_reg pc_functions[] =
		{
			{ "get_wear",		pc_get_wear			},
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

#ifdef ENABLE_BATTLE_PASS
			{ "update_dungeon_progress", pc_update_dungeon_progress },
			{ "open_battle_pass_ranking", pc_open_battle_pass_ranking },
#endif

			{ "get_real_alignment",	pc_get_real_alignment	},
			{ "get_alignment",		pc_get_alignment	},
			{ "getweapon",		pc_get_weapon		},
			{ "get_weapon",		pc_get_weapon		},
			{ "getarmor",		pc_get_armor		},
			{ "get_armor",		pc_get_armor		},
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
			{ "give_item",		pc_give_item		},
			{ "give_item2",		pc_give_or_drop_item	},
#ifdef ENABLE_DICE_SYSTEM
			{ "give_item2_with_dice",	pc_give_or_drop_item_with_dice	},
#endif
			{ "give_item2_select",		pc_give_or_drop_item_and_select	},
			{ "give_gold",		pc_give_gold		},
			{ "count_item",		pc_count_item		},
			{ "remove_item",		pc_remove_item		},
			{ "countitem",		pc_count_item		},
			{ "removeitem",		pc_remove_item		},
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
			{ "give_item_from_special_item_group", pc_give_item_from_special_item_group	},
			{ "learn_grand_master_skill", pc_learn_grand_master_skill	},
			{ "is_skill_book_no_delay",	pc_is_skill_book_no_delay},
			{ "remove_skill_book_no_delay",	pc_remove_skill_book_no_delay},

			{ "enough_inventory",	pc_enough_inventory	},
			{ "get_horse_level",	pc_get_horse_level	}, // TO BE DELETED XXX
			{ "is_horse_alive",		pc_is_horse_alive	}, // TO BE DELETED XXX
			{ "revive_horse",		pc_revive_horse		}, // TO BE DELETED XXX
			{ "have_pos_scroll",	pc_have_pos_scroll	},
			{ "have_map_scroll",	pc_have_map_scroll	},
			{ "get_war_map",		pc_get_war_map		},
			{ "get_equip_refine_level",	pc_get_equip_refine_level },
			{ "refine_equip",		pc_refine_equip		},
			{ "get_skill_level",	pc_get_skill_level	},
			{ "give_lotto",		pc_give_lotto		},
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
			{ "ore_refine",		pc_ore_refine		},
			{ "diamond_refine",		pc_diamond_refine	},

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

            { "give_polymorph_book",    pc_give_polymorph_book  },
            { "upgrade_polymorph_book", pc_upgrade_polymorph_book },
            { "get_premium_remain_sec", pc_get_premium_remain_sec },

			{ "send_block_mode",		pc_send_block_mode	},

			{ "change_empire",			pc_change_empire	},
			{ "get_change_empire_count",	pc_get_change_empire_count	},
			{ "set_change_empire_count",	pc_set_change_empire_count	},

			{ "change_name",			pc_change_name },

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

			{ "get_socket_items",	pc_get_socket_items	},
			{ "get_empty_inventory_count",	pc_get_empty_inventory_count	},

			{ "get_logoff_interval",	pc_get_logoff_interval	},

			{ "is_riding",			pc_is_riding	},
			{ "get_special_ride_vnum",	pc_get_special_ride_vnum	},

			{ "can_warp",			pc_can_warp		},

			{ "dec_skill_point",	pc_dec_skill_point	},
			{ "get_skill_point",	pc_get_skill_point	},

			{ "get_channel_id",		pc_get_channel_id	},

			{ "give_poly_marble",	pc_give_poly_marble	},
			{ "get_sig_items",		pc_get_sig_items	},

			{ "charge_cash",		pc_charge_cash		},

			{ "get_informer_type",	pc_get_informer_type	},	//���� ���� ���
			{ "get_informer_item",  pc_get_informer_item	},

			{ "give_award",			pc_give_award			},	//�Ϻ� ������ �ѹ��� �ݱ� ����
			{ "give_award_socket",	pc_give_award_socket	},	//�� �κ��丮�� ������ ����. ���� ������ ���� �Լ�.

			{ "get_killee_drop_pct",	pc_get_killee_drop_pct	}, /* mob_vnum.kill �̺�Ʈ���� killee�� pc���� level ����, pc�� �����̾� ����� ����� ����� ������ ��� Ȯ��.
																    * return ���� (����, �и�).
																    * (���� �����ѵ�, CreateDropItem�� GetDropPct�� iDeltaPercent, iRandRange�� return�Ѵٰ� ���� ��.)
																	* (�� ���� �� ������ �Ф�)
																	* ���ǻ��� : kill event������ ����� ��!
																	*/

#ifdef ENABLE_NEWSTUFF
			//pc.set_race0(race=[0. Warrior, 1. Ninja, 2. Sura, 3. Shaman, 4. Lycan])
			{ "set_race0",			pc_set_race0			},
			//if pc.delf("game_option", "block_cocks") then syschat("now you are unsafe") end
			{ "delf",				pc_del_another_quest_flag},	// delete quest flag [return lua boolean: successfulness]
			//pc.make_item0({vnum|locale_name, count}, {socket1,2,3}, {type1, value1, ... , type7, value7}, gstate(=0: giveitem, 1: dropitem, 2: drop_item_with_leadership)[, countdown_in_secs_before_ownership_vanish(=if <=10 would be 30; default=10->30)])
			{ "make_item0",			pc_make_item0		},	// [return lua boolean: successfulness]
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
#ifdef ENABLE_WOLFMAN_CHARACTER
			{ "is_flag_bleeding",		pc_if_bleeding		},
#endif
			{ "is_flag_slow",			pc_if_slow			},
			{ "is_flag_stun",			pc_if_stun			},
			//set_flags (bool) [return nothing]
			{ "set_flag_fire",			pc_sf_fire			},
			{ "set_flag_invisible",		pc_sf_invisible		},
			{ "set_flag_poison",		pc_sf_poison		},
#ifdef ENABLE_WOLFMAN_CHARACTER
			{ "set_flag_bleeding",		pc_sf_bleeding		},
#endif
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
			// pc.equip_slot0(cell)
			{ "equip_slot0",		pc_equip_slot0		},	// [return lua boolean: successfulness]
			// pc.unequip_slot0(cell)
			{ "unequip_slot0",		pc_unequip_slot0	},	// [return lua boolean: successfulness]
			{ "is_available0",		pc_is_available0	},	// [return lua boolean]
			{ "give_random_book0",	pc_give_random_book0},	// [return lua boolean]
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

#ifdef ENABLE_ACCE_SYSTEM
			{"open_acce",				pc_open_acce	},
			{"open_sash",				pc_open_acce	},
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






