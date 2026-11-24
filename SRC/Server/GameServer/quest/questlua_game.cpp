#include "stdafx.h"
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/EntityFactory.hpp"
#include "../ecs/systems/GayaSystem.hpp"
#include "../item/TypedStoneExchange.hpp"
#include "../ecs/Registry.hpp"
#include "questlua.h"
#include "questmanager.h"
#include "desc_client.h"
#include "char_interface.hpp"
#include "cmd.h"
#include "packet.h"
#include "../ecs/quest_helpers.hpp"
#include "../item/TypedGuildRewards.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/ItemStore.hpp"
#include "../item/TypedLoot.hpp"
#include "../social/TypedQuiz.hpp"

#ifdef ADVANCED_GUILD_INFO
	#include "guild_manager.h"
	#include "guild.h"
	#ifdef ENABLE_NEWSTUFF
	#include "db.h"
	#endif
#endif

#ifdef ENABLE_EVENT_MANAGER
#include "char_manager.h"
#include "../ecs/CharacterAccessors.hpp"
#endif
#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

namespace quest
{
	int game_set_event_flag(lua_State* L);
	int game_get_event_flag(lua_State* L);
	int game_get_event_flag_string(lua_State* L);

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

	ALUA(game_open_safebox)
	{
		const auto character = CQuestManager::instance().GetCurrentPCEntity();
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ShowMeSafeboxPassword");
		return 0;
	}

	ALUA(game_open_mall)
	{
		const auto character = CQuestManager::instance().GetCurrentPCEntity();
		ecs::ChatSystem::Send(character, CHAT_TYPE_COMMAND, "ShowMeMallPassword");
		return 0;
	}

	ALUA(game_drop_item)
	{
		LOG_ERROR("QUEST {}: legacy drop API requires a typed quest drop",
			CQuestManager::instance().GetCurrentQuestName());
		return 0;
	}

	static std::optional<lua_Integer> QuestItemInteger(lua_State* L, int index, lua_Integer minimum, lua_Integer maximum)
	{
		int valid = 0;
		const auto value = lua_tointegerx(L, index, &valid);
		return valid && value >= minimum && value <= maximum ? std::optional{value} : std::nullopt;
	}

	static auto QuestItemKey(lua_State* L)
	{
		const auto category = QuestItemInteger(L, 1, 1, UINT16_MAX);
		const auto local = QuestItemInteger(L, 2, 1, UINT32_MAX);
		return auriga::item::ItemKey::Create(static_cast<uint16_t>(category.value_or(0)),
			static_cast<uint32_t>(local.value_or(0)));
	}

	ALUA(game_typed_item_key)
	{
		std::size_t length = 0;
		const char* value = luaL_checklstring(L, 1, &length);
		const std::string_view code(value, length);
		const auto catalog = auriga::item::runtime::CurrentCatalog();
		if (!catalog)
			return 0;
		const auto& definitions = catalog->Entries();
		const auto found = std::ranges::find_if(definitions, [&](const auto& item) {
			return !item.second.base.retired && item.second.base.contentCode == code;
		});
		if (found == definitions.end())
			return 0;
		lua_pushinteger(L, static_cast<std::uint16_t>(found->first.Category()));
		lua_pushinteger(L, found->first.LocalId());
		return 2;
	}

