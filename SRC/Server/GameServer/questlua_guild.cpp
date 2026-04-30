#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"

#include "questlua.h"
#include "questmanager.h"
#include "desc_client.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#include "utils.h"
#include "guild.h"
#include "guild_manager.h"
#include "ecs/quest_helpers.hpp"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

namespace quest
{
    namespace
    {
        LPCHARACTER GetGuildQuestCharacter()
        {
            return CQuestManager::instance().GetCurrentCharacterPtr();
        }

        CGuild* GetGuildFromECSOrLegacy(lua_State* L)
        {
            entt::entity e = CQuestManager::instance().GetPCEntity(L);
            if (auto* gm = ECS_TryGet<ecs::GuildMembership>(e))
                return gm->guild;

            LPCHARACTER ch = GetGuildQuestCharacter();
            return ch ? ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch)) : nullptr;
        }
    }

	//
	// "guild" Lua functions
	//
    ALUA(guild_around_ranking_string)
    {
        // migrated from CHARACTER::GetGuild()
        CGuild* guild = GetGuildFromECSOrLegacy(L);
        if (!guild)
            lua_pushstring(L, "");
        else
        {
            char szBuf[4096+1];
            CGuildManager::instance().GetAroundRankString(guild->GetID(), szBuf, sizeof(szBuf));
            lua_pushstring(L, szBuf);
        }
        return 1;
    }

    ALUA(guild_high_ranking_string)
    {
        // migrated from CHARACTER::GetGuild()
        uint32_t dwMyGuild = 0;
        if (CGuild* guild = GetGuildFromECSOrLegacy(L))
            dwMyGuild = guild->GetID();
        char szBuf[4096+1];
        CGuildManager::instance().GetHighRankString(dwMyGuild, szBuf, sizeof(szBuf));
        lua_pushstring(L, szBuf);
        return 1;
    }

    ALUA(guild_get_ladder_point)
    {
        // migrated from CHARACTER::GetGuild()->GetLadderPoint()
        CGuild* guild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, guild ? guild->GetLadderPoint() : -1);
        return 1;
    }

    ALUA(guild_get_rank)
    {
        // migrated from CHARACTER::GetGuild()
        CGuild* guild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, guild ? CGuildManager::instance().GetRank(guild) : -1);
        return 1;
    }

    ALUA(guild_is_war)
    {
        // migrated from CHARACTER::GetGuild()->UnderWar()
        if (!lua_isnumber(L, 1))
        {
            sys_err("invalid argument");
            return 0;
        }
        CGuild* guild = GetGuildFromECSOrLegacy(L);
        lua_pushboolean(L, (guild && guild->UnderWar((uint32_t)lua_tonumber(L, 1))) ? 1 : 0);
        return 1;
    }

	ALUA(guild_name)
	{
        // migrated from CHARACTER::GetGuild()->GetName()
		if (!lua_isnumber(L, 1))
		{
			sys_err("invalid argument");
			return 0;
		}

		CGuild * pkGuild = CGuildManager::instance().FindGuild((uint32_t) lua_tonumber(L, 1));

		if (pkGuild)
			lua_pushstring(L, pkGuild->GetName());
		else
			lua_pushstring(L, "");

		return 1;
	}

	ALUA(guild_level)
	{
        // migrated from CHARACTER::GetGuild()->GetLevel()
		luaL_checknumber(L, 1);

		CGuild * pkGuild = CGuildManager::instance().FindGuild((uint32_t) lua_tonumber(L, 1));

		if (pkGuild)
			lua_pushnumber(L, pkGuild->GetLevel());
		else
			lua_pushnumber(L, 0);

		return 1;
	}

    ALUA(guild_war_enter)
    {
        // migrated from CHARACTER::GetGuild()->GuildWarEntryAccept()
        // DUAL-PATH: ECS update + legacy call
        if (!lua_isnumber(L, 1))
        {
            sys_err("invalid argument");
            return 0;
        }
        LPCHARACTER ch = GetGuildQuestCharacter();
        CGuild* guild = GetGuildFromECSOrLegacy(L);
        if (guild && ch)
            guild->GuildWarEntryAccept((uint32_t) lua_tonumber(L, 1), ch);
        return 0;
    }

    ALUA(guild_get_any_war)
    {
        // migrated from CHARACTER::GetGuild()->UnderAnyWar()
        CGuild* guild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, guild ? guild->UnderAnyWar() : 0);
        return 1;
    }

	ALUA(guild_get_name)
	{
        // migrated from CHARACTER::GetGuild()->GetName()
		if (!lua_isnumber(L, 1))
		{
			lua_pushstring(L,  "");
			return 1;
		}

		CGuild * pkGuild = CGuildManager::instance().FindGuild((uint32_t) lua_tonumber(L, 1));

		if (pkGuild)
			lua_pushstring(L, pkGuild->GetName());
		else
			lua_pushstring(L, "");

		return 1;
	}

    ALUA(guild_war_bet)
    {
        // migrated from CHARACTER::GetGuild()
        // DUAL-PATH: ECS update + legacy call
        if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3))
        {
            sys_err("invalid argument");
            return 0;
        }
        entt::entity e = CQuestManager::instance().GetPCEntity(L);
        if (auto* gm = ECS_TryGet<ecs::GuildMembership>(e))
        {
            if (!gm->guild)
                return 0;
        }
        LPCHARACTER ch = GetGuildQuestCharacter();
        if (!ch || !ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
            return 0;
        TPacketGDGuildWarBet p;
        p.dwWarID = (uint32_t) lua_tonumber(L, 1);
        strlcpy(p.szLogin, ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->GetAccountTable().login, sizeof(p.szLogin));
        p.dwGuild = (uint32_t) lua_tonumber(L, 2);
        p.dwGold = (uint32_t) lua_tonumber(L, 3);
        LOG_INFO("GUILD_WAR_BET: {} login {} war_id {} guild {} gold {}", ((ch)->GetName()), p.szLogin, p.dwWarID, p.dwGuild, p.dwGold);
        db_clientdesc->DBPacket(HEADER_GD_GUILD_WAR_BET, 0, &p, sizeof(p));
        return 0;
    }

	ALUA(guild_is_bet)
	{
        // migrated from CHARACTER::GetDesc()
		if (!lua_isnumber(L, 1))
		{
			sys_err("invalid argument");
			lua_pushboolean(L, true);
			return 1;
		}

		bool bBet = CGuildManager::instance().IsBet((uint32_t) lua_tonumber(L, 1),
				ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(CQuestManager::instance().GetCurrentCharacterPtr()))->GetAccountTable().login);

		lua_pushboolean(L, bBet);
		return 1;
	}

	ALUA(guild_get_warp_war_list)
	{
        // migrated from CHARACTER::GetGuild()
		FBuildLuaGuildWarList f(L);
		CGuildManager::instance().for_each_war(f);
		return 1;
	}

	ALUA(guild_get_reserve_war_table)
	{
        // migrated from CHARACTER::GetGuild()
		std::vector<CGuildWarReserveForGame *> & con = CGuildManager::instance().GetReserveWarRef();

		int i = 0;
		std::vector<CGuildWarReserveForGame *>::iterator it = con.begin();

		LOG_INFO("con.size(): {}", con.size());

		// stack : table1
		lua_newtable(L);

		while (it != con.end())
		{
			TGuildWarReserve * p = &(*(it++))->data;

			if (p->bType != GUILD_WAR_TYPE_BATTLE)
				continue;

			lua_newtable(L);

			LOG_INFO("con.size(): {} {} {} handi {}", p->dwID, p->dwGuildFrom, p->dwGuildTo, p->lHandicap);

			// stack : table1 table2
			lua_pushnumber(L, p->dwID);
			// stack : table1 table2 dwID
			lua_rawseti(L, -2, 1);

			// stack : table1 table2
			if (p->lPowerFrom > p->lPowerTo)
				lua_pushnumber(L, p->dwGuildFrom);
			else
				lua_pushnumber(L, p->dwGuildTo);
			// stack : table1 table2 guildfrom
			lua_rawseti(L, -2, 2);

			// stack : table1 table2
			if (p->lPowerFrom > p->lPowerTo)
				lua_pushnumber(L, p->dwGuildTo);
			else
				lua_pushnumber(L, p->dwGuildFrom);
			// stack : table1 table2 guildto
			lua_rawseti(L, -2, 3);

			lua_pushnumber(L, p->lHandicap);
			lua_rawseti(L, -2, 4);

			// stack : table1 table2
			lua_rawseti(L, -2, ++i);
		}

		return 1;
	}

    ALUA(guild_get_member_count)
    {
        // migrated from CHARACTER::GetGuild()->GetMemberCount()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, pGuild ? pGuild->GetMemberCount() : 0);
        return 1;
    }

    ALUA(guild_change_master)
    {
        // migrated from CHARACTER::GetGuild()->ChangeMasterTo()
        // DUAL-PATH: ECS GuildMembership check + legacy call
        LPCHARACTER ch = GetGuildQuestCharacter();
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        if (!ch)
        {
            lua_pushnumber(L, 4);
            return 1;
        }
        if ( pGuild != nullptr)
        {
            if ( pGuild->GetMasterPID() == ((ch)->GetPlayerID()) )
            {
                if ( lua_isstring(L, 1) == false )
                    lua_pushnumber(L, 0);
                else
                {
                    bool ret = pGuild->ChangeMasterTo(pGuild->GetMemberPID(lua_tostring(L, 1)));
                    lua_pushnumber(L, ret == false ? 2 : 3 );
                }
            }
            else
                lua_pushnumber(L, 1);
        }
        else
            lua_pushnumber(L, 4);
        return 1;
    }

    ALUA(guild_change_master_with_limit)
    {
        // migrated from CHARACTER::GetGuild()->ChangeMasterTo()
        // DUAL-PATH: ECS GuildMembership check + legacy call
        LPCHARACTER ch = GetGuildQuestCharacter();
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        if (!ch)
        {
            lua_pushnumber(L, 4);
            return 1;
        }
        if ( pGuild != nullptr)
        {
            if ( pGuild->GetMasterPID() == ((ch)->GetPlayerID()) )
            {
                if ( lua_isstring(L, 1) == false )
                    lua_pushnumber(L, 0);
                else
                {
                    LPCHARACTER pNewMaster = CHARACTER_MANAGER::instance().FindPC(lua_tostring(L,1));
                    if ( pNewMaster != nullptr)
                    {
                        if ( pNewMaster->GetLevel() < lua_tonumber(L, 2) )
                            lua_pushnumber(L, 6);
                        else
                        {
                            int nBeOtherLeader = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pNewMaster), "change_guild_master.be_other_leader");
                            CQuestManager::instance().GetPC( ((ch)->GetPlayerID()) );
                            if ( lua_toboolean(L, 6) == true ) nBeOtherLeader = 0;
                            if ( nBeOtherLeader > get_global_time() )
                                lua_pushnumber(L, 7);
                            else
                            {
                                bool ret = pGuild->ChangeMasterTo(pGuild->GetMemberPID(lua_tostring(L, 1)));
                                if ( ret == false )
                                    lua_pushnumber(L, 2);
                                else
                                {
                                    lua_pushnumber(L, 3);
                                    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(pNewMaster), "change_guild_master.be_other_leader", 0);
                                    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(pNewMaster), "change_guild_master.be_other_member", 0);
                                    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(pNewMaster), "change_guild_master.resign_limit", (int)lua_tonumber(L, 3));
                                    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "change_guild_master.be_other_leader", (int)lua_tonumber(L, 4));
                                    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "change_guild_master.be_other_member", (int)lua_tonumber(L, 5));
                                    ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "change_guild_master.resign_limit", 0);
                                }
                            }
                        }
                    }
                    else
                        lua_pushnumber(L, 5);
                }
            }
            else
                lua_pushnumber(L, 1);
        }
        else
            lua_pushnumber(L, 4);
        return 1;
    }

