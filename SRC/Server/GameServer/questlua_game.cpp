#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/EntityFactory.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/SessionSystem.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/systems/GayaSystem.hpp"
#include "ecs/Registry.hpp"
#include "questlua.h"
#include "questmanager.h"
#include "desc_client.h"
#include "char_interface.hpp"
#include "item_manager.h"
#include "item.h"
#include "cmd.h"
#include "packet.h"
#include "ecs/quest_helpers.hpp"

#ifdef ADVANCED_GUILD_INFO
	#include "guild_manager.h"
	#include "guild.h"
	#ifdef ENABLE_NEWSTUFF
	#include "db.h"
	#endif
#endif

#ifdef ENABLE_DICE_SYSTEM
#include "party.h"
#endif
#ifdef ENABLE_EVENT_MANAGER
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
#endif
#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

namespace quest
{
	ALUA(game_set_event_flag)
	{
		// migrated from CHARACTER::RequestSetEventFlag
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();

		if (lua_isstring(L,1) && lua_isnumber(L, 2))
			q.RequestSetEventFlag(lua_tostring(L,1), (int)lua_tonumber(L,2));

		return 0;
	}

	ALUA(game_get_event_flag)
	{
		// migrated from CHARACTER::GetEventFlag
		// DUAL-PATH: legacy fallback during migration window
		CQuestManager& q = CQuestManager::instance();

		if (lua_isstring(L,1))
			lua_pushnumber(L, q.GetEventFlag(lua_tostring(L,1)));
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(game_request_make_guild)
	{
		// migrated from CHARACTER::GetDesc()->Packet
		// DUAL-PATH: legacy only during migration window
		CQuestManager& q = CQuestManager::instance();
		LPDESC d = ecs::PlayerRuntime::GetDesc(q.GetCurrentPCEntity());
		if (d)
		{
			uint8_t header = HEADER_GC_REQUEST_MAKE_GUILD;
			d->Packet(&header, 1);
		}
		return 0;
	}

	ALUA(game_get_safebox_level)
	{
		// migrated from CHARACTER::GetSafeboxSize()
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		lua_pushnumber(L, ecs::SessionSystem::GetSafeboxSize(character) / SAFEBOX_PAGE_SIZE);
		return 1;
	}

	ALUA(game_set_safebox_level)
	{
		// migrated from CHARACTER::SetSafeboxSize()
		// DUAL-PATH: ECS update + legacy call during migration window
		CQuestManager& q = CQuestManager::instance();
		entt::entity e = q.GetPCEntity(L);
		int32_t size = static_cast<int32_t>(lua_tonumber(L, -1));
		LPDESC desc = ecs::PlayerRuntime::GetDesc(e);
		if (!desc)
			return 0;

		TSafeboxChangeSizePacket p;
		p.dwID = ecs::PlayerRuntime::GetAccountID(e);
		p.bSize = size;
		db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_CHANGE_SIZE, desc->GetHandle(), &p, sizeof(p));
		ecs::SessionSystem::SetSafeboxSize(e, SAFEBOX_PAGE_SIZE * size);
		return 0;
	}

