#include "stdafx.h"
#include "../ecs/systems/CombatSystem.hpp"
#include <Core/Logging.hpp>
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "skill.h"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/AIHelpers.hpp"
#include "../ecs/Registry.hpp"
#include "utils.h"
#include "char_interface.hpp"
#include "party.h"
#include "char_manager.h"
#include "config.h"
#include "p2p.h"
#include "desc_client.h"
#include "dungeon.h"
#include "unique_item.h"
#include "../ecs/CharacterAccessors.hpp"
#include "../ecs/components/social_components.hpp"

CPartyManager::CPartyManager()
{
	Initialize();
}

CPartyManager::~CPartyManager()
{
}

void CPartyManager::Initialize()
{
	m_bEnablePCParty = false;
}

void CPartyManager::DeleteAllParty()
{
	while (!m_set_pkPCParty.empty())
	{
		const entt::entity party = *m_set_pkPCParty.begin();
		DeleteParty(party);
	}
}

bool CPartyManager::SetParty(entt::entity chEntity)
{
	TPartyMap::iterator it = m_map_pkParty.find((ecs::PlayerRuntime::GetPlayerID(chEntity)));

	if (it == m_map_pkParty.end())
		return false;

	PartySystem::Link(it->second, chEntity);
	return true;
}

void CPartyManager::P2PLogin(uint32_t pid, const char* name)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it == m_map_pkParty.end())
		return;

	PartySystem::UpdateOnlineState(it->second, pid, name);
}

void CPartyManager::P2PLogout(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it == m_map_pkParty.end())
		return;

	PartySystem::UpdateOfflineState(it->second, pid);
}

void CPartyManager::P2PJoinParty(uint32_t leader, uint32_t pid, uint8_t role)
{
	TPartyMap::iterator it = m_map_pkParty.find(leader);

	if (it != m_map_pkParty.end())
	{
		PartySystem::P2PJoin(it->second, pid);

		if (role >= PARTY_ROLE_MAX_NUM)
			role = PARTY_ROLE_NORMAL;

		PartySystem::SetRole(it->second, pid, role, true);
	}
	else
	{
		LOG_ERROR("No such party with leader [{}]", leader);
	}
}

void CPartyManager::P2PQuitParty(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it != m_map_pkParty.end())
	{
		PartySystem::P2PQuit(it->second, pid);
	}
	else
	{
		LOG_ERROR("No such party with member [{}]", pid);
	}
}

entt::entity CPartyManager::P2PCreateParty(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);
	if (it != m_map_pkParty.end() && PartySystem::IsValid(it->second))
		return it->second;

	const entt::entity party = g_registry.create();
	g_registry.emplace<ecs::PartyState>(party);
	PartySystem::Initialize(party);

	m_set_pkPCParty.insert(party);

	SetPartyMember(pid, party);
	PartySystem::SetPCParty(party, true);
	PartySystem::P2PJoin(party, pid);

	return party;
}

void CPartyManager::P2PDeleteParty(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it != m_map_pkParty.end())
	{
		const entt::entity party = it->second;
		m_set_pkPCParty.erase(party);
		PartySystem::Destroy(party);
	}
	else
		LOG_ERROR("PARTY P2PDeleteParty Cannot find party [{}]", pid);
}

entt::entity CPartyManager::CreateParty(entt::entity leader)
{
	const entt::entity existing = ecs::SocialSystem::GetParty(leader);
	if (existing != entt::null)
		return existing;

	const entt::entity party = g_registry.create();
	auto& state = g_registry.emplace<ecs::PartyState>(party);
	PartySystem::Initialize(party);

	if (ecs::PlayerRuntime::IsPC(leader))
	{
		//TPacketGGParty p;
		//p.header	= HEADER_GG_PARTY;
		//p.subheader	= PARTY_SUBHEADER_GG_CREATE;
		//p.pid		= ecs::PlayerRuntime::GetPlayerID(leader);
		//P2P_MANAGER::instance().Send(&p, sizeof(p));
		TPacketPartyCreate p;
		p.dwLeaderPID = (ecs::PlayerRuntime::GetPlayerID(leader));

		db_clientdesc->DBPacket(HEADER_GD_PARTY_CREATE, 0, &p, sizeof(TPacketPartyCreate));

		LOG_INFO("PARTY: Create {} pid {}", ecs::PlayerRuntime::GetName(leader).data(), (ecs::PlayerRuntime::GetPlayerID(leader)));
		state.isPCParty = true;
		PartySystem::Join(party, (ecs::PlayerRuntime::GetPlayerID(leader)));

		m_set_pkPCParty.insert(party);
	}
	else
	{
		state.isPCParty = false;
		PartySystem::Join(party, ecs::PlayerRuntime::GetPacketVID(leader));
	}

	PartySystem::Link(party, leader);
	return (party);
}

void CPartyManager::DeleteParty(entt::entity party)
{
	//TPacketGGParty p;
	//p.header = HEADER_GG_PARTY;
	//p.subheader = PARTY_SUBHEADER_GG_DESTROY;
	//p.pid = pParty->GetLeaderPID();
	//P2P_MANAGER::instance().Send(&p, sizeof(p));
	TPacketPartyDelete p;
	p.dwLeaderPID = PartySystem::GetLeaderPID(party);

	db_clientdesc->DBPacket(HEADER_GD_PARTY_DELETE, 0, &p, sizeof(TPacketPartyDelete));

	m_set_pkPCParty.erase(party);
	PartySystem::Destroy(party);
}

void CPartyManager::SetPartyMember(uint32_t dwPID, entt::entity party)
{
	TPartyMap::iterator it = m_map_pkParty.find(dwPID);

	if (party == entt::null)
	{
		if (it != m_map_pkParty.end())
			m_map_pkParty.erase(it);
	}
	else
	{
		if (it != m_map_pkParty.end())
		{
			if (it->second != party)
			{
				const uint32_t pid = dwPID;
				const entt::entity previous = it->second;
				PartySystem::Quit(previous, pid);
				// Quit may have erased this index entry along the way.
				m_map_pkParty[pid] = party;
			}
		}
		else
			m_map_pkParty.insert(TPartyMap::value_type(dwPID, party));
	}
}

/////////////////////////////////////////////////////////////////////////////
//
// PartySystem begin!
//
/////////////////////////////////////////////////////////////////////////////

namespace PartySystem
{
	using namespace std;

	EVENTINFO(party_update_info)
	{
		entt::entity party { entt::null };

		party_update_info()
		{
		}
	};

	EVENTFUNC(party_update_event)
	{
		party_update_info* info = dynamic_cast<party_update_info*>( event->info );

		if ( info == nullptr)
		{
			LOG_ERROR("party_update_event> <Factor> Null pointer");
			return 0;
		}

		ecs::PartyState* state = Find(info->party);

		// A cancelled or replaced callback cannot act on a recycled party.
		if (!state || state->updateEvent != event)
			return 0;

		const entt::entity leader = GetLeader(info->party);

		if (leader != entt::null && ecs::PlayerRuntime::GetDesc(leader))
			Update(info->party);

		return PASSES_PER_SEC(3);
	}