	static int PushTypedItemName(lua_State* L, const auriga::item::ItemCatalogEntry* item, int fallbackIndex)
	{
		std::size_t length = 0;
		const char* provided = lua_tolstring(L, fallbackIndex, &length);
		std::string fallback = provided && length ? std::string(provided, length) : "Tárgy";
		for (char& character : fallback)
			if (!character || std::string_view("[]|;\r\n").find(character) != std::string_view::npos)
				character = ' ';

		const auto validKey = [](char ch) {
			return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
				(ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-';
		};
		if (item && !item->base.nameKey.empty() && std::ranges::all_of(item->base.nameKey, validKey))
			lua_pushfstring(L, "[TYPED_ITEM_NAME key;%s|fallback;%s]", item->base.nameKey.c_str(), fallback.c_str());
		else
			lua_pushlstring(L, fallback.data(), fallback.size());
		return 1;
	}

    ALUA(game_start_typed_quiz)
    {
        const auto answer = luaL_checkinteger(L, 1);
        std::size_t codeLength, questionLength;
        const char* code = luaL_checklstring(L, 2, &codeLength);
        const auto count = luaL_checkinteger(L, 3);
        const auto preset = luaL_checkinteger(L, 4);
        const char* question = luaL_checklstring(L, 5, &questionLength);
        luaL_argcheck(L, count > 0 && count <= UINT32_MAX, 3, "invalid reward quantity");
        luaL_argcheck(L, preset == 0 || preset == 100 || preset == 1000, 4, "invalid quiz preset");
        StartQuestQuiz(CQuestManager::instance().GetPCEntity(L),answer,std::string_view(code,codeLength),
            static_cast<std::uint32_t>(count),static_cast<unsigned>(preset),std::string(question,questionLength));
        return 0;
    }

    ALUA(game_cancel_typed_quiz)
    {
        CloseQuestQuiz(CQuestManager::instance().GetPCEntity(L));
        return 0;
    }

	ALUA(game_typed_item_name)
	{
		const auto catalog = auriga::item::runtime::CurrentCatalog();
		const auto key = QuestItemKey(L);
		return PushTypedItemName(L, catalog && key ? catalog->Find(*key) : nullptr, 3);
	}

	ALUA(game_typed_item_use_name)
	{
		auto& manager = CQuestManager::instance();
		const auto* state = manager.GetCurrentState();
		if (!state || state->co != L || !state->typedItemUse.id || manager.IsInOtherPCBlock())
			return PushTypedItemName(L, nullptr, 1);

		const auto actor = manager.GetCurrentPCEntity();
		const auto* inventory = g_registry.valid(actor) ? g_registry.try_get<ecs::TypedItems>(actor) : nullptr;
		const auto catalog = auriga::item::runtime::CurrentCatalog();
		if (!inventory || !catalog)
			return PushTypedItemName(L, nullptr, 1);

		const auto& items = inventory->state.items;
		const auto source = std::ranges::find_if(items, [&](const auto& row) {
			return row.owner == inventory->state.fence.owner && !row.destroyed &&
				row.id.Value() == state->typedItemUse.id && row.revision == state->typedItemUse.revision;
		});
		return PushTypedItemName(L, source == items.end() ? nullptr : catalog->Find(source->key), 1);
	}

	static int DropQuestItem(lua_State* L, bool claim)
	{
		namespace item = auriga::item;
		const auto level = QuestItemInteger(L, 3, 0, 255);
		const auto count = QuestItemInteger(L, 4, 1, 50000);
		const auto key = QuestItemKey(L);
		const auto catalog = item::runtime::CurrentCatalog();
		const auto* definition = catalog && key ? catalog->Find(*key) : nullptr;
		const auto player = CQuestManager::instance().GetCurrentPCEntity();
		bool placed = false;
		if (player != entt::null && definition && level && count && *count <= definition->base.maxStack &&
			!definition->base.retired && item::SupportsPersistentGrant(*catalog, *definition) &&
			catalog->RefinedBaseStats(*key, *level) && catalog->CurrencyMode(*key) != item::CurrencyTokenMode::GroundGold)
		{
			const auto map = ecs::PlayerRuntime::GetMapIndex(player);
			const auto x = ecs::PlayerRuntime::GetX(player);
			const auto y = ecs::PlayerRuntime::GetY(player);
			const auto z = ecs::PlayerRuntime::GetZ(player);
			const auto claimant = claim ? player : entt::null;
			const item::ItemGrant grant{*key, static_cast<uint16_t>(*level), static_cast<uint32_t>(*count)};
			placed = DropTypedLoot(grant, map, x + number(-200, 200), y + number(-200, 200), z, claimant, 10000) ||
				DropTypedLoot(grant, map, x, y, z, claimant, 10000);
		}
		if (!placed)
			LOG_ERROR("QUEST {}: could not place typed loot", CQuestManager::instance().GetCurrentQuestName());
		lua_pushboolean(L, placed);
		return 1;
	}

	ALUA(game_drop_typed_item) { return DropQuestItem(L, false); }
	ALUA(game_drop_typed_item_with_ownership) { return DropQuestItem(L, true); }

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

        if (!TryOpenTypedStoneExchange(chEntity, ecs::PlayerRuntime::GetQuestNPC(chEntity)))
            ecs::ChatSystem::Send(chEntity, CHAT_TYPE_INFO, "A kőbeváltáshoz keresd fel a kereskedőt.");
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

		GayaSystem::OpenTypedMarket(chEntity);
		return 0;
	}
#endif

#ifdef ADVANCED_GUILD_INFO
	ALUA(game_give_guild_reward)
	{
        const auto guild=luaL_checkinteger(L,1),category=luaL_checkinteger(L,2),local=luaL_checkinteger(L,3);
        const auto level=luaL_checkinteger(L,4),count=luaL_checkinteger(L,5);
        if(guild<=0 || guild>UINT32_MAX || category<=0 || category>UINT16_MAX || local<=0 || local>UINT32_MAX || level<0 || level>UINT16_MAX || count<=0 || count>UINT32_MAX)
            return luaL_error(L,"Invalid native guild reward");
        const auto key=auriga::item::ItemKey::Create(static_cast<std::uint16_t>(category),static_cast<std::uint32_t>(local));
        if(!key || !QueueTypedGuildReward(static_cast<std::uint32_t>(guild),*key,static_cast<std::uint16_t>(level),static_cast<std::uint32_t>(count)))
            return luaL_error(L,"Native guild reward unavailable");
        lua_pushboolean(L,true);return 1;
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
			{ "request_make_guild",			game_request_make_guild			},
			{ "open_safebox",				game_open_safebox				},
			{ "open_mall",					game_open_mall					},
			{ "get_event_flag",				game_get_event_flag				},
			{ "get_event_flag_string", game_get_event_flag_string },
			{ "set_event_flag",				game_set_event_flag				},
			{ "drop_item",					game_drop_item					},
			{ "drop_item_with_ownership",	game_drop_item	},
			{ "drop_typed_item", game_drop_typed_item },
			{ "drop_typed_item_with_ownership", game_drop_typed_item_with_ownership },
			{ "typed_item_key", game_typed_item_key },
			{ "typed_item_name", game_typed_item_name },
            { "start_typed_quiz", game_start_typed_quiz },
            { "cancel_typed_quiz", game_cancel_typed_quiz },
			{ "typed_item_use_name", game_typed_item_use_name },
#ifdef ADVANCED_GUILD_INFO
			{ "give_guild_reward",			game_give_guild_reward			},
			{ "mysql_query",				game_mysql_query 				},
			{ "reset_guild_war_stats",		game_reset_guild_war_stats		},
#endif
#ifdef ENABLE_DICE_SYSTEM
			{ "drop_item_with_ownership_and_dice",	game_drop_item	},
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




