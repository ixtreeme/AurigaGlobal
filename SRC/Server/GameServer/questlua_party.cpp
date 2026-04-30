#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"
#include <sstream>

#include "desc.h"
#include "party.h"
#include "char_interface.hpp"
#include "questlua.h"
#include "questmanager.h"
#include "packet.h"
#include "char_manager.h"
#include "ecs/CharacterAccessors.hpp"
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

	namespace
	{
		LPCHARACTER GetPartyQuestCharacter()
		{
			return CQuestManager::instance().GetCurrentCharacterPtr();
		}

		LPPARTY GetPartyFromECSOrLegacy(lua_State* L)
		{
			entt::entity e = CQuestManager::instance().GetPCEntity(L);
			if (auto* pm = ECS_TryGet<ecs::PartyMembership>(e))
				return pm->party;

			LPCHARACTER ch = GetPartyQuestCharacter();
			return ch ? ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)) : nullptr;
		}
	}
	//
	// "party" Lua functions
	//
	ALUA(party_clear_ready)
	{
		// migrated from CHARACTER::GetParty()->ForEachNearMember
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
		{
			FPartyClearReady f;
			ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->ForEachNearMember(f);
		}
		return 0;
	}

	ALUA(party_get_max_level)
	{
		// migrated from CHARACTER::GetParty()->GetMemberMaxLevel()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* pm = ECS_TryGet<ecs::PartyMembership>(e);
		if (!pm || !pm->party)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			lua_pushnumber(L, (ch && ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))) ? ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetMemberMaxLevel() : 1);
			return 1;
		}

		lua_pushnumber(L, pm->party->GetMemberMaxLevel());
		return 1;
	}

