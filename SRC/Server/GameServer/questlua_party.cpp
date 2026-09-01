#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include <sstream>

#include "desc.h"
#include "party.h"
#include "char_interface.hpp"
#include "questlua.h"
#include "questmanager.h"
#include "packet.h"
#include "char_manager.h"
#include "db.h"
#include "ecs/quest_helpers.hpp"

#undef sys_err
#ifndef _WIN32
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestErrorFmt(__FUNCTION__, __LINE__, FMT_STRING(fmt), __VA_ARGS__)
#endif

namespace quest
{
	using namespace std;

	//
	// "party" Lua functions
	//
	ALUA(party_clear_ready)
	{
		if (LPPARTY party = ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L)))
		{
			FPartyClearReady f;
			party->ForEachNearMember(f);
		}
		return 0;
	}

	ALUA(party_get_max_level)
	{
		LPPARTY party = ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L));
		lua_pushnumber(L, party ? party->GetMemberMaxLevel() : 1);
		return 1;
	}

#ifdef ENABLE_NEWSTUFF
	ALUA(party_get_min_level)
	{
		LPPARTY party = ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L));
		lua_pushnumber(L, party ? party->GetMemberMinLevel() : 1);
		return 1;
	}

	ALUA(party_leave_party)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		LPPARTY pParty = ecs::SocialSystem::GetParty(character);
		if (pParty)
		{
			if (pParty->GetMemberCount() == 2)
				CPartyManager::instance().DeleteParty(pParty);
			else
				pParty->Quit(ecs::PlayerRuntime::GetPlayerID(character));
		}

		lua_pushboolean(L, ecs::SocialSystem::GetParty(character) == nullptr);
		return 1;
	}

	ALUA(party_delete_party)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		LPPARTY party = ecs::SocialSystem::GetParty(character);
		if (party && party->GetLeaderPID() == ecs::PlayerRuntime::GetPlayerID(character))
			CPartyManager::instance().DeleteParty(party);

		lua_pushboolean(L, ecs::SocialSystem::GetParty(character) == nullptr);
		return 1;
	}