	bool IsValid(entt::entity party)
	{
		return Find(party) != nullptr;
	}

	entt::entity GetCharacterParty(entt::entity character)
	{
		if (character == entt::null || !g_registry.valid(character))
			return entt::null;

		const auto* refs = g_registry.try_get<ecs::SocialRefs>(character);

		if (!refs || refs->party == entt::null)
			return entt::null;

		return IsValid(refs->party) ? refs->party : entt::null;
	}

	void SetCharacterParty(entt::entity character, entt::entity party)
	{
		if (character == entt::null || !g_registry.valid(character))
			return;

		auto& refs = g_registry.get_or_emplace<ecs::SocialRefs>(character);

		if (party != entt::null && !IsValid(party))
			party = entt::null;

		refs.party = party;
	}

	void Initialize(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		LOG_TRACE("Party::Initialize");

		state->expDistributionMode = PARTY_EXP_DISTRIBUTION_NON_PARITY;

		state->leaderPID = 0;
		state->nextOwnerPID = 0;
		state->updateEvent = nullptr;

		memset(&state->roleCount, 0, sizeof(state->roleCount));
		memset(&state->maxRole, 0, sizeof(state->maxRole));
		state->maxRole[PARTY_ROLE_LEADER] = 1;
		state->maxRole[PARTY_ROLE_NORMAL] = 32;

		state->startTime = get_dword_time();
		state->longTimeExpBonus = 0;

		state->healTime = get_dword_time();
		state->healReady = false;
		state->canUsePartyHeal = false;

		state->leadership = 0;
		state->expBonus = 0;
		state->attBonus = 0;
		state->defBonus = 0;

		state->nearMemberCount = 0;

		state->isPCParty = false;
		state->dungeon = entt::null;
	}

	namespace
	{
		void IncreaseOwnership(ecs::PartyState& state)
		{
			if (state.members.empty())
			{
				state.nextOwnerPID = 0;
				return;
			}

			auto it = state.members.find(state.nextOwnerPID);

			if (it == state.members.end())
			{
				state.nextOwnerPID = state.members.begin()->first;
				return;
			}

			++it;
			state.nextOwnerPID = (it == state.members.end()) ? state.members.begin()->first : it->first;
		}

		void RemoveBonus(ecs::PartyState& state, entt::entity party)
		{
			for (auto& row : state.members)
			{
				if (IsLinked(row.second.member))
					ComputeRolePoint(party, row.second.member, row.second.bRole, false);

				row.second.bNear = false;
			}
		}

		void RemoveBonusForOne(ecs::PartyState& state, entt::entity party, uint32_t pid)
		{
			auto it = state.members.find(pid);

			if (it == state.members.end())
				return;

			if (IsLinked(it->second.member))
				ComputeRolePoint(party, it->second.member, it->second.bRole, false);
		}
	}

	void Destroy(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		LOG_TRACE("Party::Destroy");

		if (state->isPCParty)
		{
			for (auto& row : state->members)
				CPartyManager::instance().SetPartyMember(row.first, entt::null);

			// The index writes above can retire rows; re-check before reading on.
			state = Find(party);
			if (!state)
				return;
		}

		event_cancel(&state->updateEvent);

		RemoveBonus(*state, party);

		// A disconnect callback can retire a member while this loop runs, so
		// the pids are snapshotted and each row is re-resolved.
		std::vector<uint32_t> pids;
		pids.reserve(state->members.size());

		for (auto& row : state->members)
			pids.push_back(row.first);

		const uint32_t dwTime = get_dword_time();

		for (const uint32_t pid : pids)
		{
			state = Find(party);
			if (!state)
				return;

			auto it = state->members.find(pid);

			if (it == state->members.end())
				continue;

			const entt::entity member = it->second.member;

			if (IsLinked(member))
			{
				if (ecs::PlayerRuntime::GetDesc(member))
				{
					TPacketGCPartyRemove p;
					p.header = HEADER_GC_PARTY_REMOVE;
					p.pid = ecs::PlayerRuntime::GetPlayerID(member);
					ecs::PlayerRuntime::GetDesc(member)->Packet(&p, sizeof(p));
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(member, CHAT_TYPE_INFO, 213, "");
#endif
				}
				else
				{
					CombatSystem::SetLastAttacked(member, dwTime);
					ecs::PlayerRuntime::StartDestroyWhenIdleEvent(member);
				}

				ecs::SocialSystem::SetParty(member, entt::null);
			}
		}

		state = Find(party);
		if (!state)
			return;

		state->members.clear();
		state->nextOwnerPID = 0;

		g_registry.destroy(party);
	}

	void SetPCParty(entt::entity party, bool b)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		state->isPCParty = b;
	}

#ifdef TEXTS_IMPROVEMENT
	void ChatPacketToAllMemberNew(entt::entity party, uint8_t type, uint32_t idx, const char * format, ...)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		char chatbuf[256];
		va_list args;
		va_start(args, format);
		vsnprintf(chatbuf, sizeof(chatbuf), format, args);
		va_end(args);

		for (auto& row : state->members)
		{
			if (IsLinked(row.second.member))
				ecs::ChatSystem::SendNew(row.second.member, type, idx, "%s", chatbuf);
		}
	}