#ifdef ENABLE_NEWSTUFF
	ALUA(party_get_min_level)
	{
		// migrated from CHARACTER::GetParty()->GetMemberMinLevel()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* pm = ECS_TryGet<ecs::PartyMembership>(e);
		if (!pm || !pm->party)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			lua_pushnumber(L, (ch && ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))) ? ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetMemberMinLevel() : 1);
			return 1;
		}

		lua_pushnumber(L, pm->party->GetMemberMinLevel());
		return 1;
	}

	ALUA(party_leave_party)
	{
		// migrated from CHARACTER::GetParty()->Quit
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		// if (!ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))&&!CPartyManager::instance().IsEnablePCParty()&&ch->GetDungeon())
			// return 0;

		LPPARTY pParty = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
		if (pParty)
		{
			if (pParty->GetMemberCount() == 2)
				CPartyManager::instance().DeleteParty(pParty);
			else
				pParty->Quit(((ch)->GetPlayerID()));
		}

		lua_pushboolean(L, ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))== nullptr);
		return 1;
	}

	ALUA(party_delete_party)
	{
		// migrated from CHARACTER::GetParty()->Delete
		// DUAL-PATH: legacy only during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		// if (!ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))&&!CPartyManager::instance().IsEnablePCParty()&&ch->GetDungeon()&&!ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetLeaderPID() == ((ch)->GetPlayerID()))
			// return 0;

		if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)) && ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetLeaderPID() == ((ch)->GetPlayerID()))
			CPartyManager::instance().DeleteParty(ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)));

		lua_pushboolean(L, ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))== nullptr);
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

        void operator()(LPCHARACTER ch)
        {
            LOG_INFO("CINEMASEND_TRY {}", ((ch)->GetName()));

            if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
            {
                LOG_INFO("CINEMASEND {}", ((ch)->GetName()));
                ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->BufferedPacket(&pack, sizeof(struct packet_script));
                ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(data.c_str(),data.size());
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
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
		{
			FRunCinematicSender f(lua_tostring(L, 1));

			ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->Update();
			ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->ForEachNearMember(f);
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

		void operator()(LPCHARACTER ch)
		{
			LOG_INFO("CINEMASEND_TRY {}", ((ch)->GetName()));

			if (ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
			{
				LOG_INFO("CINEMASEND {}", ((ch)->GetName()));
				ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->BufferedPacket(&packet_script, sizeof(struct packet_script));
				ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch))->Packet(str,len);
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
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
		{
			FCinematicSender f(lua_tostring(L, 1));

			ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->Update();
			ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->ForEachNearMember(f);
		}
		return 0;
	}

	ALUA(party_get_near_count)
	{
		// migrated from CHARACTER::GetParty()->GetNearMemberCount()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* pm = ECS_TryGet<ecs::PartyMembership>(e);
		if (!pm || !pm->party)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			lua_pushnumber(L, (ch && ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))) ? ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetNearMemberCount() : 0);
			return 1;
		}

		lua_pushnumber(L, pm->party->GetNearMemberCount());
		return 1;
	}

	ALUA(party_syschat)
	{
		// migrated from CHARACTER::GetParty()->ForEachOnlineMember
		// DUAL-PATH: legacy only during migration window
		LPCHARACTER ch = CQuestManager::Instance().GetCurrentCharacterPtr();
		LPPARTY pParty = ch ? ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)) : nullptr;

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
		// migrated from CHARACTER::GetParty()->GetLeaderPID()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* pm = ECS_TryGet<ecs::PartyMembership>(e);
		if (!pm || !pm->party)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			if (!ch || !ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
			{
				lua_pushboolean(L, 0);
				return 1;
			}

			lua_pushboolean(L, ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetLeaderPID() == ((ch)->GetPlayerID()) ? 1 : 0);
			return 1;
		}

		auto* pid = ECS_TryGet<ecs::PlayerID>(e);
		lua_pushboolean(L, (pid && pm->party->GetLeaderPID() == pid->pid) ? 1 : 0);
		return 1;
	}

	ALUA(party_is_party)
	{
		// migrated from CHARACTER::GetParty()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* pm = ECS_TryGet<ecs::PartyMembership>(e);
		if (!pm)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			lua_pushboolean(L, (ch && ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))) ? 1 : 0);
			return 1;
		}

		lua_pushboolean(L, pm->party != nullptr ? 1 : 0);
		return 1;
	}

	ALUA(party_get_leader_pid)
	{
		// migrated from CHARACTER::GetParty()->GetLeaderPID()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* pm = ECS_TryGet<ecs::PartyMembership>(e);
		if (!pm || !pm->party)
		{
			const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			lua_pushnumber(L, (ch && ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))) ? ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetLeaderPID() : -1);
			return 1;
		}

		lua_pushnumber(L, pm->party->GetLeaderPID());
		return 1;
	}


	ALUA(party_chat)
	{
		// migrated from CHARACTER::GetParty()->ForEachOnlineMember
		// DUAL-PATH: legacy only during migration window
		LPCHARACTER ch = CQuestManager::Instance().GetCurrentCharacterPtr();
		LPPARTY pParty = ch ? ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)) : nullptr;

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
		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		LPPARTY pParty = ch ? ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)) : nullptr;
		PC* pPC = q.GetCurrentPC();

		const char* sz = lua_tostring(L,1);

		if (pParty)
		{
			FPartyCheckFlagLt f;
			f.flagname = pPC->GetCurrentQuestName() + "."+sz;
			f.value = (int) rint(lua_tonumber(L, 2));

			bool returnBool = pParty->ForEachOnMapMemberBool(f, ((ch)->GetMapIndex()));
			lua_pushboolean(L, returnBool);
		}

		return 1;
	}

	ALUA(party_set_flag)
	{
		// migrated from CHARACTER::GetParty()->SetFlag
		// DUAL-PATH: ECS update + legacy call during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)) && lua_isstring(L, 1) && lua_isnumber(L, 2))
			ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->SetFlag(lua_tostring(L, 1), (int)lua_tonumber(L, 2));

		return 0;
	}

	ALUA(party_get_flag)
	{
		// migrated from CHARACTER::GetParty()->GetFlag
		// DUAL-PATH: legacy fallback during migration window
		const entt::entity chEntity = CQuestManager::instance().GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)) || !lua_isstring(L, 1))
			lua_pushnumber(L, 0);
		else
			lua_pushnumber(L, ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->GetFlag(lua_tostring(L, 1)));

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

		const entt::entity chEntity = q.GetCurrentPCEntity();

		auto* ch = ecs::LegacyCharOf(chEntity);
		if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
			ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->ForEachOnlineMember(f);
		else
			f(ch);

		return 0;
	}

	ALUA(party_is_in_dungeon)
	{
		// migrated from CHARACTER::GetParty()->GetDungeon()
		entt::entity e = CQuestManager::instance().GetPCEntity(L);
		auto* pm = ECS_TryGet<ecs::PartyMembership>(e);
		if (!pm || !pm->party)
		{
			CQuestManager & q = CQuestManager::instance();
			const entt::entity chEntity = q.GetCurrentPCEntity();
			auto* ch = ecs::LegacyCharOf(chEntity);
			LPPARTY pParty = ch ? ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)) : nullptr;
			lua_pushboolean(L, (pParty != nullptr && pParty->GetDungeon()) ? true : false);
			return 1;
		}

		lua_pushboolean(L, pm->party->GetDungeon() ? true : false);
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
		void operator () (LPCHARACTER ch)
		{
			AffectSystem::AddAffect(AIHelpers::EcsOf(ch), dwType, bApplyOn, lApplyValue, dwFlag, lDuration, lSPCost, bOverride, IsCube);
		}
	};

	// ��Ƽ ������ ���� �ִ� �Լ�.
	// ���� �ʿ� �ִ� ��Ƽ���� ������ �޴´�.
	ALUA(party_give_buff)
	{
		// migrated from CHARACTER::AddAffect
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
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
		if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch)))
			ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch))->ForEachOnMapMember(f, ((ch)->GetMapIndex()));
		else
			f(ch);

		lua_pushboolean(L, true);
		return 1;
	}

	struct FPartyPIDCollector
	{
		std::vector <uint32_t> vecPIDs;
		FPartyPIDCollector()
		{
		}
		void operator () (LPCHARACTER ch)
		{
			vecPIDs.push_back(((ch)->GetPlayerID()));
		}
	};

	ALUA(party_get_member_pids)
	{
		// migrated from CHARACTER::GetParty()->ForEachOnMapMember
		// DUAL-PATH: legacy fallback during migration window
		CQuestManager & q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		LPPARTY pParty = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
		if (nullptr == pParty)
		{
			return 0;
		}
		FPartyPIDCollector f;
		pParty->ForEachOnMapMember(f, ((ch)->GetMapIndex()));

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
		std::string name = "";

		CQuestManager & q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
		{
			lua_pushstring(L, name.c_str());
			return 1;
		}

		LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
		if (party)
		{
			int32_t pid = party->GetLeaderPID();
			LPCHARACTER leader = CHARACTER_MANAGER::instance().FindByPID(pid);
			name = leader != nullptr ? leader->GetName() : ((ch)->GetName());
		}
		else
		{
			name = ((ch)->GetName());
		}

		lua_pushstring(L, name.c_str());
		return 1;
	}

	ALUA(party_give_gold)
	{
		// migrated from CHARACTER::PointChange(POINT_GOLD, ...)
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
		{
			return 0;
		}

		int64_t gold = (int64_t)lua_tonumber(L, -1);

		LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
		if (party)
		{
			FPartyPIDCollector f;
			party->ForEachOnMapMember(f, ((ch)->GetMapIndex()));

			for (auto it = f.vecPIDs.begin(); it != f.vecPIDs.end(); ++it)
			{
				LPCHARACTER tch = CHARACTER_MANAGER::instance().FindByPID(*it);
				if (tch && tch->IsPC())
				{
					if (gold + tch->GetGold() < 0)
					{
						sys_err("QUEST wrong ChangeGold {} (now {})", gold, tch->GetGold());
					}
					else
					{
						DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST, ((tch)->GetPlayerID()), gold);
						ecs::PointSystem::Change(AIHelpers::EcsOf(tch), POINT_GOLD, gold, true);
					}
				}
			}

			if (!q.GetPC(((ch)->GetPlayerID())))
			{
				sys_err("cannot return to main.");
			}
		}
		else
		{
			if (gold + ch->GetGold() < 0)
			{
				sys_err("QUEST wrong ChangeGold {} (now {})", gold, ch->GetGold());
			}
			else
			{
				DBManager::instance().SendMoneyLog(MONEY_LOG_QUEST, ((ch)->GetPlayerID()), gold);
				ecs::PointSystem::Change(AIHelpers::EcsOf(ch), POINT_GOLD, gold, true);
			}
		}

		return 0;
	}

	ALUA(party_give_blacksmith)
	{
		// migrated from CHARACTER::SetQuestFlag
		// DUAL-PATH: legacy only during migration window
		CQuestManager & q = CQuestManager::instance();
		const entt::entity chEntity = q.GetCurrentPCEntity();
		auto* ch = ecs::LegacyCharOf(chEntity);
		if (!ch)
		{
			return 0;
		}

		LPPARTY party = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(ch));
		if (party)
		{
			FPartyPIDCollector f;
			party->ForEachOnMapMember(f, ((ch)->GetMapIndex()));

			for (auto it = f.vecPIDs.begin(); it != f.vecPIDs.end(); it++)
			{
				LPCHARACTER tch = CHARACTER_MANAGER::instance().FindByPID(*it);
				if (tch && tch->IsPC())
				{
					ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(tch), "deviltower_zone.can_refine", 1);
				}
			}

			if (!q.GetPC(((ch)->GetPlayerID())))
			{
				sys_err("cannot return to main.");
			}
		}
		else
		{
			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(ch), "deviltower_zone.can_refine", 1);
		}

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