	ALUA(game_open_safebox)
	{
		// migrated from CHARACTER::SetSafeboxOpenPosition()
		// DUAL-PATH: ECS update + legacy call during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		if (chEntity != entt::null && g_registry.valid(chEntity))
		{
			auto& sb = g_registry.get_or_emplace<ecs::SafeboxRef>(chEntity);
			sb.isOpening = true;
			g_registry.emplace_or_replace<ecs::DirtyTag>(chEntity);
		}
		ecs::SessionSystem::SetSafeboxOpenPosition(chEntity);
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "ShowMeSafeboxPassword");
		return 0;
	}

	ALUA(game_open_mall)
	{
		// migrated from CHARACTER::SetSafeboxOpenPosition
		// DUAL-PATH: legacy only during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		ecs::SessionSystem::SetSafeboxOpenPosition(chEntity);
		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "ShowMeMallPassword");
		return 0;
	}

	ALUA(game_drop_item)
	{
		// migrated from CHARACTER::DropItem
		// DUAL-PATH: legacy only during migration window
		//
		// Syntax: game.drop_item(50050, 1)
		//
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		uint32_t item_vnum = (uint32_t) lua_tonumber(L, 1);
		int count = (int) lua_tonumber(L, 2);
		int32_t x = ecs::PlayerRuntime::GetX(chEntity);
		int32_t y = ecs::PlayerRuntime::GetY(chEntity);

		const entt::entity item = ITEM_MANAGER::instance().CreateItem(item_vnum, count);

		if (!ItemSystem::IsValidItem(item))
		{
			sys_err("cannot create item vnum {} count {}", item_vnum, count);
			return 0;
		}

		PIXEL_POSITION pos;
		pos.x = x + number(-200, 200);
		pos.y = y + number(-200, 200);

		if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
			item, ecs::PlayerRuntime::GetMapIndex(chEntity), pos, 300))
			ItemSystem::DestroyItemEntityEcs(item, "QUEST_DROP_GROUND_FAILED");

		return 0;
	}

	ALUA(game_drop_item_with_ownership)
	{
		// migrated from CHARACTER::DropItemWithOwnership
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		entt::entity item = entt::null;
		switch (lua_gettop(L))
		{
		case 1:
			item = ITEM_MANAGER::instance().CreateItem((uint32_t) lua_tonumber(L, 1));
			break;
		case 2:
		case 3:
			item = ITEM_MANAGER::instance().CreateItem((uint32_t) lua_tonumber(L, 1), (int) lua_tonumber(L, 2));
			break;
		default:
			return 0;
		}

		if (!ItemSystem::IsValidItem(item))
		{
			return 0;
		}

		if (lua_isnumber(L, 3))
		{
			int sec = (int) lua_tonumber(L, 3);
			if (sec <= 0)
			{
				ItemSystem::SetGroundOwnership(item, chEntity);
			}
			else
			{
				ItemSystem::SetGroundOwnership(item, chEntity, sec);
			}
		}
		else
			ItemSystem::SetGroundOwnership(item, chEntity);

		PIXEL_POSITION pos;
		pos.x = ecs::PlayerRuntime::GetX(chEntity) + number(-200, 200);
		pos.y = ecs::PlayerRuntime::GetY(chEntity) + number(-200, 200);

		if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
			item, ecs::PlayerRuntime::GetMapIndex(chEntity), pos, 300))
			ItemSystem::DestroyItemEntityEcs(item, "QUEST_OWNED_DROP_GROUND_FAILED");

		return 0;
	}

#ifdef ENABLE_DICE_SYSTEM
	ALUA(game_drop_item_with_ownership_and_dice)
	{
		// migrated from CHARACTER::DropItemWithOwnership
		// DUAL-PATH: legacy only during migration window
		entt::entity item = entt::null;
		switch (lua_gettop(L))
		{
		case 1:
			item = ITEM_MANAGER::instance().CreateItem((uint32_t) lua_tonumber(L, 1));
			break;
		case 2:
		case 3:
			item = ITEM_MANAGER::instance().CreateItem((uint32_t) lua_tonumber(L, 1), (int) lua_tonumber(L, 2));
			break;
		default:
			return 0;
		}

		if (!ItemSystem::IsValidItem(item))
		{
			return 0;
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		entt::entity owner = chEntity;
		if (ecs::SocialSystem::GetParty(chEntity))
			owner = ItemSystem::RollPartyDropOwnership(item, chEntity);

		if (lua_isnumber(L, 3))
		{
			int sec = (int) lua_tonumber(L, 3);
			if (sec <= 0)
			{
				ItemSystem::SetGroundOwnership(item, owner);
			}
			else
			{
				ItemSystem::SetGroundOwnership(item, owner, sec);
			}
		}
		else
			ItemSystem::SetGroundOwnership(item, owner);

		PIXEL_POSITION pos;
		pos.x = ecs::PlayerRuntime::GetX(chEntity) + number(-200, 200);
		pos.y = ecs::PlayerRuntime::GetY(chEntity) + number(-200, 200);

		if (!ItemSystem::PlaceItemOnGroundLegacyBoundary(
			item, ecs::PlayerRuntime::GetMapIndex(chEntity), pos, 300))
			ItemSystem::DestroyItemEntityEcs(item, "QUEST_DICE_DROP_GROUND_FAILED");

		return 0;
	}
#endif

	ALUA(game_web_mall)
	{
		// migrated from CHARACTER::do_in_game_mall
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		open_in_game_mall(chEntity);
		return 0;
	}


#ifdef ENABLE_GAYA_SYSTEM
	ALUA(game_open_gaya_c)
	{
		// migrated from CHARACTER::ChatPacket
		// DUAL-PATH: legacy only during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		if (quest::CQuestManager::instance().GetEventFlag("gaya_disable") == 1)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 734, "");
#endif
			return 0;
		}

		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "OpenGuiGaya");
		return 0;
	}

	ALUA(game_open_gaya_m)
	{
		// migrated from CHARACTER::ChatPacket
		// DUAL-PATH: legacy only during migration window
		CQuestManager& q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		if (quest::CQuestManager::instance().GetEventFlag("gaya_disable") == 1)
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 734, "");
#endif
			return 0;
		}

		if (!GayaSystem::CheckItemsFull(chEntity))
		{
			GayaSystem::UpdateItems(chEntity);
			GayaSystem::InfoMarket(chEntity);
			GayaSystem::StartCheckTimeMarket(chEntity);
		}
		else
		{
			GayaSystem::InfoMarket(chEntity);
			GayaSystem::StartCheckTimeMarket(chEntity);
		}

		ecs::ChatSystem::Send(chEntity, CHAT_TYPE_COMMAND, "OpenGuiGayaMarket");
		return 0;
	}