#endif

    struct FRunCinematicSender
    {
        std::string data;
        struct packet_script pack;

        FRunCinematicSender(const char* str)
        {
            data = "[RUN_CINEMA value;";
            data += str;
            data += "]";

            pack.header = HEADER_GC_SCRIPT;
            pack.skin = CQuestManager::QUEST_SKIN_CINEMATIC;
            //pack.skin = CQuestManager::QUEST_SKIN_NOWINDOW;
            pack.src_size = data.size();
            pack.size = pack.src_size + sizeof(struct packet_script);
        }

		void operator()(entt::entity character)
		{
			LOG_INFO("CINEMASEND_TRY {}", ecs::PlayerRuntime::GetName(character).data());

			if (LPDESC desc = ecs::PlayerRuntime::GetDesc(character))
			{
				LOG_INFO("CINEMASEND {}", ecs::PlayerRuntime::GetName(character).data());
				desc->BufferedPacket(&pack, sizeof(struct packet_script));
				desc->Packet(data.c_str(),data.size());
            }
        }
    };

	ALUA(party_run_cinematic)
	{
		// migrated from CHARACTER::GetParty()->ForEachNearMember
		// DUAL-PATH: legacy only during migration window
		if (!lua_isstring(L, 1))
			return 0;

		LOG_INFO("RUN_CINEMA {}", lua_tostring(L, 1));
		if (LPPARTY party = ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L)))
		{
			FRunCinematicSender f(lua_tostring(L, 1));
			party->Update();
			ecs::SocialSystem::ForEachNearPartyMember(
				CQuestManager::instance().GetPCEntity(L), f);
		}

		return 0;
	}

	struct FCinematicSender
	{
		const char* str;
		struct ::packet_script packet_script;
		int len;

		FCinematicSender(const char* str)
			: str(str)
		{
			len = strlen(str);

			packet_script.header = HEADER_GC_SCRIPT;
			packet_script.skin = CQuestManager::QUEST_SKIN_CINEMATIC;
			packet_script.src_size = len;
			packet_script.size = packet_script.src_size + sizeof(struct packet_script);
		}

		void operator()(entt::entity character)
		{
			LOG_INFO("CINEMASEND_TRY {}", ecs::PlayerRuntime::GetName(character).data());

			if (LPDESC desc = ecs::PlayerRuntime::GetDesc(character))
			{
				LOG_INFO("CINEMASEND {}", ecs::PlayerRuntime::GetName(character).data());
				desc->BufferedPacket(&packet_script, sizeof(struct packet_script));
				desc->Packet(str,len);
			}
		}
	};

	ALUA(party_show_cinematic)
	{
		// migrated from CHARACTER::GetParty()->ForEachNearMember
		// DUAL-PATH: legacy only during migration window
		if (!lua_isstring(L, 1))
			return 0;

		LOG_INFO("CINEMA {}", lua_tostring(L, 1));
		if (LPPARTY party = ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L)))
		{
			FCinematicSender f(lua_tostring(L, 1));
			party->Update();
			ecs::SocialSystem::ForEachNearPartyMember(
				CQuestManager::instance().GetPCEntity(L), f);
		}
		return 0;
	}

	ALUA(party_get_near_count)
	{
		LPPARTY party = ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L));
		lua_pushnumber(L, party ? party->GetNearMemberCount() : 0);
		return 1;
	}

	ALUA(party_syschat)
	{
		// migrated from CHARACTER::GetParty()->ForEachOnlineMember
		// DUAL-PATH: legacy only during migration window
		LPPARTY pParty = ecs::SocialSystem::GetParty(CQuestManager::Instance().GetPCEntity(L));

		if (pParty)
		{
			ostringstream s;
			combine_lua_string(L, s);

			FPartyChat f(CHAT_TYPE_INFO, s.str().c_str());

			pParty->ForEachOnlineMember(f);
		}

		return 0;
	}

	ALUA(party_is_leader)
	{
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		LPPARTY party = ecs::SocialSystem::GetParty(character);
		lua_pushboolean(L, party && party->GetLeaderPID() == ecs::PlayerRuntime::GetPlayerID(character));
		return 1;
	}

	ALUA(party_is_party)
	{
		lua_pushboolean(L,
			ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L)) != nullptr);
		return 1;
	}

	ALUA(party_get_leader_pid)
	{
		LPPARTY party = ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L));
		lua_pushnumber(L, party ? party->GetLeaderPID() : -1);
		return 1;
	}


	ALUA(party_chat)
	{
		// migrated from CHARACTER::GetParty()->ForEachOnlineMember
		// DUAL-PATH: legacy only during migration window
		LPPARTY pParty = ecs::SocialSystem::GetParty(CQuestManager::Instance().GetPCEntity(L));

		if (pParty)
		{
			ostringstream s;
			combine_lua_string(L, s);

			FPartyChat f(CHAT_TYPE_TALKING, s.str().c_str());

			pParty->ForEachOnlineMember(f);
		}

		return 0;
	}


	ALUA(party_is_map_member_flag_lt)
	{
		// migrated from CHARACTER::GetParty()->ForEachOnMapMemberBool
		// DUAL-PATH: legacy only during migration window

		if (!lua_isstring(L, 1) || !lua_isnumber(L, 2))
		{
			lua_pushnumber(L, 0);
			return 1;
		}

		CQuestManager& q = CQuestManager::Instance();
		const entt::entity character = q.GetPCEntity(L);
		LPPARTY pParty = ecs::SocialSystem::GetParty(character);
		PC* pPC = q.GetCurrentPC();

		const char* sz = lua_tostring(L,1);

		if (pParty)
		{
			FPartyCheckFlagLt f;
			f.flagname = pPC->GetCurrentQuestName() + "."+sz;
			f.value = (int) rint(lua_tonumber(L, 2));

			bool returnBool = pParty->ForEachOnMapMemberBool(
				f, ecs::PlayerRuntime::GetMapIndex(character));
			lua_pushboolean(L, returnBool);
		}

		return 1;
	}

	ALUA(party_set_flag)
	{
		LPPARTY party = ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L));
		if (party && lua_isstring(L, 1) && lua_isnumber(L, 2))
			party->SetFlag(lua_tostring(L, 1), static_cast<int>(lua_tonumber(L, 2)));

		return 0;
	}

	ALUA(party_get_flag)
	{
		LPPARTY party = ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L));
		if (!party || !lua_isstring(L, 1))
			lua_pushnumber(L, 0);
		else
			lua_pushnumber(L, party->GetFlag(lua_tostring(L, 1)));

		return 1;
	}

	ALUA(party_set_quest_flag)
	{
		// migrated from CHARACTER::SetQuestFlag
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();

		FSetQuestFlag f;

		f.flagname = q.GetCurrentPC()->GetCurrentQuestName() + "." + lua_tostring(L, 1);
		f.value = (int) rint(lua_tonumber(L, 2));

		const entt::entity character = q.GetPCEntity(L);
		if (ecs::SocialSystem::GetParty(character))
			ecs::SocialSystem::ForEachOnlinePartyMember(character,
				[&f](entt::entity member) {
					ecs::QuestSystem::SetFlag(member, f.flagname, f.value);
				});
		else
			ecs::QuestSystem::SetFlag(character, f.flagname, f.value);

		return 0;
	}

	ALUA(party_is_in_dungeon)
	{
		LPPARTY party = ecs::SocialSystem::GetParty(CQuestManager::instance().GetPCEntity(L));
		lua_pushboolean(L, party && party->GetDungeon());
		return 1;
	}

	struct FGiveBuff
	{
		uint32_t dwType;
		uint8_t bApplyOn;
		int32_t lApplyValue;
		uint32_t dwFlag;
		int32_t lDuration;
		int32_t lSPCost;
		bool bOverride;
		bool IsCube;

		FGiveBuff (uint32_t _dwType, uint8_t _bApplyOn, int32_t _lApplyValue, uint32_t _dwFlag, int32_t _lDuration,
			int32_t _lSPCost, bool _bOverride, bool _IsCube = false)
			: dwType (_dwType), bApplyOn (_bApplyOn), lApplyValue (_lApplyValue), dwFlag(_dwFlag), lDuration(_lDuration),
				lSPCost(_lSPCost), bOverride(_bOverride), IsCube(_IsCube)
		{}
		void operator () (entt::entity character)
		{
			AffectSystem::AddAffect(character,
				dwType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost, bOverride, IsCube);
		}
	};

	// ��Ƽ ������ ���� �ִ� �Լ�.
	// ���� �ʿ� �ִ� ��Ƽ���� ������ �޴´�.
	ALUA(party_give_buff)
	{
		// migrated from CHARACTER::AddAffect
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();
		const entt::entity character = q.GetPCEntity(L);
		if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4) ||
			!lua_isnumber(L, 5) || !lua_isnumber(L, 6) || !lua_isboolean(L, 7) || !lua_isboolean(L, 8))
		{
			lua_pushboolean (L, false);
			return 1;
		}
		uint32_t dwType = lua_tonumber(L, 1);
		uint8_t bApplyOn = lua_tonumber(L, 2);
		int32_t lApplyValue = lua_tonumber(L, 3);
		uint32_t dwFlag = lua_tonumber(L, 4);
		int32_t lDuration = lua_tonumber(L, 5);
		int32_t lSPCost = lua_tonumber(L, 6);
		bool bOverride = lua_toboolean(L, 7);
		bool IsCube = lua_toboolean(L, 8);

		FGiveBuff f (dwType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost, bOverride, IsCube);
		if (ecs::SocialSystem::GetParty(character))
			ecs::SocialSystem::ForEachPartyMemberOnMap(
				character, ecs::PlayerRuntime::GetMapIndex(character), f);
		else
			AffectSystem::AddAffect(character, dwType, bApplyOn, lApplyValue,
				dwFlag, lDuration, lSPCost, bOverride, IsCube);

		lua_pushboolean(L, true);
		return 1;
	}

	struct FPartyPIDCollector
	{
		std::vector <uint32_t> vecPIDs;
		FPartyPIDCollector()
		{
		}
		void operator () (entt::entity character)
		{
			vecPIDs.push_back(ecs::PlayerRuntime::GetPlayerID(character));
		}
	};

	ALUA(party_get_member_pids)
	{
		// migrated from CHARACTER::GetParty()->ForEachOnMapMember
		// DUAL-PATH: legacy fallback during migration window
		CQuestManager & q = CQuestManager::instance();
		const entt::entity character = q.GetPCEntity(L);
		LPPARTY pParty = ecs::SocialSystem::GetParty(character);
		if (nullptr == pParty)
		{
			return 0;
		}
		FPartyPIDCollector f;
		ecs::SocialSystem::ForEachPartyMemberOnMap(
			character, ecs::PlayerRuntime::GetMapIndex(character), f);

		for (std::vector <uint32_t>::iterator it = f.vecPIDs.begin(); it != f.vecPIDs.end(); it++)
		{
			lua_pushnumber(L, *it);
		}
		return f.vecPIDs.size();
	}

	ALUA(party_get_leader_name)
	{
		// migrated from CHARACTER::GetParty()->GetLeaderPID
		// DUAL-PATH: legacy fallback during migration window
		const entt::entity character = CQuestManager::instance().GetPCEntity(L);
		std::string name;
		LPPARTY party = ecs::SocialSystem::GetParty(character);
		if (party)
		{
			const entt::entity leader = ecs::PlayerRuntime::FindByPlayerID(party->GetLeaderPID());
			name = leader != entt::null
				? ecs::PlayerRuntime::GetName(leader).data()
				: ecs::PlayerRuntime::GetName(character).data();
		}
		else
			name = ecs::PlayerRuntime::GetName(character).data();

		lua_pushstring(L, name.c_str());
		return 1;
	}

	ALUA(party_give_gold)
	{
		CQuestManager& q = CQuestManager::instance();
		const entt::entity character = q.GetPCEntity(L);
		const int64_t gold = static_cast<int64_t>(lua_tonumber(L, -1));
		LPPARTY party = ecs::SocialSystem::GetParty(character);
		if (party)
		{
			FPartyPIDCollector collector;
			ecs::SocialSystem::ForEachPartyMemberOnMap(
				character, ecs::PlayerRuntime::GetMapIndex(character), collector);
			for (const uint32_t playerID : collector.vecPIDs)
			{
				const entt::entity member = ecs::PlayerRuntime::FindByPlayerID(playerID);
				if (!ecs::PlayerRuntime::IsPC(member))
					continue;
				if (gold + ecs::PointSystem::GetGold(member) < 0)
					sys_err("QUEST wrong ChangeGold {} (now {})", gold, ecs::PointSystem::GetGold(member));
				else
				{
					DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST, playerID, gold);
					ecs::PointSystem::Change(member, POINT_GOLD, gold, true);
				}
			}
			if (!q.GetPC(ecs::PlayerRuntime::GetPlayerID(character)))
				sys_err("cannot return to main.");
		}
		else if (gold + ecs::PointSystem::GetGold(character) < 0)
			sys_err("QUEST wrong ChangeGold {} (now {})", gold, ecs::PointSystem::GetGold(character));
		else
		{
			DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST,
				ecs::PlayerRuntime::GetPlayerID(character), gold);
			ecs::PointSystem::Change(character, POINT_GOLD, gold, true);
		}
		return 0;
	}

	ALUA(party_give_blacksmith)
	{
		CQuestManager& q = CQuestManager::instance();
		const entt::entity character = q.GetPCEntity(L);
		LPPARTY party = ecs::SocialSystem::GetParty(character);
		if (party)
		{
			FPartyPIDCollector collector;
			ecs::SocialSystem::ForEachPartyMemberOnMap(
				character, ecs::PlayerRuntime::GetMapIndex(character), collector);
			for (const uint32_t playerID : collector.vecPIDs)
			{
				const entt::entity member = ecs::PlayerRuntime::FindByPlayerID(playerID);
				if (ecs::PlayerRuntime::IsPC(member))
					ecs::QuestSystem::SetFlag(member, "deviltower_zone.can_refine", 1);
			}
			if (!q.GetPC(ecs::PlayerRuntime::GetPlayerID(character)))
				sys_err("cannot return to main.");
		}
		else
			ecs::QuestSystem::SetFlag(character, "deviltower_zone.can_refine", 1);
		return 0;
	}

	void RegisterPartyFunctionTable()
	{
		luaL_reg party_functions[] =
		{
			{ "is_leader",		party_is_leader		},
			{ "is_party",		party_is_party		},
			{ "get_leader_pid",	party_get_leader_pid},
			{ "setf",			party_set_flag		},
			{ "getf",			party_get_flag		},
			{ "setqf",			party_set_quest_flag},
			{ "chat",			party_chat			},
			{ "syschat",		party_syschat		},
			{ "get_near_count",	party_get_near_count},
			{ "show_cinematic",	party_show_cinematic},
			{ "run_cinematic",	party_run_cinematic	},
			{ "get_max_level",	party_get_max_level	},
#ifdef ENABLE_NEWSTUFF
			{ "get_min_level",	party_get_min_level	},	// [return lua number]
			{ "leave_party",	party_leave_party	},	// [return lua boolean=successfulness]
			{ "delete_party",	party_delete_party	},	// [return lua boolean=successfulness]
#endif
			{ "clear_ready",	party_clear_ready	},
			{ "is_in_dungeon",	party_is_in_dungeon	},
			{ "give_buff",		party_give_buff		},
			{ "is_map_member_flag_lt",	party_is_map_member_flag_lt	},
			{ "get_member_pids",		party_get_member_pids	}, // ��Ƽ������ pid�� return
			{"get_leader_name", party_get_leader_name},
			{"give_gold", party_give_gold},
			{"give_blacksmith", party_give_blacksmith},
			{nullptr, nullptr}
		};

		CQuestManager::instance().AddLuaFunctionTable("party", party_functions);
	}
}