#endif

	uint32_t GetLeaderPID(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		return state ? state->leaderPID : 0;
	}

	uint32_t GetMemberCount(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		return state ? static_cast<uint32_t>(state->members.size()) : 0;
	}

	uint32_t GetNearMemberCount(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		return state ? static_cast<uint32_t>(state->nearMemberCount) : 0;
	}

	bool IsMember(entt::entity party, uint32_t pid)
	{
		ecs::PartyState* state = Find(party);
		return state && state->members.find(pid) != state->members.end();
	}

	void P2PJoin(entt::entity party, uint32_t dwPID)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		auto it = state->members.find(dwPID);

		if (it == state->members.end())
		{
			ecs::PartyMember Member;

			Member.member	= entt::null;
			Member.bNear		= false;

			if (state->members.empty())
			{
				Member.bRole = PARTY_ROLE_LEADER;
				state->leaderPID = dwPID;
			}
			else
				Member.bRole = PARTY_ROLE_NORMAL;

			if (state->isPCParty)
			{
				const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID);

				if (ecs::IsCharacter(ch))
				{
					LOG_INFO("PARTY: Join {} pid {} leader {}", ecs::PlayerRuntime::GetName(ch).data(), dwPID, state->leaderPID);
					Member.strName = ecs::PlayerRuntime::GetName(ch).data();

					if (Member.bRole == PARTY_ROLE_LEADER)
						state->leadership = SkillSystem::GetSkillLevel(ch, SKILL_LEADERSHIP);
				}
				else
				{
					CCI * pcci = P2P_MANAGER::instance().FindByPID(dwPID);

					if (!pcci);
					else if (pcci->bChannel == g_bChannel)
						Member.strName = pcci->szName;
					else
						LOG_ERROR("member is not in same channel PID: {} channel {}, this channel {}", dwPID, static_cast<int>(pcci->bChannel), static_cast<int>(g_bChannel));
				}
			}

			LOG_TRACE("PARTY[{}] MemberCountChange {} -> {}", state->leaderPID, state->members.size(), state->members.size()+1);

			state->members.insert(std::map<uint32_t, ecs::PartyMember>::value_type(dwPID, Member));

			if (state->members.size() == 1)
				state->nextOwnerPID = dwPID;

			if (state->isPCParty)
			{
				CPartyManager::instance().SetPartyMember(dwPID, party);
				SendPartyJoinOneToAll(party, dwPID);

				const entt::entity ch = CHARACTER_MANAGER::instance().FindEntityByPID(dwPID);

				if (ecs::IsCharacter(ch))
					SendParameter(party, ch);
			}
		}

		if (state->dungeon != entt::null)
		{
			DungeonSystem::QuitParty(state->dungeon, party);
		}
	}

	void Join(entt::entity party, uint32_t dwPID)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		const bool isPCParty = state->isPCParty;

		P2PJoin(party, dwPID);

		if (isPCParty)
		{
			TPacketPartyAdd p;
			p.dwLeaderPID = GetLeaderPID(party);
			p.dwPID = dwPID;
			p.bState = PARTY_ROLE_NORMAL;
			db_clientdesc->DBPacket(HEADER_GD_PARTY_ADD, 0, &p, sizeof(TPacketPartyAdd));
		}
	}

	void P2PQuit(entt::entity party, uint32_t dwPID)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		auto it = state->members.find(dwPID);

		if (it == state->members.end())
			return;

		if (state->isPCParty)
			SendPartyRemoveOneToAll(party, dwPID);

		if (dwPID == state->nextOwnerPID)
			IncreaseOwnership(*state);

		if (state->isPCParty)
			RemoveBonusForOne(*state, party, dwPID);

		const entt::entity member = it->second.member;
		uint8_t bRole = it->second.bRole;

		state->members.erase(it);

		LOG_TRACE("PARTY[{}] MemberCountChange {} -> {}", state->leaderPID, state->members.size(), state->members.size() - 1);

		if (bRole < PARTY_ROLE_MAX_NUM)
		{
			--state->roleCount[bRole];
		}
		else
		{
			LOG_ERROR("ROLE_COUNT_QUIT_ERROR: INDEX({}) > MAX({})", bRole, PARTY_ROLE_MAX_NUM);
		}

		if (IsLinked(member))
		{
			ecs::SocialSystem::SetParty(member, entt::null);
			ComputeRolePoint(party, member, bRole, false);
		}

		if (state->isPCParty)
			CPartyManager::instance().SetPartyMember(dwPID, entt::null);

		if (bRole == PARTY_ROLE_LEADER)
			CPartyManager::instance().DeleteParty(party);

	}

	void Quit(entt::entity party, uint32_t dwPID)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		// Always PC
		P2PQuit(party, dwPID);

		state = Find(party);
		if (!state)
			return;

		if (state->isPCParty && dwPID != state->leaderPID)
		{
			//TPacketGGParty p;
			//p.header = HEADER_GG_PARTY;
			//p.subheader = PARTY_SUBHEADER_GG_QUIT;
			//p.pid = dwPID;
			//p.leaderpid = GetLeaderPID();
			//P2P_MANAGER::instance().Send(&p, sizeof(p));
			TPacketPartyRemove p;
			p.dwPID = dwPID;
			p.dwLeaderPID = state->leaderPID;
			db_clientdesc->DBPacket(HEADER_GD_PARTY_REMOVE, 0, &p, sizeof(TPacketPartyRemove));
		}
	}

	void Link(entt::entity party, entt::entity character)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		auto it = (ecs::PlayerRuntime::IsPC(character))
			? state->members.find(ecs::PlayerRuntime::GetPlayerID(character))
			: state->members.find(ecs::PlayerRuntime::GetPacketVID(character));

		if (it == state->members.end())
		{
			LOG_ERROR("{} is not member of this party", ecs::PlayerRuntime::GetName(character).data());
			return;
		}

		if (state->isPCParty && !state->updateEvent)
		{
			party_update_info* info = AllocEventInfo<party_update_info>();
			info->party = party;
			state->updateEvent = event_create(party_update_event, info, PASSES_PER_SEC(3));
		}

		LOG_TRACE("PARTY[{}] {} linked to party", state->leaderPID, ecs::PlayerRuntime::GetName(character).data());

		it->second.member = character;
		ecs::SocialSystem::SetParty(character, party);

		if (ecs::PlayerRuntime::IsPC(character))
		{
			if (it->second.strName.empty())
			{
				it->second.strName = ecs::PlayerRuntime::GetName(character).data();
			}

			SendPartyJoinOneToAll(party, (ecs::PlayerRuntime::GetPlayerID(character)));

			SendPartyJoinAllToOne(party, character);
			SendPartyLinkOneToAll(party, character);
			SendPartyLinkAllToOne(party, character);
			SendPartyInfoAllToOne(party, character);
			SendPartyInfoOneToAll(party, character);

			SendParameter(party, character);

			//LOG_INFO("PARTY-DUNGEON connect {} {}", static_cast<const void*>(this), static_cast<const void*>(GetDungeon()));
			const entt::entity partyDungeon = GetDungeon(party);

			if (partyDungeon != entt::null && DungeonSystem::GetMapIndex(partyDungeon) == ecs::PlayerRuntime::GetMapIndex(character))
			{
				ecs::SocialSystem::SetDungeon(character, partyDungeon);
			}

			RequestSetMemberLevel(party, (ecs::PlayerRuntime::GetPlayerID(character)), (ecs::PointSystem::GetLevel(character)));

		}
	}

	void RequestSetMemberLevel(entt::entity party, uint32_t pid, uint8_t level)
	{
		TPacketPartySetMemberLevel p;
		p.dwLeaderPID = GetLeaderPID(party);
		p.dwPID = pid;
		p.bLevel = level;
		db_clientdesc->DBPacket(HEADER_GD_PARTY_SET_MEMBER_LEVEL, 0, &p, sizeof(TPacketPartySetMemberLevel));
	}

	void P2PSetMemberLevel(entt::entity party, uint32_t pid, uint8_t level)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		if (!state->isPCParty)
			return;

		LOG_TRACE("PARTY P2PSetMemberLevel leader {} pid {} level {}", state->leaderPID, pid, static_cast<int>(level));

		auto it = state->members.find(pid);
		if (it != state->members.end())
		{
			it->second.bLevel = level;
		}
	}

	namespace
	{
		struct FExitDungeon
		{
			void operator()(entt::entity member)
			{
				ecs::MovementSystem::ExitToSavedLocation(member);
			}
		};
	}

	void Unlink(entt::entity party, entt::entity character)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		auto it = (ecs::PlayerRuntime::IsPC(character))
			? state->members.find(ecs::PlayerRuntime::GetPlayerID(character))
			: state->members.find(ecs::PlayerRuntime::GetPacketVID(character));

		if (it == state->members.end())
		{
			LOG_ERROR("{} is not member of this party", ecs::PlayerRuntime::GetName(character).data());
			return;
		}

		if (ecs::PlayerRuntime::IsPC(character))
		{
			SendPartyUnlinkOneToAll(party, character);

			if (it->second.bRole == PARTY_ROLE_LEADER)
			{
				RemoveBonus(*state, party);

				if (ecs::SocialSystem::GetDungeon(it->second.member) != entt::null)
				{
					FExitDungeon f;
					ForEachNearMember(party, f);
				}
			}
		}

		it->second.member = entt::null;
		ecs::SocialSystem::SetParty(character, entt::null);
	}

	void SendPartyRemoveOneToAll(entt::entity party, uint32_t pid)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		TPacketGCPartyRemove p;
		p.header = HEADER_GC_PARTY_REMOVE;
		p.pid = pid;

		for (auto& row : state->members)
		{
			if (IsLinked(row.second.member) && ecs::PlayerRuntime::GetDesc(row.second.member))
				ecs::PlayerRuntime::GetDesc(row.second.member)->Packet(&p, sizeof(p));
		}
	}

	void SendPartyJoinOneToAll(entt::entity party, uint32_t pid)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		auto itMember = state->members.find(pid);

		if (itMember == state->members.end())
			return;

		const ecs::PartyMember& r = itMember->second;

		TPacketGCPartyAdd p;

		p.header = HEADER_GC_PARTY_ADD;
		p.pid = pid;
		strlcpy(p.name, r.strName.c_str(), sizeof(p.name));

		for (auto& row : state->members)
		{
			if (IsLinked(row.second.member) && ecs::PlayerRuntime::GetDesc(row.second.member))
				ecs::PlayerRuntime::GetDesc(row.second.member)->Packet(&p, sizeof(p));
		}
	}

	void SendPartyJoinAllToOne(entt::entity party, entt::entity character)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		TPacketGCPartyAdd p;

		p.header = HEADER_GC_PARTY_ADD;
		p.name[CHARACTER_NAME_MAX_LEN] = '\0';

		for (auto& row : state->members)
		{
			p.pid = row.first;
			strlcpy(p.name, row.second.strName.c_str(), sizeof(p.name));
			ecs::PlayerRuntime::GetDesc(character)->Packet(&p, sizeof(p));
		}
	}

	void SendPartyUnlinkOneToAll(entt::entity party, entt::entity character)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		TPacketGCPartyLink p;
		p.header = HEADER_GC_PARTY_UNLINK;
		p.pid = (ecs::PlayerRuntime::GetPlayerID(character));
		p.vid = ecs::PlayerRuntime::GetPacketVID(character);

		for (auto& row : state->members)
		{
			if (IsLinked(row.second.member) && ecs::PlayerRuntime::GetDesc(row.second.member))
			{
				ecs::PlayerRuntime::GetDesc(row.second.member)->Packet(&p, sizeof(p));
			}
		}
	}

	void SendPartyLinkOneToAll(entt::entity party, entt::entity character)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		TPacketGCPartyLink p;
		p.header = HEADER_GC_PARTY_LINK;
		p.vid = ecs::PlayerRuntime::GetPacketVID(character);
		p.pid = (ecs::PlayerRuntime::GetPlayerID(character));

		for (auto& row : state->members)
		{
			if (IsLinked(row.second.member) && ecs::PlayerRuntime::GetDesc(row.second.member))
			{
				ecs::PlayerRuntime::GetDesc(row.second.member)->Packet(&p, sizeof(p));
			}
		}
	}

	void SendPartyLinkAllToOne(entt::entity party, entt::entity character)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		TPacketGCPartyLink p;
		p.header = HEADER_GC_PARTY_LINK;

		for (auto& row : state->members)
		{
			if (IsLinked(row.second.member))
			{
				p.vid = ecs::PlayerRuntime::GetPacketVID(row.second.member);
				p.pid = (ecs::PlayerRuntime::GetPlayerID(row.second.member));
				ecs::PlayerRuntime::GetDesc(character)->Packet(&p, sizeof(p));
			}
		}
	}

	void SendPartyInfoOneToAll(entt::entity party, uint32_t pid)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		auto it = state->members.find(pid);

		if (it == state->members.end())
			return;

		if (IsLinked(it->second.member))
		{
			SendPartyInfoOneToAll(party, it->second.member);
			return;
		}

		// Data Building
		TPacketGCPartyUpdate p;
		memset(&p, 0, sizeof(p));
		p.header = HEADER_GC_PARTY_UPDATE;
		p.pid = pid;
		p.percent_hp = 255;
		p.role = it->second.bRole;

		for (auto& row : state->members)
		{
			if (IsLinked(row.second.member) && (ecs::PlayerRuntime::GetDesc(row.second.member)))
			{
				//LOG_TRACE("PARTY send info {}[{}] to {}[{}]", ecs::PlayerRuntime::GetName(((ch) ? (ch)->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetPacketVID(((ch) ? (ch)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetName(row.second.member).data(), ecs::PlayerRuntime::GetPacketVID(row.second.member));
				ecs::PlayerRuntime::GetDesc(row.second.member)->Packet(&p, sizeof(p));
			}
		}
	}

	void SendPartyInfoOneToAll(entt::entity party, entt::entity character)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		if (!ecs::PlayerRuntime::GetDesc(character))
			return;

		// Data Building
		TPacketGCPartyUpdate p;
		NetworkSyncSystem::BuildPartyUpdatePacket(g_registry, character, p);

		for (auto& row : state->members)
		{
			if (IsLinked(row.second.member) && (ecs::PlayerRuntime::GetDesc(row.second.member)))
			{
				LOG_TRACE("PARTY send info {}[{}] to {}[{}]", ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetPacketVID(character), ecs::PlayerRuntime::GetName(row.second.member).data(), ecs::PlayerRuntime::GetPacketVID(row.second.member));
				ecs::PlayerRuntime::GetDesc(row.second.member)->Packet(&p, sizeof(p));
			}
		}
	}

	void SendPartyInfoAllToOne(entt::entity party, entt::entity character)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		TPacketGCPartyUpdate p;

		for (auto& row : state->members)
		{
			if (!IsLinked(row.second.member))
			{
				uint32_t pid = row.first;
				memset(&p, 0, sizeof(p));
				p.header = HEADER_GC_PARTY_UPDATE;
				p.pid = pid;
				p.percent_hp = 255;
				p.role = row.second.bRole;
				ecs::PlayerRuntime::GetDesc(character)->Packet(&p, sizeof(p));
				continue;
			}

			NetworkSyncSystem::BuildPartyUpdatePacket(g_registry, row.second.member, p);
			LOG_TRACE("PARTY send info {}[{}] to {}[{}]", ecs::PlayerRuntime::GetName(row.second.member).data(), ecs::PlayerRuntime::GetPacketVID(row.second.member), ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetPacketVID(character));
			ecs::PlayerRuntime::GetDesc(character)->Packet(&p, sizeof(p));
		}
	}

	void SendMessage(entt::entity party, entt::entity character, uint8_t bMsg, uint32_t dwArg1, uint32_t dwArg2)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		if (ecs::SocialSystem::GetParty(character) != party)
		{
			LOG_ERROR("{} is not member of this party {}", ecs::PlayerRuntime::GetName(character).data(), static_cast<uint32_t>(party));
			return;
		}

		switch (bMsg)
		{
			case PM_ATTACK:
				break;

			case PM_RETURN:
				{
					for (auto& row : state->members)
					{
						const entt::entity other = row.second.member;

						if (IsLinked(other) && other != character)
						{
							uint32_t x = dwArg1 + number(-500, 500);
							uint32_t y = dwArg2 + number(-500, 500);

							CombatSystem::SetVictim(other, entt::null);
							ecs::MovementSystem::SetRotationToXY(other, x, y);

							if (ecs::MovementSystem::Goto(other, x, y))
							{
								const entt::entity victim = CombatSystem::GetVictim(other);
								LOG_TRACE("{} {} RETURN victim {}", ecs::PlayerRuntime::GetName(other).data(), static_cast<uint32_t>(other), static_cast<uint32_t>(victim));
								ecs::MovementSystem::SendMovePacket(other, FUNC_WAIT, 0, 0, 0, 0);
							}
						}
					}
				}
				break;

			case PM_ATTACKED_BY:
				{
					const entt::entity victimEntity = CombatSystem::GetVictim(character);

					if (victimEntity == entt::null)
						return;

					for (auto& row : state->members)
					{
						const entt::entity other = row.second.member;

						if (IsLinked(other) && other != character)
						{
							if (CombatSystem::CanBeginFight(other))
								CombatSystem::BeginFight(other, victimEntity);
						}
					}
				}
				break;

			case PM_AGGRO_INCREASE:
				{
					const entt::entity victim = CHARACTER_MANAGER::instance().FindEntity(dwArg2);

					if (victim == entt::null)
						return;

					for (auto& row : state->members)
					{
						const entt::entity other = row.second.member;

						if (IsLinked(other) && other != character)
						{
							CombatSystem::UpdateAggrPoint(other, victim, DAMAGE_TYPE_SPECIAL, dwArg1);
						}
					}
				}
				break;
		}
	}

	// The leader is the member holding the leader PID. m_pkChrLeader kept the same
	// pointer a second time, and operator[] here inserted an empty member whenever
	// that row was missing.
	entt::entity GetLeader(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return entt::null;

		const auto it = state->members.find(state->leaderPID);
		return it != state->members.end() && IsLinked(it->second.member) ? it->second.member : entt::null;
	}

	bool SetRole(entt::entity party, uint32_t dwPID, uint8_t bRole, bool bSet)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return false;

		auto it = state->members.find(dwPID);

		if (it == state->members.end())
		{
			return false;
		}

		const entt::entity chEntity = it->second.member;


		if (bSet)
		{
			if (state->roleCount[bRole] >= state->maxRole[bRole])
				return false;

			if (it->second.bRole != PARTY_ROLE_NORMAL)
				return false;

			it->second.bRole = bRole;

			if (IsLinked(chEntity) && GetLeader(party) != entt::null)
				ComputeRolePoint(party, chEntity, bRole, true);

			if (bRole < PARTY_ROLE_MAX_NUM)
			{
				++state->roleCount[bRole];
			}
			else
			{
			LOG_ERROR("ROLE_COUNT_INC_ERROR: INDEX({}) > MAX({})", static_cast<int>(bRole), PARTY_ROLE_MAX_NUM);
			}
		}
		else
		{
			if (it->second.bRole == PARTY_ROLE_LEADER)
				return false;

			if (it->second.bRole == PARTY_ROLE_NORMAL)
				return false;

			it->second.bRole = PARTY_ROLE_NORMAL;

			if (IsLinked(chEntity) && GetLeader(party) != entt::null)
				ComputeRolePoint(party, chEntity, PARTY_ROLE_NORMAL, false);

			if (bRole < PARTY_ROLE_MAX_NUM)
			{
				--state->roleCount[bRole];
			}
			else
			{
				LOG_ERROR("ROLE_COUNT_DEC_ERROR: INDEX({}) > MAX({})", static_cast<int>(bRole), PARTY_ROLE_MAX_NUM);
			}
		}

		SendPartyInfoOneToAll(party, dwPID);
		return true;
	}

	uint8_t GetRole(entt::entity party, uint32_t pid)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return PARTY_ROLE_NORMAL;

		auto it = state->members.find(pid);

		if (it == state->members.end())
			return PARTY_ROLE_NORMAL;

		return it->second.bRole;
	}

	bool IsRole(entt::entity party, uint32_t pid, uint8_t bRole)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return false;

		auto it = state->members.find(pid);

		if (it == state->members.end())
			return false;

		return it->second.bRole == bRole;
	}

	void HealParty(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		{
			return;
		}
		if (!state->healReady)
			return;

		const entt::entity lEntity = GetLeader(party);

		for (auto& row : state->members)
		{
			if (!IsLinked(row.second.member))
				continue;

			const entt::entity chEntity = row.second.member;


			if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(lEntity)-ecs::PlayerRuntime::GetX(chEntity), ecs::PlayerRuntime::GetY(lEntity)-ecs::PlayerRuntime::GetY(chEntity)) < PARTY_DEFAULT_RANGE)
			{
				ecs::PointSystem::Change(chEntity, POINT_HP, ecs::PointSystem::GetMaxHP(chEntity)-ecs::PlayerRuntime::GetHP(chEntity));
				ecs::PointSystem::Change(chEntity, POINT_SP, ecs::PointSystem::GetMaxSP(chEntity)-ecs::PlayerRuntime::GetSP(chEntity));
			}
		}

		state->healReady = false;
		state->healTime = get_dword_time();
	}

	void SummonToLeader(entt::entity party, uint32_t pid)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		int xy[12][2] =
		{
			{	250,	0		},
			{	216,	125		},
			{	125,	216		},
			{	0,		250		},
			{	-125,	216		},
			{	-216,	125		},
			{	-250,	0		},
			{	-216,	-125	},
			{	-125,	-216	},
			{	0,		-250	},
			{	125,	-216	},
			{	216,	-125	},
		};

		int n = 0;
		int x[12], y[12];

		SECTREE_MANAGER & s = SECTREE_MANAGER::instance();
		const entt::entity lEntity = GetLeader(party);


		auto itMember = state->members.find(pid);

		if (itMember == state->members.end())
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(lEntity, CHAT_TYPE_INFO, 209, "");
#endif
			return;
		}

		const entt::entity chEntity = itMember->second.member;


		if (!IsLinked(chEntity))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(lEntity, CHAT_TYPE_INFO, 209, "");