#endif

#ifdef ADVANCED_GUILD_INFO
	ALUA(game_give_guild_reward)
	{
		// migrated from CHARACTER::GetGuild()->GiveReward
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			sys_err("Wrong Input");
			return 0;
		}
		int guild_id = (int)lua_tonumber(L, 1);
		int item_reward = (int)lua_tonumber(L, 2);
		CGuild * g = CGuildManager::instance().FindGuild(guild_id);
		g->GiveReward(item_reward);
		return 1;
	}

	ALUA(game_reset_guild_war_stats)
	{
		// migrated from CHARACTER::DBPacket
		// DUAL-PATH: legacy only during migration window
		// CGuildManager::instance().ResetStatsToAll();
		TPacketGuildReset p;
		p.stat = 0;
		db_clientdesc->DBPacket(HEADER_GD_GUILD_RESET, 0, &p, sizeof(p));
		return 1;
	}

	ALUA(game_mysql_query)
	{
		// migrated from CHARACTER::DirectQuery
		// DUAL-PATH: legacy only during migration window
		//MYSQL_FIELD *field;
		SQLMsg* run = DBManager::instance().DirectQuery(lua_tostring(L,1));
		MYSQL_RES* res=run->Get()->pSQLResult;
		if (!res){
			lua_pushnumber(L, 0);
			return 0;
		}
		MYSQL_ROW row;
		lua_newtable(L);
		int rowcount = 1;
		while((row = mysql_fetch_row(res))){
			lua_newtable(L);
			lua_pushnumber(L, rowcount);
			lua_pushvalue(L, -2);
			lua_settable(L, -4);
			unsigned int fields = mysql_num_fields(res);
			for(unsigned int i = 0; i < fields; i++){
				lua_pushnumber(L, i + 1);
				lua_pushstring(L, row[i]);
				lua_settable(L, -3);
			}
			lua_pop(L, 1);
			rowcount++;
		}
		return 1;
	}
#endif
#ifdef ENABLE_EVENT_MANAGER
	int game_check_event(lua_State* L)
	{
		// migrated from CHARACTER::CheckEventIsActive
		// DUAL-PATH: legacy only during migration window
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			lua_pushboolean(L, false);
			return 1;
		}
		auto it = CHARACTER_MANAGER::instance().CheckEventIsActive(lua_tonumber(L, 1), lua_tonumber(L, 2));
		lua_pushboolean(L, it != nullptr);
		return 1;
	}
#endif
	void RegisterGameFunctionTable()
	{
		luaL_reg game_functions[] =
		{
			{ "get_safebox_level",			game_get_safebox_level			},
			{ "request_make_guild",			game_request_make_guild			},
			{ "set_safebox_level",			game_set_safebox_level			},
			{ "open_safebox",				game_open_safebox				},
			{ "open_mall",					game_open_mall					},
			{ "get_event_flag",				game_get_event_flag				},
			{ "set_event_flag",				game_set_event_flag				},
			{ "drop_item",					game_drop_item					},
			{ "drop_item_with_ownership",	game_drop_item_with_ownership	},
#ifdef ADVANCED_GUILD_INFO
			{ "give_guild_reward",			game_give_guild_reward			},
			{ "mysql_query",				game_mysql_query 				},
			{ "reset_guild_war_stats",		game_reset_guild_war_stats		},
#endif
#ifdef ENABLE_DICE_SYSTEM
			{ "drop_item_with_ownership_and_dice",	game_drop_item_with_ownership_and_dice	},
#endif
			{ "open_web_mall",				game_web_mall					},
#ifdef ENABLE_GAYA_SYSTEM
			{ "open_gaya",					game_open_gaya_c				},
			{ "open_gaya_market",			game_open_gaya_m 				},
#endif
#ifdef ENABLE_EVENT_MANAGER
			{ "check_event",		game_check_event			},
#endif

			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("game", game_functions);
	}
}