#ifdef ADVANCED_GUILD_INFO
    ALUA(guild_get_wins)
    {
        // migrated from CHARACTER::GetGuild()->GetGuildWarWinCount()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, pGuild ? pGuild->GetGuildWarWinCount() : 0);
        return 1;
    }
#endif

#ifdef ENABLE_NEWSTUFF
    ALUA(guild_get_sp0)
    {
        // migrated from CHARACTER::GetGuild()->GetSP()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, (pGuild != nullptr) ? pGuild->GetSP() : 0);
        return 1;
    }

    ALUA(guild_get_maxsp0)
    {
        // migrated from CHARACTER::GetGuild()->GetMaxSP()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, (pGuild != nullptr) ? pGuild->GetMaxSP() : 0);
        return 1;
    }

    ALUA(guild_get_money0)
    {
        // migrated from CHARACTER::GetGuild()->GetGuildMoney()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, (pGuild != nullptr) ? pGuild->GetGuildMoney() : 0);
        return 1;
    }

    ALUA(guild_get_max_member0)
    {
        // migrated from CHARACTER::GetGuild()->GetMaxMemberCount()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, (pGuild != nullptr) ? pGuild->GetMaxMemberCount() : 0);
        return 1;
    }

    ALUA(guild_get_total_member_level0)
    {
        // migrated from CHARACTER::GetGuild()->GetTotalLevel()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, (pGuild != nullptr) ? pGuild->GetTotalLevel() : 0);
        return 1;
    }

    ALUA(guild_has_land0)
    {
        // migrated from CHARACTER::GetGuild()->HasLand()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushboolean(L, (pGuild != nullptr) ? pGuild->HasLand() : false);
        return 1;
    }

    ALUA(guild_get_win_count0)
    {
        // migrated from CHARACTER::GetGuild()->GetGuildWarWinCount()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, (pGuild != nullptr) ? pGuild->GetGuildWarWinCount() : 0);
        return 1;
    }

    ALUA(guild_get_draw_count0)
    {
        // migrated from CHARACTER::GetGuild()->GetGuildWarDrawCount()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, (pGuild != nullptr) ? pGuild->GetGuildWarDrawCount() : 0);
        return 1;
    }

    ALUA(guild_get_loss_count0)
    {
        // migrated from CHARACTER::GetGuild()->GetGuildWarLossCount()
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, (pGuild != nullptr) ? pGuild->GetGuildWarLossCount() : 0);
        return 1;
    }

    ALUA(guild_add_comment0)
    {
        // migrated from CHARACTER::GetGuild()->AddComment()
        // DUAL-PATH: ECS update + legacy call
        LPCHARACTER ch = GetGuildQuestCharacter();
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        if (pGuild && ch)
            pGuild->AddComment(ch, std::string(lua_tostring(L, 1)));
        return 0;
    }

	// ALUA(guild_set_war_data0)
	// {
		// LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		// CGuild* pGuild = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch));
		// if (pGuild)
			// pGuild->SetWarData(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3));
		// return 0;
	// }

	ALUA(guild_get_skill_level0)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CGuild* pGuild = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch));
		lua_pushnumber(L, (pGuild)?pGuild->GetSkillLevel(lua_tonumber(L, 1)):0);
		return 1;
	}

    ALUA(guild_set_skill_level0)
    {
        // migrated from CHARACTER::GetGuild()->SetSkillLevel()
        // DUAL-PATH: ECS update + legacy call
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        if (pGuild)
            pGuild->SetSkillLevel(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_isnumber(L, 3) ? lua_tonumber(L, 3) : 0);
        return 0;
    }

	ALUA(guild_get_skill_point0)
	{
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		CGuild* pGuild = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(ch));
		lua_pushnumber(L, (pGuild)?pGuild->GetSkillPoint():0);
		return 1;
	}

    ALUA(guild_set_skill_point0)
    {
        // migrated from CHARACTER::GetGuild()->SetSkillPoint()
        // DUAL-PATH: ECS update + legacy call
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        if (pGuild)
            pGuild->SetSkillPoint(lua_tonumber(L, 1));
        return 0;
    }

	ALUA(guild_get_exp_level0)
	{
        // migrated from CHARACTER::GetGuild()
		lua_pushnumber(L, guild_exp_table2[MINMAX(0, lua_tonumber(L, 1) ,GUILD_MAX_LEVEL)]);
		return 1;
	}

    ALUA(guild_offer_exp0)
    {
        // migrated from CHARACTER::GetGuild()->OfferExp()
        // DUAL-PATH: ECS update + legacy call
        LPCHARACTER ch = GetGuildQuestCharacter();
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        if (!pGuild || !ch)
        {
            lua_pushboolean(L, false);
            return 1;
        }
        uint32_t offer = lua_tonumber(L, 1);
        if (pGuild->GetLevel() >= GUILD_MAX_LEVEL)
            lua_pushboolean(L, false);
        else
        {
            offer /= 100;
            offer *= 100;
            lua_pushboolean(L, pGuild->OfferExp(ch, offer) ? true : false);
        }
        return 1;
    }

    ALUA(guild_give_exp0)
    {
        // migrated from CHARACTER::GetGuild()->GuildPointChange()
        // DUAL-PATH: ECS update + legacy call
        CGuild* pGuild = GetGuildFromECSOrLegacy(L);
        if (!pGuild)
            return 0;
        pGuild->GuildPointChange(POINT_EXP, lua_tonumber(L, 1) / 100, true);
        return 0;
    }

