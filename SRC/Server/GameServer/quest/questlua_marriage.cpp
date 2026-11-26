#include "stdafx.h"
#include "../ecs/ResourceAdmission.hpp"
#include "../ecs/systems/PlayerItemActions.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../item/ItemCatalogRuntime.hpp"
#include "../item/ItemStore.hpp"
#include "../item/TypedDrops.hpp"
#include "../item/TypedInventorySync.hpp"
#include "../item/TypedRecovery.hpp"
#include "../network/desc.h"
#include <item/MarriageItems.hpp>
#include <openssl/rand.h>
#include <Core/Logging.hpp>
#include "wedding.h"
#include "../ecs/components/social_components.hpp"
#include "questmanager.h"
#include "utils.h"
#include "config.h"

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
	namespace item = auriga::item;
	namespace player = ecs::PlayerRuntime;

	bool WeddingAttire(entt::entity actor)
	{
		const auto* inventory = g_registry.valid(actor) ? g_registry.try_get<ecs::TypedItems>(actor) : nullptr;
		if (!inventory) return false;
		return std::ranges::any_of(inventory->state.items, [&](const auto& row) {
			return !row.destroyed && row.owner == inventory->state.fence.owner && row.placement &&
				row.placement->kind == item::ItemStorageKind::Equipment && row.key.Category() == item::ItemCategory::Armor &&
				row.key.LocalId() >= 193 && row.key.LocalId() <= 196;
		});
	}

	bool NearMarriageNpc(entt::entity actor, entt::entity npc)
	{
		return player::IsPC(actor) && player::IsValid(npc) &&
			player::GetMapIndex(actor) == player::GetMapIndex(npc) &&
			DISTANCE_APPROX(player::GetX(actor) - player::GetX(npc), player::GetY(actor) - player::GetY(npc)) < 1000;
	}

	struct MarriageActor
	{
		entt::entity entity;
		decltype(ecs::TypedItemLogin::token) login;
	};

	int SettleMarriageItems(lua_State* L, item::MarriageItemAction action)
	{
		using Action = item::MarriageItemAction;
		auto& manager = CQuestManager::instance();
		const auto* state = manager.GetCurrentState();
		const auto refuse = [L] {
			lua_pushboolean(L, false);
			return 1;
		};
		if (!lua_isyieldable(L) || !state || state->co != L || manager.IsInOtherPCBlock()) return refuse();

		const bool engage = action == Action::Engage;
		const bool solo = action == Action::UnilateralDivorce;
		const auto initiator = manager.GetCurrentPCEntity();
		const auto npc = manager.GetCurrentNPCEntity();
		if (!NearMarriageNpc(initiator, npc)) return refuse();
		const auto race = player::GetRaceNum(npc);
		if (engage ? race != 9006 : (race != 11000 && race != 11002 && race != 11004)) return refuse();
		const auto pid = player::GetPlayerID(initiator);
		auto& marriages = marriage::CManager::instance();
		const auto relation = marriages.Get(pid);

		entt::entity partner = entt::null;
		std::uint32_t partnerPid = 0;
		if (!solo)
		{
			const auto vid = luaL_checkinteger(L, 1);
			if (vid <= 0 || vid > UINT32_MAX) return refuse();
			partner = player::FindByVID(static_cast<std::uint32_t>(vid));
			if (!NearMarriageNpc(partner, npc) || partner == initiator) return refuse();
			partnerPid = player::GetPlayerID(partner);
		}
		if (engage)
		{
			const int level = ecs::PointSystem::GetLevel(initiator);
			const int partnerLevel = ecs::PointSystem::GetLevel(partner);
			if (relation != entt::null || marriages.IsEngagedOrMarried(partnerPid) ||
				level < 25 || partnerLevel <= 25 || std::abs(level - partnerLevel) > 15 ||
				!WeddingAttire(initiator) || !WeddingAttire(partner))
				return refuse();
		}
		else
		{
			if (!marriages.IsMarried(pid)) return refuse();
			const auto spouse = marriage::MarriageSystem::GetOther(relation, pid);
			if (!spouse || (!solo && partnerPid != spouse)) return refuse();
			partnerPid = spouse;
		}

		namespace admission = ecs::resource_admission;
		const auto operation = auriga::resource_admission::Operation::ItemUse;
		auto permit = solo ? admission::Enter({initiator}, operation) : admission::Enter({initiator, partner}, operation);
		const auto catalog = item::runtime::CurrentCatalog();
		if (!permit || !permit->Current() || !catalog) return refuse();

		std::vector<MarriageActor> actors;
		std::vector<item::MarriageItemParticipant> participants;
		std::vector<item::runtime::OwnerRead> reads;
		for (const auto actor : {initiator, partner})
		{
			if (actor == entt::null) continue;
			if (!player::IsValid(actor) || !PlayerItemActions::CanHandleItems(actor) || g_registry.all_of<ecs::TypedItemBusy>(actor))
				return refuse();
			const auto* login = g_registry.try_get<ecs::TypedItemLogin>(actor);
			const auto* typed = g_registry.try_get<ecs::TypedItems>(actor);
			const auto* desc = player::GetDesc(actor);
			if (!login || !typed || !desc || !desc->IsPhase(PHASE_GAME)) return refuse();
			const auto& current = typed->state;
			actors.push_back({actor, login->token});
			participants.push_back({current.fence, current.storages, current.items, current.wallets});
			reads.push_back({current.fence.owner, current.fence.sessionGeneration});
		}

		std::array<std::uint8_t, 16> request{};
		if (RAND_bytes(request.data(), static_cast<int>(request.size())) != 1) return refuse();
		const auto ids = engage ? item::runtime::PeekInstanceIds(2) : std::vector<item::ItemInstanceId>{};
		auto write = item::MakeMarriageItemCommit(*catalog, request, {action, pid, partnerPid}, participants, ids, TypedItemEffectNow());
		if (!write) return refuse();
		item::runtime::SpendInstanceIds(static_cast<std::size_t>(std::ranges::count_if(
			write->items, [](const auto& change) { return !change.before; })));

		for (const auto& actor : actors) g_registry.emplace<ecs::TypedItemBusy>(actor.entity);
		auto held = std::make_shared<auriga::resource_admission::Permit>(std::move(*permit));
		const bool queued = item::runtime::RequestCommitMany(std::move(reads), std::move(*write),
			[actors, pid, coroutine = L, held](item::runtime::MultiCommitOutcome result) mutable {
				bool resume = false;
				for (std::size_t i = 0; i < actors.size(); ++i)
				{
					const auto [actor, token] = actors[i];
					const auto* login = g_registry.valid(actor) ? g_registry.try_get<ecs::TypedItemLogin>(actor) : nullptr;
					if (!login || login->token != token) continue;
					if (i < result.states.size() && result.states[i])
						g_registry.emplace_or_replace<ecs::TypedItems>(actor, std::move(*result.states[i]));
					g_registry.remove<ecs::TypedItemBusy>(actor);
					if (i == 0) resume = true;
				}
				held.reset();
				if (resume) CQuestManager::instance().ResumeTypedItems(pid, coroutine, result.write.has_value());
				for (const auto& [actor, token] : actors)
				{
					const auto* login = g_registry.valid(actor) ? g_registry.try_get<ecs::TypedItemLogin>(actor) : nullptr;
					if (login && login->token == token) { SendTypedInventory(actor); PumpTypedItemGrants(actor); }
				}
			});
		if (!queued)
		{
			for (const auto& actor : actors)
				if (g_registry.valid(actor.entity)) g_registry.remove<ecs::TypedItemBusy>(actor.entity);
			return refuse();
		}
		lua_settop(L, 0);
		lua_pushliteral(L, "typed_items");
		return lua_yieldk(L, 1, 0, [](lua_State* thread, int, lua_KContext) { return lua_gettop(thread); });
	}
	}

	ALUA(marriage_typed_attire)
	{
		lua_pushboolean(L, WeddingAttire(CQuestManager::instance().GetCurrentPCEntity()));
		return 1;
	}

	ALUA(marriage_typed_engage) { return SettleMarriageItems(L, item::MarriageItemAction::Engage); }
	ALUA(marriage_typed_mutual_divorce) { return SettleMarriageItems(L, item::MarriageItemAction::MutualDivorce); }
	ALUA(marriage_typed_unilateral_divorce) { return SettleMarriageItems(L, item::MarriageItemAction::UnilateralDivorce); }

	ALUA(marriage_engage_to)
	{
		// migrated from marriage system
		uint32_t vid = (uint32_t) lua_tonumber(L, 1);
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const entt::entity other = ecs::PlayerRuntime::FindByVID(vid);
		if (ecs::PlayerRuntime::IsPC(chEntity) && ecs::PlayerRuntime::IsPC(other))
		{
			marriage::CManager::instance().RequestAdd(
				ecs::PlayerRuntime::GetPlayerID(chEntity), ecs::PlayerRuntime::GetPlayerID(other),
				ecs::PlayerRuntime::GetName(chEntity).data(), ecs::PlayerRuntime::GetName(other).data());
		}
		return 0;
	}

	ALUA(marriage_remove)
	{
		// migrated from marriage system
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);
		if (couple == entt::null)
		{
			sys_err("pid[{}:{}] is not exist couple", playerID, ecs::PlayerRuntime::GetName(chEntity).data());
			return 0;
		}
		marriage::CManager::instance().RequestRemove(playerID, marriage::MarriageSystem::GetOther(couple, playerID));
		return 0;
	}

	ALUA(marriage_set_to_marriage)
	{
		// migrated from marriage system
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);
		if (couple == entt::null)
		{
			sys_err("pid[{}:{}] is not exist couple", playerID, ecs::PlayerRuntime::GetName(chEntity).data());
			return 0;
		}
		marriage::MarriageSystem::SetMarried(couple);
		return 0;
	}

	ALUA(marriage_find_married_vid)
	{
		// migrated from marriage system
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);
		uint32_t vid = 0;
		if (couple != entt::null)
		{
			const entt::entity other = ecs::PlayerRuntime::FindByPlayerID(marriage::MarriageSystem::GetOther(couple, playerID));
			vid = ecs::PlayerRuntime::GetPacketVID(other);
		}

		lua_pushnumber(L, vid);

		return 1;
	}

	struct FBuildLuaWeddingMapList
	{
		lua_State * L;
		int m_count;
		FBuildLuaWeddingMapList(lua_State * L) : L(L), m_count(1)
		{
			lua_newtable(L);
		}

		void operator() (entt::entity couple)
		{
			const auto* state = marriage::MarriageSystem::State(couple);
			if (!state || !state->weddingMapIndex)
				return;

			lua_newtable(L);
			lua_pushnumber(L, state->pid1);
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, state->pid2);
			lua_rawseti(L, -2, 2);
			lua_pushstring(L, state->name1.c_str());
			lua_rawseti(L, -2, 3);
			lua_pushstring(L, state->name2.c_str());
			lua_rawseti(L, -2, 4);
			lua_rawseti(L, -2, m_count++);
		}
	};

	ALUA(marriage_get_wedding_list)
	{
		// migrated from marriage system
		marriage::CManager::instance().for_each_wedding(FBuildLuaWeddingMapList(L));
		return 1;
	}

	ALUA(marriage_join_wedding)
	{
		// migrated from marriage system
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
		{
			sys_err("invalid player id for wedding map");
			return 0;
		}

		uint32_t pid1 = (uint32_t) lua_tonumber(L, 1);
		uint32_t pid2 = (uint32_t) lua_tonumber(L, 2);

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(pid1);
		if (couple == entt::null)
		{
			sys_err("pid[{}:{}] is not exist couple", (playerID), ecs::PlayerRuntime::GetName(chEntity).data());
			return 0;
		}
		if (marriage::MarriageSystem::GetOther(couple, pid1) != pid2)
		{
			sys_err("not married {} {}", pid1, pid2);
			return 0;
		}
		//PREVENT_HACK
		if (ecs::PlayerRuntime::IsHack(chEntity))
			return 0;
		//END_PREVENT_HACK

		marriage::MarriageSystem::WarpToWeddingMap(couple, (playerID));
		return 0;
	}

	ALUA(marriage_warp_to_my_marriage_map)
	{
		// migrated from marriage system
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);
		if (couple == entt::null)
		{
			sys_err("pid[{}:{}] is not exist couple", (playerID), ecs::PlayerRuntime::GetName(chEntity).data());
			return 0;
		}


		//PREVENT_HACK
		if (ecs::PlayerRuntime::IsHack(chEntity))
			return 0;
		//END_PREVENT_HACK

		marriage::MarriageSystem::WarpToWeddingMap(couple, (playerID));
		return 0;
	}

	ALUA(marriage_end_wedding)
	{
		// migrated from marriage system
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);
		if (couple == entt::null)
		{
			sys_err("pid[{}:{}] is not exist couple", (playerID), ecs::PlayerRuntime::GetName(chEntity).data());
			return 0;
		}
		const auto* state = marriage::MarriageSystem::State(couple);
		if (state && state->weddingMapIndex)
		{
			marriage::MarriageSystem::RequestEndWedding(couple);
		}
		return 0;
	}

	ALUA(marriage_wedding_dark)
	{
		// migrated from marriage system
		if (!lua_isboolean(L, 1))
		{
			sys_err("invalid argument 1 : must be boolean");
			return 0;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);

		if (couple == entt::null)
		{
			sys_err("pid[{}:{}] is not exist couple", (playerID), ecs::PlayerRuntime::GetName(chEntity).data());
			return 0;
		}
		const auto* state = marriage::MarriageSystem::State(couple);
		if (state && state->weddingMapIndex)
		{
			const entt::entity pWedding = marriage::WeddingManager::instance().Find(*state->weddingMapIndex);
			marriage::WeddingSystem::SetDark(pWedding, lua_toboolean(L, 1) != 0);
		}

		return 0;
	}

	ALUA(marriage_wedding_client_command)
	{
		// migrated from marriage system
		if (!lua_isstring(L, 1))
		{
			sys_err("invalid argument 1 : must be string");
			return 0;
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);
		if (couple == entt::null)
		{
			sys_err("pid[{}:{}] is not exist couple", (playerID), ecs::PlayerRuntime::GetName(chEntity).data());
			return 0;
		}
		const auto* state = marriage::MarriageSystem::State(couple);
		if (state && state->weddingMapIndex)
		{
			const entt::entity pWedding = marriage::WeddingManager::instance().Find(*state->weddingMapIndex);
			marriage::WeddingSystem::ShoutInMap(pWedding, CHAT_TYPE_COMMAND, lua_tostring(L, 1));
		}
		return 0;

	}

	ALUA(marriage_wedding_is_playing_music)
	{
		// migrated from marriage system
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);
		if (couple == entt::null)
		{
			sys_err("pid[{}:{}] is not exist couple", (playerID), ecs::PlayerRuntime::GetName(chEntity).data());
			return 0;
		}
		const auto* state = marriage::MarriageSystem::State(couple);
		if (state && state->weddingMapIndex)
		{
			const entt::entity pWedding = marriage::WeddingManager::instance().Find(*state->weddingMapIndex);
			if (pWedding != entt::null)
				lua_pushboolean(L, marriage::WeddingSystem::IsPlayingMusic(pWedding));
			else
				lua_pushboolean(L, false);
		}

		lua_pushboolean(L, false);
		return 1;
	}
	ALUA(marriage_wedding_music)
	{
		// migrated from marriage system
		if (!lua_isboolean(L, 1))
		{
			sys_err("invalid argument 1 : must be boolean");
			return 0;
		}
		if (!lua_isstring(L, 2))
		{
			sys_err("invalid argument 2 : must be string");
			return 0;
		}

		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();

		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);
		if (couple == entt::null)
		{
			sys_err("pid[{}:{}] is not exist couple", (playerID), ecs::PlayerRuntime::GetName(chEntity).data());
			return 0;
		}
		const auto* state = marriage::MarriageSystem::State(couple);
		if (state && state->weddingMapIndex)
		{
			const entt::entity pWedding = marriage::WeddingManager::instance().Find(*state->weddingMapIndex);
			marriage::WeddingSystem::SetMusic(
					pWedding,
					lua_toboolean(L, 1) != 0,
					lua_tostring(L, 2)
					);
		}
		return 0;
	}
	ALUA(marriage_wedding_snow)
	{
		// migrated from marriage system
		if (!lua_isboolean(L, 1))
		{
			sys_err("invalid argument 1 : must be boolean");
			return 0;
		}
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);
		if (couple == entt::null)
		{
			sys_err("pid[{}:{}] is not exist couple", (playerID), ecs::PlayerRuntime::GetName(chEntity).data());
			return 0;
		}
		const auto* state = marriage::MarriageSystem::State(couple);
		if (state && state->weddingMapIndex)
		{
			const entt::entity pWedding = marriage::WeddingManager::instance().Find(*state->weddingMapIndex);
			marriage::WeddingSystem::SetSnow(pWedding, lua_toboolean(L, 1) != 0);
		}
		return 0;
	}

	ALUA(marriage_in_my_wedding)
	{
		// migrated from marriage system
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);
		const auto* state = marriage::MarriageSystem::State(couple);
		if (state && state->weddingMapIndex)
		{
			lua_pushboolean(L, (uint32_t)ecs::PlayerRuntime::GetMapIndex(chEntity) == *state->weddingMapIndex);
		}
		else
		{
			lua_pushboolean(L, 0);
		}
		return 1;
	}

	ALUA(marriage_get_married_time)
	{
		// migrated from marriage system
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		const uint32_t playerID = ecs::PlayerRuntime::GetPlayerID(chEntity);
		const entt::entity couple = marriage::CManager::instance().Get(playerID);

		if (couple == entt::null)
		{
			sys_err("trying to get time for not married character");
			lua_pushnumber(L, 0);
			return 1;
		}

		lua_pushnumber(L, get_global_time() - marriage::MarriageSystem::State(couple)->marryTime);
		return 1;
	}

	void RegisterMarriageFunctionTable()
	{
		luaL_reg marriage_functions[] =
		{
			{ "engage_to",		marriage_engage_to	    },
			{ "typed_attire", marriage_typed_attire },
			{ "typed_engage", marriage_typed_engage },
			{ "typed_mutual_divorce", marriage_typed_mutual_divorce },
			{ "typed_unilateral_divorce", marriage_typed_unilateral_divorce },
			{ "remove",			marriage_remove		    },
			{ "find_married_vid",	marriage_find_married_vid   },
			{ "get_wedding_list",	marriage_get_wedding_list   },
			{ "join_wedding",		marriage_join_wedding	    },
			{ "set_to_marriage",	marriage_set_to_marriage    },
			{ "end_wedding",		marriage_end_wedding	    },
			{ "wedding_dark",		marriage_wedding_dark	    },
			{ "wedding_snow",		marriage_wedding_snow	    },
			{ "wedding_music",		marriage_wedding_music	    },
			{ "wedding_is_playing_music", marriage_wedding_is_playing_music },
			{ "wedding_client_command",	marriage_wedding_client_command    },
			{ "in_my_wedding",		marriage_in_my_wedding	    },
			{ "warp_to_my_marriage_map",marriage_warp_to_my_marriage_map},
			{ "get_married_time",	marriage_get_married_time   },
			{nullptr, nullptr}
		};
		CQuestManager::instance().AddLuaFunctionTable("marriage", marriage_functions);
	}
}