#endif
			return;
		}

		if (!CombatSystem::CanSummon(chEntity, state->leadership))
		{
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(lEntity, CHAT_TYPE_INFO, 198, "");
#endif
			return;
		}

		for (int i = 0; i < 12; ++i)
		{
			PIXEL_POSITION p;

			if (s.GetMovablePosition(ecs::PlayerRuntime::GetMapIndex(lEntity), ecs::PlayerRuntime::GetX(lEntity) + xy [i][0], ecs::PlayerRuntime::GetY(lEntity) + xy[i][1], p))
			{
				x[n] = p.x;
				y[n] = p.y;
				n++;
			}
		}

		if (n != 0) {
			int i = number(0, n - 1);
			ecs::MovementSystem::Show(chEntity, ecs::PlayerRuntime::GetMapIndex(lEntity), x[i], y[i]);
			ecs::MovementSystem::Stop(chEntity);
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 219, "");
		}
#endif
	}

	entt::entity GetNextOwnership(entt::entity party, entt::entity fallback, int32_t x, int32_t y)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return fallback;

		if (state->nextOwnerPID == 0)
			return fallback;

		int size = state->members.size();

		while (size-- > 0)
		{
			auto it = state->members.find(state->nextOwnerPID);

			if (it == state->members.end())
				return fallback;

			const entt::entity member = it->second.member;

			if (IsLinked(member) && DISTANCE_APPROX(ecs::PlayerRuntime::GetX(member) - x, ecs::PlayerRuntime::GetY(member) - y) < 3000)
			{
				IncreaseOwnership(*state);
				return member;
			}

			IncreaseOwnership(*state);
		}

		return fallback;
	}

	void ComputeRolePoint(entt::entity party, entt::entity character, uint8_t bRole, bool bAdd)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		if (!bAdd)
		{
			ecs::PointSystem::Change(character, POINT_PARTY_ATTACKER_BONUS, -ecs::PointSystem::Get(character, POINT_PARTY_ATTACKER_BONUS));
			ecs::PointSystem::Change(character, POINT_PARTY_TANKER_BONUS, -ecs::PointSystem::Get(character, POINT_PARTY_TANKER_BONUS));
			ecs::PointSystem::Change(character, POINT_PARTY_BUFFER_BONUS, -ecs::PointSystem::Get(character, POINT_PARTY_BUFFER_BONUS));
			ecs::PointSystem::Change(character, POINT_PARTY_SKILL_MASTER_BONUS, -ecs::PointSystem::Get(character, POINT_PARTY_SKILL_MASTER_BONUS));
			ecs::PointSystem::Change(character, POINT_PARTY_DEFENDER_BONUS, -ecs::PointSystem::Get(character, POINT_PARTY_DEFENDER_BONUS));
			ecs::PointSystem::Change(character, POINT_PARTY_HASTE_BONUS, -ecs::PointSystem::Get(character, POINT_PARTY_HASTE_BONUS));
			ecs::PointSystem::ComputeBattlePoints(character);
			return;
		}

		//SKILL_POWER_BY_LEVEL
		float k = (float) ecs::PlayerRuntime::GetSkillPowerByLevel(character, MIN(SKILL_MAX_LEVEL, state->leadership)) / 100.0f;
		//float k = (float) aiSkillPowerByLevel[MIN(SKILL_MAX_LEVEL, m_iLeadership)] / 100.0f;
		//
		//LOG_INFO("ComputeRolePoint {}i {}, {}", k, SKILL_MAX_LEVEL, m_iLeadership);
		//END_SKILL_POWER_BY_LEVEL

		switch (bRole)
		{
			case PARTY_ROLE_ATTACKER:
				{
					//int iBonus = (int) (10 + 90 * k);
					int iBonus = (int) (10 + 60 * k);

					if (ecs::PointSystem::Get(character, POINT_PARTY_ATTACKER_BONUS) != iBonus)
					{
						ecs::PointSystem::Change(character, POINT_PARTY_ATTACKER_BONUS, iBonus - ecs::PointSystem::Get(character, POINT_PARTY_ATTACKER_BONUS));
						ecs::PointSystem::Compute(character);
					}
				}
				break;

			case PARTY_ROLE_TANKER:
				{
					int iBonus = (int) (50 + 1450 * k);

					if (ecs::PointSystem::Get(character, POINT_PARTY_TANKER_BONUS) != iBonus)
					{
						ecs::PointSystem::Change(character, POINT_PARTY_TANKER_BONUS, iBonus - ecs::PointSystem::Get(character, POINT_PARTY_TANKER_BONUS));
						ecs::PointSystem::Compute(character);
					}
				}
				break;

			case PARTY_ROLE_BUFFER:
				{
					int iBonus = (int) (5 + 45 * k);

					if (ecs::PointSystem::Get(character, POINT_PARTY_BUFFER_BONUS) != iBonus)
					{
						ecs::PointSystem::Change(character, POINT_PARTY_BUFFER_BONUS, iBonus - ecs::PointSystem::Get(character, POINT_PARTY_BUFFER_BONUS));
					}
				}
				break;

			case PARTY_ROLE_SKILL_MASTER:
				{
					int iBonus = (int) (25 + 600 * k);

					if (ecs::PointSystem::Get(character, POINT_PARTY_SKILL_MASTER_BONUS) != iBonus)
					{
						ecs::PointSystem::Change(character, POINT_PARTY_SKILL_MASTER_BONUS, iBonus - ecs::PointSystem::Get(character, POINT_PARTY_SKILL_MASTER_BONUS));
						ecs::PointSystem::Compute(character);
					}
				}
				break;
			case PARTY_ROLE_HASTE:
				{
					int iBonus = (int) (1+5*k);
					if (ecs::PointSystem::Get(character, POINT_PARTY_HASTE_BONUS) != iBonus)
					{
						ecs::PointSystem::Change(character, POINT_PARTY_HASTE_BONUS, iBonus - ecs::PointSystem::Get(character, POINT_PARTY_HASTE_BONUS));
						ecs::PointSystem::Compute(character);
					}
				}
				break;
			case PARTY_ROLE_DEFENDER:
				{
					int iBonus = (int) (5+30*k);
					if (ecs::PointSystem::Get(character, POINT_PARTY_DEFENDER_BONUS) != iBonus)
					{
						ecs::PointSystem::Change(character, POINT_PARTY_DEFENDER_BONUS, iBonus - ecs::PointSystem::Get(character, POINT_PARTY_DEFENDER_BONUS));
						ecs::PointSystem::Compute(character);
					}
				}
				break;
		}
	}

	void Update(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		LOG_TRACE("PARTY::Update");

		const entt::entity lEntity = GetLeader(party);


		if (lEntity == entt::null)
			return;

		int iNearMember = 0;
		bool bResendAll = false;

		for (auto& row : state->members)
		{
			const entt::entity member = row.second.member;

			row.second.bNear = false;

			if (!IsLinked(member))
				continue;

			if (ecs::SocialSystem::GetDungeon(lEntity) != entt::null)
				row.second.bNear = ecs::SocialSystem::GetDungeon(lEntity) == ecs::SocialSystem::GetDungeon(member);
			else
				row.second.bNear = (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(lEntity)-ecs::PlayerRuntime::GetX(member), ecs::PlayerRuntime::GetY(lEntity)-ecs::PlayerRuntime::GetY(member)) < PARTY_DEFAULT_RANGE);

			if (row.second.bNear)
			{
				++iNearMember;
				//LOG_INFO("NEAR {}", ecs::PlayerRuntime::GetName(member).data());
			}
		}

		if (iNearMember <= 1 && ecs::SocialSystem::GetDungeon(lEntity) == entt::null)
		{
			for (auto& row : state->members)
				row.second.bNear = false;

			iNearMember = 0;
		}

		if (iNearMember != state->nearMemberCount)
		{
			state->nearMemberCount = iNearMember;
			bResendAll = true;
		}

		state->leadership = SkillSystem::GetSkillLevel(lEntity, SKILL_LEADERSHIP);
		int iNewExpBonus = ComputePartyBonusExpPercent(party);
		state->attBonus = ComputePartyBonusAttackGrade(party);
		state->defBonus = ComputePartyBonusDefenseGrade(party);

		if (state->expBonus != iNewExpBonus)
		{
			bResendAll = true;
			state->expBonus = iNewExpBonus;
		}

		bool bLongTimeExpBonusChanged = false;

		if (!state->longTimeExpBonus && (get_dword_time() - state->startTime > PARTY_ENOUGH_MINUTE_FOR_EXP_BONUS * 60 * 1000 / 1))
		{
			bLongTimeExpBonusChanged = true;
			state->longTimeExpBonus = 5;
			bResendAll = true;
		}

		for (auto& row : state->members)
		{
			const entt::entity member = row.second.member;
			if (!IsLinked(member))
				continue;

#ifdef TEXTS_IMPROVEMENT
			if (bLongTimeExpBonusChanged && ecs::PlayerRuntime::GetDesc(member)) {
				ecs::ChatSystem::SendNew(member, CHAT_TYPE_INFO, 487, "");
			}
#endif

			bool bNear = row.second.bNear;

			ComputeRolePoint(party, member, row.second.bRole, bNear);

			if (bNear)
			{
				if (!bResendAll)
					SendPartyInfoOneToAll(party, member);
			}
		}

		// PARTY_ROLE_LIMIT_LEVEL_BUG_FIX
		state->maxRole[PARTY_ROLE_ATTACKER]	 = state->leadership >= 10 ? 1 : 0;
		state->maxRole[PARTY_ROLE_HASTE]	 = state->leadership >= 20 ? 1 : 0;
		state->maxRole[PARTY_ROLE_TANKER]	 = state->leadership >= 20 ? 1 : 0;
		state->maxRole[PARTY_ROLE_BUFFER]	 = state->leadership >= 25 ? 1 : 0;
		state->maxRole[PARTY_ROLE_SKILL_MASTER] = state->leadership >= 35 ? 1 : 0;
		state->maxRole[PARTY_ROLE_DEFENDER] 	 = state->leadership >= 40 ? 1 : 0;
		state->maxRole[PARTY_ROLE_ATTACKER]	+= state->leadership >= 40 ? 1 : 0;
		// END_OF_PARTY_ROLE_LIMIT_LEVEL_BUG_FIX

		// Party Heal Update
		if (!state->healReady)
		{
			if (!state->canUsePartyHeal && state->leadership >= 18)
				state->healTime = get_dword_time();

			state->canUsePartyHeal = state->leadership >= 18;

			uint32_t PartyHealCoolTime = (state->leadership >= 40) ? PARTY_HEAL_COOLTIME_SHORT * 60 * 1000 : PARTY_HEAL_COOLTIME_LONG * 60 * 1000;

			if (state->canUsePartyHeal)
			{
				if (get_dword_time() > state->healTime + PartyHealCoolTime)
				{
					state->healReady = true;

					// send heal ready
					if (0)
						if (lEntity != entt::null)
							ecs::ChatSystem::Send(lEntity, CHAT_TYPE_COMMAND, "PartyHealReady");
				}
			}
		}

		if (bResendAll)
		{
			for (auto& row : state->members)
				if (IsLinked(row.second.member))
					SendPartyInfoOneToAll(party, row.second.member);
		}
	}

	void UpdateOnlineState(entt::entity party, uint32_t dwPID, const char* name)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		auto itMember = state->members.find(dwPID);

		if (itMember == state->members.end())
			return;

		ecs::PartyMember& r = itMember->second;

		TPacketGCPartyAdd p;

		p.header = HEADER_GC_PARTY_ADD;
		p.pid = dwPID;
		r.strName = name;
		strlcpy(p.name, name, sizeof(p.name));

		for (auto& row : state->members)
		{
			if (IsLinked(row.second.member) && ecs::PlayerRuntime::GetDesc(row.second.member))
				ecs::PlayerRuntime::GetDesc(row.second.member)->Packet(&p, sizeof(p));
		}
	}

	void UpdateOfflineState(entt::entity party, uint32_t dwPID)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		TPacketGCPartyAdd p;
		p.header = HEADER_GC_PARTY_ADD;
		p.pid = dwPID;
		memset(p.name, 0, CHARACTER_NAME_MAX_LEN+1);

		for (auto& row : state->members)
		{
			if (IsLinked(row.second.member) && ecs::PlayerRuntime::GetDesc(row.second.member))
				ecs::PlayerRuntime::GetDesc(row.second.member)->Packet(&p, sizeof(p));
		}
	}

	int GetFlag(entt::entity party, std::string_view name)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return 0;

		const std::string key(name);
		auto it = state->flags.find(key);

		if (it != state->flags.end())
		{
			//LOG_INFO("PARTY GetFlag {} {}", name.c_str(), it->second);
			return it->second;
		}

		//LOG_INFO("PARTY GetFlag {} 0", name.c_str());
		return 0;
	}

	void SetFlag(entt::entity party, std::string_view name, int value)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		const std::string key(name);
		auto it = state->flags.find(key);

		//LOG_INFO("PARTY SetFlag {} {}", name.c_str(), value);
		if (it == state->flags.end())
		{
			state->flags.insert(make_pair(key, value));
		}
		else if (it->second != value)
		{
			it->second = value;
		}
	}

	void SetDungeon(entt::entity party, entt::entity pDungeon)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		state->dungeon = pDungeon != entt::null && DungeonSystem::IsValid(pDungeon)
			? pDungeon
			: entt::null;
		state->flags.clear();
	}

	entt::entity GetDungeon(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state || state->dungeon == entt::null)
			return entt::null;

		return DungeonSystem::IsValid(state->dungeon) ? state->dungeon : entt::null;
	}


	bool IsPositionNearLeader(entt::entity party, entt::entity character)
	{
		const entt::entity chrLeader = GetLeader(party);
		if (chrLeader == entt::null)
			return false;

		if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(character) - ecs::PlayerRuntime::GetX(chrLeader), ecs::PlayerRuntime::GetY(character) - ecs::PlayerRuntime::GetY(chrLeader)) >= PARTY_DEFAULT_RANGE)
			return false;

		return true;
	}


	int GetExpBonusPercent(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return 0;

		if (static_cast<int>(state->nearMemberCount) <= 1)
			return 0;

		return state->expBonus + state->longTimeExpBonus;
	}

	bool IsNearLeader(entt::entity party, uint32_t pid)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return false;

		auto it = state->members.find(pid);

		if (it == state->members.end())
			return false;

		return it->second.bNear;
	}

	uint8_t CountMemberByVnum(entt::entity party, uint32_t dwVnum)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return 0;

		if (state->isPCParty)
			return 0;

		uint8_t bCount = 0;

		for (auto& row : state->members)
		{
			const entt::entity tch = row.second.member;
			if (!IsLinked(tch))
				continue;

			if (ecs::PlayerRuntime::IsPC(tch))
				continue;

			const TMobTable* mobTable = ecs::PlayerRuntime::GetMobTable(tch);
			if (mobTable && mobTable->dwVnum == dwVnum)
				++bCount;
		}

		return bCount;
	}

	void SendParameter(entt::entity party, entt::entity character)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		TPacketGCPartyParameter p;

		p.bHeader = HEADER_GC_PARTY_PARAMETER;
		p.bDistributeMode = state->expDistributionMode;

		LPDESC d = ecs::PlayerRuntime::GetDesc(character);

		if (d)
		{
			d->Packet(&p, sizeof(TPacketGCPartyParameter));
		}
	}

	void SendParameterToAll(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		if (!state->isPCParty)
			return;

		for (auto& row : state->members)
			if (IsLinked(row.second.member))
				SendParameter(party, row.second.member);
	}

	void SetParameter(entt::entity party, int iMode)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return;

		if (iMode >= PARTY_EXP_DISTRIBUTION_MAX_NUM)
		{
			LOG_ERROR("Invalid exp distribution mode {}", iMode);
			return;
		}

		state->expDistributionMode = iMode;
		SendParameterToAll(party);
	}

	int GetExpDistributionMode(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		return state ? state->expDistributionMode : PARTY_EXP_DISTRIBUTION_NON_PARITY;
	}

	uint8_t GetMemberMaxLevel(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return 0;

		uint8_t bMax = 0;

		auto it = state->members.begin();
		while (it != state->members.end())
		{
			if (!it->second.bLevel)
			{
				++it;
				continue;
			}

			if (!bMax)
				bMax = it->second.bLevel;
			else if (it->second.bLevel)
				bMax = MAX(bMax, it->second.bLevel);
			++it;
		}
		return bMax;
	}

	uint8_t GetMemberMinLevel(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return PLAYER_MAX_LEVEL_CONST;

		uint8_t bMin = PLAYER_MAX_LEVEL_CONST;

		auto it = state->members.begin();
		while (it != state->members.end())
		{
			if (!it->second.bLevel)
			{
				++it;
				continue;
			}

			if (!bMin)
				bMin = it->second.bLevel;
			else if (it->second.bLevel)
				bMin = MIN(bMin, it->second.bLevel);
			++it;
		}
		return bMin;
	}

	int ComputePartyBonusExpPercent(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return 0;

		if (static_cast<int>(state->nearMemberCount) <= 1)
			return 0;

		const entt::entity leaderEntity = GetLeader(party);


		int iBonusPartyExpFromItem = 0;

		// UPGRADE_PARTY_BONUS
		int iMemberCount=MIN(8, state->nearMemberCount);

		if (leaderEntity != entt::null && (ItemSystem::IsEquipUniqueItem(leaderEntity, UNIQUE_ITEM_PARTY_BONUS_EXP) || ItemSystem::IsEquipUniqueItem(leaderEntity, UNIQUE_ITEM_PARTY_BONUS_EXP_MALL)
			|| ItemSystem::IsEquipUniqueItem(leaderEntity, UNIQUE_ITEM_PARTY_BONUS_EXP_GIFT) || ItemSystem::IsEquipUniqueGroup(leaderEntity, 10010)))
		{
			iBonusPartyExpFromItem = 30;
		}

#ifdef ENABLE_NEW_USE_POTION
		if (leaderEntity != entt::null && ecs::PointSystem::Get(leaderEntity, POINT_PARTY_DROPEXP) > 0) {
			iBonusPartyExpFromItem += ecs::PointSystem::Get(leaderEntity, POINT_PARTY_DROPEXP);
		}
#endif

		return iBonusPartyExpFromItem + CHN_aiPartyBonusExpPercentByMemberCount[iMemberCount];
		// END_OF_UPGRADE_PARTY_BONUS
	}

	int GetPartyBonusExpPercent(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		return state ? state->expBonus : 0;
	}

	int GetPartyBonusAttackGrade(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		return state ? state->attBonus : 0;
	}

	int GetPartyBonusDefenseGrade(entt::entity party)
	{
		ecs::PartyState* state = Find(party);
		return state ? state->defBonus : 0;
	}

	int ComputePartyBonusAttackGrade(entt::entity party)
	{
		/*
		   if (GetNearMemberCount() <= 1)
		   return 0;

		   int leadership = SkillSystem::GetSkillLevel(GetLeader(), SKILL_LEADERSHIP);
		   int n = GetNearMemberCount();

		   if (n >= 3 && leadership >= 10)
		   return 2;

		   if (n >= 2 && leadership >= 4)
		   return 1;
		 */
		return 0;
	}

	int ComputePartyBonusDefenseGrade(entt::entity party)
	{
		/*
		   if (GetNearMemberCount() <= 1)
		   return 0;

		   int leadership = SkillSystem::GetSkillLevel(GetLeader(), SKILL_LEADERSHIP);
		   int n = GetNearMemberCount();

		   if (n >= 5 && leadership >= 24)
		   return 2;

		   if (n >= 4 && leadership >= 16)
		   return 1;
		 */
		return 0;
	}

	bool IsPartyInDungeon(entt::entity party, int mapIndex)
	{
		ecs::PartyState* state = Find(party);
		if (!state)
			return false;

		for (auto& row : state->members)
		{
			const entt::entity member = row.second.member;

			if (!IsLinked(member))
			{
				continue;
			}

			const entt::entity d = ecs::SocialSystem::GetDungeon(member);

			if(d == entt::null)
			{
				LOG_TRACE("not in dungeon");
				continue;
			}

			if( mapIndex == (DungeonSystem::GetMapIndex(d))/10000 )
			{
				return true;
			}

		}
		return false;
	}
}