#endif

    ALUA(guild_get_id)
    {
        // migrated from CHARACTER::GetGuild()->GetID()
        CGuild* guild = GetGuildFromECSOrLegacy(L);
        lua_pushnumber(L, guild ? guild->GetID() : 0);
        return 1;
    }

	void RegisterGuildFunctionTable()
	{
		luaL_reg guild_functions[] =
		{
			{ "get_rank",				guild_get_rank				},
			{ "get_ladder_point",		guild_get_ladder_point		},
			{ "high_ranking_string",	guild_high_ranking_string	},
			{ "around_ranking_string",	guild_around_ranking_string	},
			{ "name",					guild_name					},
			{ "level",					guild_level					},
			{ "is_war",					guild_is_war				},
			{ "war_enter",				guild_war_enter				},
			{ "get_any_war",			guild_get_any_war			},
			{ "get_reserve_war_table",	guild_get_reserve_war_table	},
			{ "get_name",				guild_get_name				},
			{ "war_bet",				guild_war_bet				},
			{ "is_bet",					guild_is_bet				},
			{ "get_warp_war_list",		guild_get_warp_war_list		},
			{ "get_member_count",		guild_get_member_count		},
			{ "change_master",			guild_change_master			},
			{ "change_master_with_limit",			guild_change_master_with_limit			},
#ifdef ADVANCED_GUILD_INFO
			{ "get_wins",				guild_get_wins				},
#endif
#ifdef ENABLE_NEWSTUFF
			{ "get_sp0",				guild_get_sp0				},	// get guild sp [return lua number]
			{ "get_maxsp0",				guild_get_maxsp0			},	// get guild maxsp [return lua number]
			{ "get_money0",				guild_get_money0			},	// get money guild [return lua number]
			{ "get_max_member0",		guild_get_max_member0		},	// get max joinable members [return lua number]
			{ "get_total_member_level0",	guild_get_total_member_level0	},	// get the sum of all the members' level [return lua number]
			{ "has_land0",				guild_has_land0				},	// get whether guild has a land or not [return lua boolean]
			{ "get_win_count0",			guild_get_win_count0		},	// get guild wins [return lua number]
			{ "get_draw_count0",		guild_get_draw_count0		},	// get guild draws [return lua number]
			{ "get_loss_count0",		guild_get_loss_count0		},	// get guild losses [return lua number]
			{ "add_comment0",			guild_add_comment0			},	// add a comment into guild notice board [return nothing]
			// guild.ladder_point0(point)
			// guild.set_war_data0(win, draw, loss)
			// { "set_war_data0",			guild_set_war_data0			},	// set guild win/draw/loss [return nothing]
			{ "get_skill_level0",		guild_get_skill_level0		},	// get guild skill level [return lua number]
			{ "set_skill_level0",		guild_set_skill_level0		},	// set guild skill level [return nothing]
			{ "get_skill_point0",		guild_get_skill_point0		},	// get guild skill points [return lua number]
			{ "set_skill_point0",		guild_set_skill_point0		},	// set guild skill points [return nothing]
			{ "get_exp_level0",			guild_get_exp_level0		},	// get how much exp is necessary for such <level> [return lua number]
			{ "offer_exp0",				guild_offer_exp0			},	// give player's <exp> to guild [return lua boolean=successfulness]
			{ "give_exp0",				guild_give_exp0				},	// give <exp> to guild [return nothing]
#endif
			{"get_id", guild_get_id},
			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("guild", guild_functions);
	}
}


