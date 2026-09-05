#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/MovementSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "ecs/systems/NetworkSyncSystem.hpp"
#include "ecs/systems/ItemSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/Registry.hpp"
#include "utils.h"
#include "char_interface.hpp"
#include "party.h"
#include "char_manager.h"
#include "config.h"
#include "p2p.h"
#include "desc_client.h"
#include "dungeon.h"
#include "unique_item.h"
#include "ecs/CharacterAccessors.hpp"

#ifdef ENABLE_DICE_SYSTEM
void FPartyDropDiceRoll::Process(const LPCHARACTER mobVictim)
{
	const entt::entity itemOwner = m_itemOwner ? m_itemOwner->GetEntityHandle() : entt::null;
	if (!m_itemOwner || !ItemSystem::IsValidItem(m_itemDrop))
		return;

	LPPARTY party = m_itemOwner->GetParty();
	const bool rollForParty =
		(!mobVictim || (mobVictim->GetMobRank() >= MOB_RANK_BOSS && mobVictim->GetMobRank() <= MOB_RANK_KING)) &&
		party && party->GetNearMemberCount() > 1;

	if (rollForParty)
	{
#ifdef TEXTS_IMPROVEMENT
		party->ChatPacketToAllMemberNew(CHAT_TYPE_DICE_INFO, 542, "%s", ItemSystem::GetItemName(m_itemDrop));
#endif
		party->ForEachNearMember(*this);
		if (!m_itemOwner)
			return;

		ItemSystem::SetGroundOwnership(m_itemDrop, itemOwner);
#ifdef TEXTS_IMPROVEMENT
		party->ChatPacketToAllMemberNew(CHAT_TYPE_DICE_INFO, 903, "%s#%s",
			ecs::PlayerRuntime::GetName(itemOwner).data(), ItemSystem::GetItemName(m_itemDrop));
#endif
		return;
	}

	ItemSystem::SetGroundOwnership(m_itemDrop, itemOwner);
}
#endif
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
	TPCPartySet::iterator it = m_set_pkPCParty.begin();

	while (it != m_set_pkPCParty.end())
	{
		DeleteParty(*it);
		it = m_set_pkPCParty.begin();
	}
}

bool CPartyManager::SetParty(LPCHARACTER ch)	// PC�� ����ؾ� �Ѵ�!!
{
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;
	TPartyMap::iterator it = m_map_pkParty.find((ecs::PlayerRuntime::GetPlayerID(chEntity)));

	if (it == m_map_pkParty.end())
		return false;

	LPPARTY pParty = it->second;
	pParty->Link(chEntity);
	return true;
}

void CPartyManager::P2PLogin(uint32_t pid, const char* name)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it == m_map_pkParty.end())
		return;

	it->second->UpdateOnlineState(pid, name);
}
void CPartyManager::P2PLogout(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it == m_map_pkParty.end())
		return;

	it->second->UpdateOfflineState(pid);
}

void CPartyManager::P2PJoinParty(uint32_t leader, uint32_t pid, uint8_t role)
{
	TPartyMap::iterator it = m_map_pkParty.find(leader);

	if (it != m_map_pkParty.end())
	{
		it->second->P2PJoin(pid);

		if (role >= PARTY_ROLE_MAX_NUM)
			role = PARTY_ROLE_NORMAL;

		it->second->SetRole(pid, role, true);
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
		it->second->P2PQuit(pid);
	}
	else
	{
		LOG_ERROR("No such party with member [{}]", pid);
	}
}

LPPARTY CPartyManager::P2PCreateParty(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);
	if (it != m_map_pkParty.end())
		return it->second;

	LPPARTY pParty = M2_NEW CParty;

	m_set_pkPCParty.insert(pParty);

	SetPartyMember(pid, pParty);
	pParty->SetPCParty(true);
	pParty->P2PJoin(pid);

	return pParty;
}

void CPartyManager::P2PDeleteParty(uint32_t pid)
{
	TPartyMap::iterator it = m_map_pkParty.find(pid);

	if (it != m_map_pkParty.end())
	{
		m_set_pkPCParty.erase(it->second);
		M2_DELETE(it->second);
	}
	else
		LOG_ERROR("PARTY P2PDeleteParty Cannot find party [{}]", pid);
}

LPPARTY CPartyManager::CreateParty(entt::entity leader)
{
	LPCHARACTER pLeader = ecs::LegacyCharOf(leader);
	if (ecs::SocialSystem::GetParty(leader))
		return ecs::SocialSystem::GetParty(leader);

	LPPARTY pParty = M2_NEW CParty;

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
		pParty->SetPCParty(true);
		pParty->Join((ecs::PlayerRuntime::GetPlayerID(leader)));

		m_set_pkPCParty.insert(pParty);
	}
	else
	{
		pParty->SetPCParty(false);
		pParty->Join(pLeader->GetLegacyVID());
	}

	pParty->Link(leader);
	return (pParty);
}

void CPartyManager::DeleteParty(LPPARTY pParty)
{
	//TPacketGGParty p;
	//p.header = HEADER_GG_PARTY;
	//p.subheader = PARTY_SUBHEADER_GG_DESTROY;
	//p.pid = pParty->GetLeaderPID();
	//P2P_MANAGER::instance().Send(&p, sizeof(p));
	TPacketPartyDelete p;
	p.dwLeaderPID = pParty->GetLeaderPID();

	db_clientdesc->DBPacket(HEADER_GD_PARTY_DELETE, 0, &p, sizeof(TPacketPartyDelete));

	m_set_pkPCParty.erase(pParty);
	M2_DELETE(pParty);
}

void CPartyManager::SetPartyMember(uint32_t dwPID, LPPARTY pParty)
{
	TPartyMap::iterator it = m_map_pkParty.find(dwPID);

	if (pParty == nullptr)
	{
		if (it != m_map_pkParty.end())
			m_map_pkParty.erase(it);
	}
	else
	{
		if (it != m_map_pkParty.end())
		{
			if (it->second != pParty)
			{
				it->second->Quit(dwPID);
				it->second = pParty;
			}
		}
		else
			m_map_pkParty.insert(TPartyMap::value_type(dwPID, pParty));
	}
}

EVENTINFO(party_update_event_info)
{
	uint32_t pid;

	party_update_event_info()
	: pid( 0 )
	{
	}
};

/////////////////////////////////////////////////////////////////////////////
//
// CParty begin!
//
/////////////////////////////////////////////////////////////////////////////
EVENTFUNC(party_update_event)
{
	party_update_event_info* info = dynamic_cast<party_update_event_info*>( event->info );

	if ( info == nullptr)
	{
		LOG_ERROR("party_update_event> <Factor> Null pointer");
		return 0;
	}

	uint32_t pid = info->pid;
	const entt::entity leader = CHARACTER_MANAGER::instance().FindEntityByPID(pid);

	if (leader != entt::null && ecs::PlayerRuntime::GetDesc(leader))
	{
		LPPARTY pParty = ecs::SocialSystem::GetParty(leader);

		if (pParty)
			pParty->Update();
	}

	return PASSES_PER_SEC(3);
}

CParty::CParty()
{
	Initialize();
}

CParty::~CParty()
{
	Destroy();
}

void CParty::Initialize()
{
	LOG_TRACE("Party::Initialize");

	m_iExpDistributionMode = PARTY_EXP_DISTRIBUTION_NON_PARITY;
	m_pkChrExpCentralize = nullptr;

	m_dwLeaderPID = 0;

	m_eventUpdate = nullptr;

	memset(&m_anRoleCount, 0, sizeof(m_anRoleCount));
	memset(&m_anMaxRole, 0, sizeof(m_anMaxRole));
	m_anMaxRole[PARTY_ROLE_LEADER] = 1;
	m_anMaxRole[PARTY_ROLE_NORMAL] = 32;

	m_dwPartyStartTime = get_dword_time();
	m_iLongTimeExpBonus = 0;

	m_dwPartyHealTime = get_dword_time();
	m_bPartyHealReady = false;
	m_bCanUsePartyHeal = false;

	m_iLeadership = 0;
	m_iExpBonus = 0;
	m_iAttBonus = 0;
	m_iDefBonus = 0;

	m_itNextOwner = m_memberMap.begin();

	m_iCountNearPartyMember = 0;

	m_pkChrLeader = nullptr;
	m_bPCParty = false;
	m_pkDungeon = nullptr;
	m_pkDungeon_for_Only_party = nullptr;
}


void CParty::Destroy()
{
	LOG_TRACE("Party::Destroy");

	// PC�� ���� ��Ƽ�� ��Ƽ�Ŵ����� �ʿ��� PID�� �����ؾ� �Ѵ�.
	if (m_bPCParty)
	{
		for (TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
			CPartyManager::instance().SetPartyMember(it->first, nullptr);
	}

	event_cancel(&m_eventUpdate);

	RemoveBonus();

	TMemberMap::iterator it = m_memberMap.begin();

	uint32_t dwTime = get_dword_time();

	while (it != m_memberMap.end())
	{
		TMember & rMember = it->second;
		++it;

		if (rMember.pCharacter)
		{
			if (ecs::PlayerRuntime::GetDesc(((rMember.pCharacter) ? (rMember.pCharacter)->GetEntityHandle() : entt::null)))
			{
				TPacketGCPartyRemove p;
				p.header = HEADER_GC_PARTY_REMOVE;
				p.pid = ecs::PlayerRuntime::GetPlayerID(((rMember.pCharacter) ? (rMember.pCharacter)->GetEntityHandle() : entt::null));
				ecs::PlayerRuntime::GetDesc(((rMember.pCharacter) ? (rMember.pCharacter)->GetEntityHandle() : entt::null))->Packet(&p, sizeof(p));
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(((rMember.pCharacter) ? (rMember.pCharacter)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 213, "");
#endif
			}
			else
			{
				// NPC�� ��� ���� �ð� �� ���� ���� �ƴ� �� ������� �ϴ� �̺�Ʈ�� ���۽�Ų��.
				rMember.pCharacter->SetLastAttacked(dwTime);
				rMember.pCharacter->StartDestroyWhenIdleEvent();
			}

			rMember.pCharacter->SetParty(nullptr);
		}
	}

	m_memberMap.clear();
	m_itNextOwner = m_memberMap.begin();

	if (m_pkDungeon_for_Only_party != nullptr)
	{
		m_pkDungeon_for_Only_party->SetPartyNull();
		m_pkDungeon_for_Only_party = nullptr;
	}
}

#ifdef TEXTS_IMPROVEMENT
void CParty::ChatPacketToAllMemberNew(uint8_t type, uint32_t idx, const char * format, ...) {
	char chatbuf[256];
	va_list args;
	va_start(args, format);
	vsnprintf(chatbuf, sizeof(chatbuf), format, args);
	va_end(args);

	TMemberMap::iterator it;
	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it) {
		TMember & rMember = it->second;
		if (rMember.pCharacter) {
			ecs::ChatSystem::SendNew(((rMember.pCharacter) ? (rMember.pCharacter)->GetEntityHandle() : entt::null), type, idx, "%s", chatbuf);
		}
	}
}
#endif

uint32_t CParty::GetLeaderPID()
{
	return m_dwLeaderPID;
}

uint32_t CParty::GetMemberCount()
{
	return m_memberMap.size();
}

void CParty::P2PJoin(uint32_t dwPID)
{
	TMemberMap::iterator it = m_memberMap.find(dwPID);

	if (it == m_memberMap.end())
	{
		TMember Member;

		Member.pCharacter	= nullptr;
		Member.bNear		= false;

		if (m_memberMap.empty())
		{
			Member.bRole = PARTY_ROLE_LEADER;
			m_dwLeaderPID = dwPID;
		}
		else
			Member.bRole = PARTY_ROLE_NORMAL;

		if (m_bPCParty)
		{
			auto* ch = CHARACTER_MANAGER::instance().FindByPID(dwPID);

			if (ch)
			{
				LOG_INFO("PARTY: Join {} pid {} leader {}", ecs::PlayerRuntime::GetName(((ch) ? (ch)->GetEntityHandle() : entt::null)).data(), dwPID, m_dwLeaderPID);
				Member.strName = ecs::PlayerRuntime::GetName(((ch) ? (ch)->GetEntityHandle() : entt::null)).data();

				if (Member.bRole == PARTY_ROLE_LEADER)
					m_iLeadership = ch->GetLeadershipSkillLevel();
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

		LOG_TRACE("PARTY[{}] MemberCountChange {} -> {}", GetLeaderPID(), GetMemberCount(), GetMemberCount()+1);

		m_memberMap.insert(TMemberMap::value_type(dwPID, Member));

		if (m_memberMap.size() == 1)
			m_itNextOwner = m_memberMap.begin();

		if (m_bPCParty)
		{
			CPartyManager::instance().SetPartyMember(dwPID, this);
			SendPartyJoinOneToAll(dwPID);

			auto* ch = CHARACTER_MANAGER::instance().FindByPID(dwPID);

			if (ch)
				SendParameter(((ch) ? (ch)->GetEntityHandle() : entt::null));
		}
	}

	if (m_pkDungeon)
	{
		m_pkDungeon->QuitParty(this);
	}
}

void CParty::Join(uint32_t dwPID)
{
	P2PJoin(dwPID);

	if (m_bPCParty)
	{
		TPacketPartyAdd p;
		p.dwLeaderPID = GetLeaderPID();
		p.dwPID = dwPID;
		p.bState = PARTY_ROLE_NORMAL; // #0000790: [M2EU] CZ ũ���� ����: �ʱ�ȭ �߿�!
		db_clientdesc->DBPacket(HEADER_GD_PARTY_ADD, 0, &p, sizeof(p));
	}
}

void CParty::P2PQuit(uint32_t dwPID)
{
	TMemberMap::iterator it = m_memberMap.find(dwPID);

	if (it == m_memberMap.end())
		return;

	if (m_bPCParty)
		SendPartyRemoveOneToAll(dwPID);

	if (it == m_itNextOwner)
		IncreaseOwnership();

	if (m_bPCParty)
		RemoveBonusForOne(dwPID);

	auto* ch = it->second.pCharacter;
	uint8_t bRole = it->second.bRole;

	m_memberMap.erase(it);

	LOG_TRACE("PARTY[{}] MemberCountChange {} -> {}", GetLeaderPID(), GetMemberCount(), GetMemberCount() - 1);

	if (bRole < PARTY_ROLE_MAX_NUM)
	{
		--m_anRoleCount[bRole];
	}
	else
	{
		LOG_ERROR("ROLE_COUNT_QUIT_ERROR: INDEX({}) > MAX({})", bRole, PARTY_ROLE_MAX_NUM);
	}

	if (ch)
	{
		ch->SetParty(nullptr);
		ComputeRolePoint(((ch) ? (ch)->GetEntityHandle() : entt::null), bRole, false);
	}

	if (m_bPCParty)
		CPartyManager::instance().SetPartyMember(dwPID, nullptr);

	// ������ ������ ��Ƽ�� �ػ�Ǿ�� �Ѵ�.
	if (bRole == PARTY_ROLE_LEADER)
		CPartyManager::instance().DeleteParty(this);

	// �� �Ʒ��� �ڵ带 �߰����� �� ��!!! �� DeleteParty �ϸ� this�� ����.
}

void CParty::Quit(uint32_t dwPID)
{
	// Always PC
	P2PQuit(dwPID);

	if (m_bPCParty && dwPID != GetLeaderPID())
	{
		//TPacketGGParty p;
		//p.header = HEADER_GG_PARTY;
		//p.subheader = PARTY_SUBHEADER_GG_QUIT;
		//p.pid = dwPID;
		//p.leaderpid = GetLeaderPID();
		//P2P_MANAGER::instance().Send(&p, sizeof(p));
		TPacketPartyRemove p;
		p.dwPID = dwPID;
		p.dwLeaderPID = GetLeaderPID();
		db_clientdesc->DBPacket(HEADER_GD_PARTY_REMOVE, 0, &p, sizeof(p));
	}
}

void CParty::Link(entt::entity character)
{
	LPCHARACTER pkChr = ecs::LegacyCharOf(character);
	TMemberMap::iterator it;

	if (ecs::PlayerRuntime::IsPC(character))
		it = m_memberMap.find(ecs::PlayerRuntime::GetPlayerID(character));
	else
		it = m_memberMap.find(pkChr->GetLegacyVID());

	if (it == m_memberMap.end())
	{
		LOG_ERROR("{} is not member of this party", ecs::PlayerRuntime::GetName(character).data());
		return;
	}

	// �÷��̾� ��Ƽ�� ��� ������Ʈ �̺�Ʈ ����
	if (m_bPCParty && !m_eventUpdate)
	{
		party_update_event_info* info = AllocEventInfo<party_update_event_info>();
		info->pid = m_dwLeaderPID;
		m_eventUpdate = event_create(party_update_event, info, PASSES_PER_SEC(3));
	}

	if (it->second.bRole == PARTY_ROLE_LEADER)
		m_pkChrLeader = pkChr;

	LOG_TRACE("PARTY[{}] {} linked to party", GetLeaderPID(), ecs::PlayerRuntime::GetName(character).data());

	it->second.pCharacter = pkChr;
	pkChr->SetParty(this);

	if (ecs::PlayerRuntime::IsPC(character))
	{
		if (it->second.strName.empty())
		{
			it->second.strName = ecs::PlayerRuntime::GetName(character).data();
		}

		SendPartyJoinOneToAll((ecs::PlayerRuntime::GetPlayerID(character)));

		SendPartyJoinAllToOne(character);
		SendPartyLinkOneToAll(character);
		SendPartyLinkAllToOne(character);
		SendPartyInfoAllToOne(character);
		SendPartyInfoOneToAll(character);

		SendParameter(character);

		//LOG_INFO("PARTY-DUNGEON connect {} {}", static_cast<const void*>(this), static_cast<const void*>(GetDungeon()));
		if (GetDungeon() && GetDungeon()->GetMapIndex() == ecs::PlayerRuntime::GetMapIndex(character))
		{
			pkChr->SetDungeon(GetDungeon());
		}

		RequestSetMemberLevel((ecs::PlayerRuntime::GetPlayerID(character)), (ecs::PointSystem::GetLevel(character)));

	}
}

void CParty::RequestSetMemberLevel(uint32_t pid, uint8_t level)
{
	TPacketPartySetMemberLevel p;
	p.dwLeaderPID = GetLeaderPID();
	p.dwPID = pid;
	p.bLevel = level;
	db_clientdesc->DBPacket(HEADER_GD_PARTY_SET_MEMBER_LEVEL, 0, &p, sizeof(TPacketPartySetMemberLevel));
}

void CParty::P2PSetMemberLevel(uint32_t pid, uint8_t level)
{
	if (!m_bPCParty)
		return;

	TMemberMap::iterator it;

	LOG_TRACE("PARTY P2PSetMemberLevel leader {} pid {} level {}", GetLeaderPID(), pid, static_cast<int>(level));

	it = m_memberMap.find(pid);
	if (it != m_memberMap.end())
	{
		it->second.bLevel = level;
	}
}

namespace
{
	struct FExitDungeon
	{
		void operator()(LPCHARACTER ch)
		{
			ecs::MovementSystem::ExitToSavedLocation(((ch) ? (ch)->GetEntityHandle() : entt::null));
		}
	};
}

void CParty::Unlink(entt::entity character)
{
	LPCHARACTER pkChr = ecs::LegacyCharOf(character);
	TMemberMap::iterator it;

	if (ecs::PlayerRuntime::IsPC(character))
		it = m_memberMap.find(ecs::PlayerRuntime::GetPlayerID(character));
	else
		it = m_memberMap.find(pkChr->GetLegacyVID());

	if (it == m_memberMap.end())
	{
		LOG_ERROR("{} is not member of this party", ecs::PlayerRuntime::GetName(character).data());
		return;
	}

	if (ecs::PlayerRuntime::IsPC(character))
	{
		SendPartyUnlinkOneToAll(character);
		//SendPartyUnlinkAllToOne(pkChr); // ����� ���̹Ƿ� ���� Unlink ��Ŷ�� ���� �ʿ� ����.

		if (it->second.bRole == PARTY_ROLE_LEADER)
		{
			RemoveBonus();

			if (it->second.pCharacter->GetDungeon())
			{
				// TODO: ������ ������ �������� ������
				FExitDungeon f;
				ForEachNearMember(f);
			}
		}
	}

	if (it->second.bRole == PARTY_ROLE_LEADER) {
		m_pkChrLeader = nullptr;
	}

	it->second.pCharacter = nullptr;
	pkChr->SetParty(nullptr);
}

void CParty::SendPartyRemoveOneToAll(uint32_t pid)
{
	TMemberMap::iterator it;

	TPacketGCPartyRemove p;
	p.header = HEADER_GC_PARTY_REMOVE;
	p.pid = pid;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)))
			ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null))->Packet(&p, sizeof(p));
	}
}

void CParty::SendPartyJoinOneToAll(uint32_t pid)
{
	const TMember& r = m_memberMap[pid];

	TPacketGCPartyAdd p;

	p.header = HEADER_GC_PARTY_ADD;
	p.pid = pid;
	strlcpy(p.name, r.strName.c_str(), sizeof(p.name));

	for (TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)))
			ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null))->Packet(&p, sizeof(p));
	}
}

void CParty::SendPartyJoinAllToOne(entt::entity character)
{
	if (!ecs::PlayerRuntime::GetDesc(character))
		return;

	TPacketGCPartyAdd p;

	p.header = HEADER_GC_PARTY_ADD;
	p.name[CHARACTER_NAME_MAX_LEN] = '\0';

	for (TMemberMap::iterator it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		p.pid = it->first;
		strlcpy(p.name, it->second.strName.c_str(), sizeof(p.name));
		ecs::PlayerRuntime::GetDesc(character)->Packet(&p, sizeof(p));
	}
}

void CParty::SendPartyUnlinkOneToAll(entt::entity character)
{
	if (!ecs::PlayerRuntime::GetDesc(character))
		return;

	TMemberMap::iterator it;

	TPacketGCPartyLink p;
	p.header = HEADER_GC_PARTY_UNLINK;
	p.pid = (ecs::PlayerRuntime::GetPlayerID(character));
	p.vid = ecs::PlayerRuntime::GetPacketVID(character);

	for (it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)))
		{
			ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null))->Packet(&p, sizeof(p));
		}
	}
}

void CParty::SendPartyLinkOneToAll(entt::entity character)
{
	if (!ecs::PlayerRuntime::GetDesc(character))
		return;

	TMemberMap::iterator it;

	TPacketGCPartyLink p;
	p.header = HEADER_GC_PARTY_LINK;
	p.vid = ecs::PlayerRuntime::GetPacketVID(character);
	p.pid = (ecs::PlayerRuntime::GetPlayerID(character));

	for (it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)))
		{
			ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null))->Packet(&p, sizeof(p));
		}
	}
}

void CParty::SendPartyLinkAllToOne(entt::entity character)
{
	if (!ecs::PlayerRuntime::GetDesc(character))
		return;

	TMemberMap::iterator it;

	TPacketGCPartyLink p;
	p.header = HEADER_GC_PARTY_LINK;

	for (it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter)
		{
			p.vid = ecs::PlayerRuntime::GetPacketVID(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null));
			p.pid = (ecs::PlayerRuntime::GetPlayerID(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)));
			ecs::PlayerRuntime::GetDesc(character)->Packet(&p, sizeof(p));
		}
	}
}

void CParty::SendPartyInfoOneToAll(uint32_t pid)
{
	TMemberMap::iterator it = m_memberMap.find(pid);

	if (it == m_memberMap.end())
		return;

	if (it->second.pCharacter)
	{
		SendPartyInfoOneToAll(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null));
		return;
	}

	// Data Building
	TPacketGCPartyUpdate p;
	memset(&p, 0, sizeof(p));
	p.header = HEADER_GC_PARTY_UPDATE;
	p.pid = pid;
	p.percent_hp = 255;
	p.role = it->second.bRole;

	for (it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		if ((it->second.pCharacter) && (ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null))))
		{
			//LOG_TRACE("PARTY send info {}[{}] to {}[{}]", ecs::PlayerRuntime::GetName(((ch) ? (ch)->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetPacketVID(((ch) ? (ch)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetName(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetPacketVID(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)));
			ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null))->Packet(&p, sizeof(p));
		}
	}
}

void CParty::SendPartyInfoOneToAll(entt::entity character)
{
	if (!ecs::PlayerRuntime::GetDesc(character))
		return;

	TMemberMap::iterator it;

	// Data Building
	TPacketGCPartyUpdate p;
	NetworkSyncSystem::BuildPartyUpdatePacket(g_registry, character, p);

	for (it = m_memberMap.begin();it!= m_memberMap.end(); ++it)
	{
		if ((it->second.pCharacter) && (ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null))))
		{
			LOG_TRACE("PARTY send info {}[{}] to {}[{}]", ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetPacketVID(character), ecs::PlayerRuntime::GetName(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetPacketVID(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)));
			ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null))->Packet(&p, sizeof(p));
		}
	}
}

void CParty::SendPartyInfoAllToOne(entt::entity character)
{
	TMemberMap::iterator it;

	TPacketGCPartyUpdate p;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (!it->second.pCharacter)
		{
			uint32_t pid = it->first;
			memset(&p, 0, sizeof(p));
			p.header = HEADER_GC_PARTY_UPDATE;
			p.pid = pid;
			p.percent_hp = 255;
			p.role = it->second.bRole;
			ecs::PlayerRuntime::GetDesc(character)->Packet(&p, sizeof(p));
			continue;
		}

		NetworkSyncSystem::BuildPartyUpdatePacket(g_registry, ((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null), p);
		LOG_TRACE("PARTY send info {}[{}] to {}[{}]", ecs::PlayerRuntime::GetName(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)).data(), ecs::PlayerRuntime::GetPacketVID(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetName(character).data(), ecs::PlayerRuntime::GetPacketVID(character));
		ecs::PlayerRuntime::GetDesc(character)->Packet(&p, sizeof(p));
	}
}

void CParty::SendMessage(entt::entity character, uint8_t bMsg, uint32_t dwArg1, uint32_t dwArg2)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
	if (ecs::SocialSystem::GetParty(character) != this)
	{
		LOG_ERROR("{} is not member of this party {}", ecs::PlayerRuntime::GetName(character).data(), static_cast<const void*>(this));
		return;
	}

	switch (bMsg)
	{
		case PM_ATTACK:
			break;

		case PM_RETURN:
			{
				TMemberMap::iterator it = m_memberMap.begin();

				while (it != m_memberMap.end())
				{
					TMember & rMember = it->second;
					++it;

					auto* pkChr = static_cast<LPCHARACTER>(nullptr);

					if ((pkChr = rMember.pCharacter) && ch != pkChr)
					{
						uint32_t x = dwArg1 + number(-500, 500);
						uint32_t y = dwArg2 + number(-500, 500);

						pkChr->SetVictim(entt::null);
						pkChr->SetRotationToXY(x, y);

						if (ecs::MovementSystem::Goto(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null), x, y))
						{
							auto* victim = pkChr->GetVictim();
							LOG_TRACE("{} {} RETURN victim {}", ecs::PlayerRuntime::GetName(((pkChr) ? (pkChr)->GetEntityHandle() : entt::null)).data(), static_cast<const void*>(get_pointer(pkChr)), static_cast<const void*>(get_pointer(victim)));
							pkChr->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
						}
					}
				}
			}
			break;

		case PM_ATTACKED_BY:	// ���� �޾���, �������� ������ ��û
			{
				// ������ ���� ��
				auto* pkChrVictim = ch->GetVictim();

				if (!pkChrVictim)
					return;

				TMemberMap::iterator it = m_memberMap.begin();

				while (it != m_memberMap.end())
				{
					TMember & rMember = it->second;
					++it;

					auto* pkChr = static_cast<LPCHARACTER>(nullptr);

					if ((pkChr = rMember.pCharacter) && ch != pkChr)
					{
						if (pkChr->CanBeginFight())
							pkChr->BeginFight((pkChrVictim ? pkChrVictim->GetEntityHandle() : entt::null));
					}
				}
			}
			break;

		case PM_AGGRO_INCREASE:
			{
				auto* victim = CHARACTER_MANAGER::instance().Find(dwArg2);

				if (!victim)
					return;

				TMemberMap::iterator it = m_memberMap.begin();

				while (it != m_memberMap.end())
				{
					TMember & rMember = it->second;
					++it;

					auto* pkChr = static_cast<LPCHARACTER>(nullptr);

					if ((pkChr = rMember.pCharacter) && ch != pkChr)
					{
						pkChr->UpdateAggrPoint((victim ? victim->GetEntityHandle() : entt::null), DAMAGE_TYPE_SPECIAL, dwArg1);
					}
				}
			}
			break;
	}
}

LPCHARACTER CParty::GetLeaderCharacter()
{
	return m_memberMap[GetLeaderPID()].pCharacter;
}

bool CParty::SetRole(uint32_t dwPID, uint8_t bRole, bool bSet)
{
	TMemberMap::iterator it = m_memberMap.find(dwPID);

	if (it == m_memberMap.end())
	{
		return false;
	}

	auto* ch = it->second.pCharacter;
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;


	if (bSet)
	{
		if (m_anRoleCount[bRole] >= m_anMaxRole[bRole])
			return false;

		if (it->second.bRole != PARTY_ROLE_NORMAL)
			return false;

		it->second.bRole = bRole;

		if (ch && GetLeader())
			ComputeRolePoint(chEntity, bRole, true);

		if (bRole < PARTY_ROLE_MAX_NUM)
		{
			++m_anRoleCount[bRole];
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

		if (ch && GetLeader())
			ComputeRolePoint(chEntity, PARTY_ROLE_NORMAL, false);

		if (bRole < PARTY_ROLE_MAX_NUM)
		{
			--m_anRoleCount[bRole];
		}
		else
		{
			LOG_ERROR("ROLE_COUNT_DEC_ERROR: INDEX({}) > MAX({})", static_cast<int>(bRole), PARTY_ROLE_MAX_NUM);
		}
	}

	SendPartyInfoOneToAll(dwPID);
	return true;
}

uint8_t CParty::GetRole(uint32_t pid)
{
	TMemberMap::iterator it = m_memberMap.find(pid);

	if (it == m_memberMap.end())
		return PARTY_ROLE_NORMAL;
	else
		return it->second.bRole;
}

bool CParty::IsRole(uint32_t pid, uint8_t bRole)
{
	TMemberMap::iterator it = m_memberMap.find(pid);

	if (it == m_memberMap.end())
		return false;

	return it->second.bRole == bRole;
}

void CParty::RemoveBonus()
{
	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		auto* ch = it->second.pCharacter;

		if (ch)
		{
			ComputeRolePoint(((ch) ? (ch)->GetEntityHandle() : entt::null), it->second.bRole, false);
		}

		it->second.bNear = false;
	}
}

void CParty::RemoveBonusForOne(uint32_t pid)
{
	TMemberMap::iterator it = m_memberMap.find(pid);

	if (it == m_memberMap.end())
		return;

	auto* ch = it->second.pCharacter;

	if (ch)
		ComputeRolePoint(((ch) ? (ch)->GetEntityHandle() : entt::null), it->second.bRole, false);
}

void CParty::HealParty()
{
	// XXX DELETEME Ŭ���̾�Ʈ �Ϸ�ɶ�����
	{
		return;
	}
	if (!m_bPartyHealReady)
		return;

	TMemberMap::iterator it;
	auto* l = GetLeaderCharacter();
	const entt::entity lEntity = l ? l->GetEntityHandle() : entt::null;


	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (!it->second.pCharacter)
			continue;

		auto* ch = it->second.pCharacter;
		const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;


		if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(lEntity)-ecs::PlayerRuntime::GetX(chEntity), ecs::PlayerRuntime::GetY(lEntity)-ecs::PlayerRuntime::GetY(chEntity)) < PARTY_DEFAULT_RANGE)
		{
			ecs::PointSystem::Change(chEntity, POINT_HP, ecs::PointSystem::GetMaxHP(chEntity)-ch->GetHP());
			ecs::PointSystem::Change(chEntity, POINT_SP, ecs::PointSystem::GetMaxSP(chEntity)-ch->GetSP());
		}
	}

	m_bPartyHealReady = false;
	m_dwPartyHealTime = get_dword_time();
}

void CParty::SummonToLeader(uint32_t pid)
{
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
	auto* l = GetLeaderCharacter();
	const entt::entity lEntity = l ? l->GetEntityHandle() : entt::null;


	if (m_memberMap.find(pid) == m_memberMap.end())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(lEntity, CHAT_TYPE_INFO, 209, "");
#endif
		return;
	}

	auto* ch = m_memberMap[pid].pCharacter;
	const entt::entity chEntity = ch ? ch->GetEntityHandle() : entt::null;


	if (!ch)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(lEntity, CHAT_TYPE_INFO, 209, "");
#endif
		return;
	}

	if (!ch->CanSummon(m_iLeadership))
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

void CParty::IncreaseOwnership()
{
	if (m_memberMap.empty())
	{
		m_itNextOwner = m_memberMap.begin();
		return;
	}

	if (m_itNextOwner == m_memberMap.end())
		m_itNextOwner = m_memberMap.begin();
	else
	{
		m_itNextOwner++;

		if (m_itNextOwner == m_memberMap.end())
			m_itNextOwner = m_memberMap.begin();
	}
}

LPCHARACTER CParty::GetNextOwnership(LPCHARACTER ch, int32_t x, int32_t y)
{
	if (m_itNextOwner == m_memberMap.end())
		return ch;

	int size = m_memberMap.size();

	while (size-- > 0)
	{
		auto* pkMember = m_itNextOwner->second.pCharacter;
		const entt::entity member = pkMember ? pkMember->GetEntityHandle() : entt::null;


		if (pkMember && DISTANCE_APPROX(ecs::PlayerRuntime::GetX(member) - x, ecs::PlayerRuntime::GetY(member) - y) < 3000)
		{
			IncreaseOwnership();
			return pkMember;
		}

		IncreaseOwnership();
	}

	return ch;
}

void CParty::ComputeRolePoint(entt::entity character, uint8_t bRole, bool bAdd)
{
	LPCHARACTER ch = ecs::LegacyCharOf(character);
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
	float k = (float) ch->GetSkillPowerByLevel( MIN(SKILL_MAX_LEVEL, m_iLeadership ) )/ 100.0f;
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
					ch->ComputePoints();
				}
			}
			break;

		case PARTY_ROLE_TANKER:
			{
				int iBonus = (int) (50 + 1450 * k);

				if (ecs::PointSystem::Get(character, POINT_PARTY_TANKER_BONUS) != iBonus)
				{
					ecs::PointSystem::Change(character, POINT_PARTY_TANKER_BONUS, iBonus - ecs::PointSystem::Get(character, POINT_PARTY_TANKER_BONUS));
					ch->ComputePoints();
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
					ch->ComputePoints();
				}
			}
			break;
		case PARTY_ROLE_HASTE:
			{
				int iBonus = (int) (1+5*k);
				if (ecs::PointSystem::Get(character, POINT_PARTY_HASTE_BONUS) != iBonus)
				{
					ecs::PointSystem::Change(character, POINT_PARTY_HASTE_BONUS, iBonus - ecs::PointSystem::Get(character, POINT_PARTY_HASTE_BONUS));
					ch->ComputePoints();
				}
			}
			break;
		case PARTY_ROLE_DEFENDER:
			{
				int iBonus = (int) (5+30*k);
				if (ecs::PointSystem::Get(character, POINT_PARTY_DEFENDER_BONUS) != iBonus)
				{
					ecs::PointSystem::Change(character, POINT_PARTY_DEFENDER_BONUS, iBonus - ecs::PointSystem::Get(character, POINT_PARTY_DEFENDER_BONUS));
					ch->ComputePoints();
				}
			}
			break;
	}
}

void CParty::Update()
{
	LOG_TRACE("PARTY::Update");

	auto* l = GetLeaderCharacter();
	const entt::entity lEntity = l ? l->GetEntityHandle() : entt::null;


	if (!l)
		return;

	TMemberMap::iterator it;

	int iNearMember = 0;
	bool bResendAll = false;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		auto* ch = it->second.pCharacter;

		it->second.bNear = false;

		if (!ch)
			continue;

		if (l->GetDungeon())
			it->second.bNear = l->GetDungeon() == ch->GetDungeon();
		else
			it->second.bNear = (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(lEntity)-ecs::PlayerRuntime::GetX(((ch) ? (ch)->GetEntityHandle() : entt::null)), ecs::PlayerRuntime::GetY(lEntity)-ecs::PlayerRuntime::GetY(((ch) ? (ch)->GetEntityHandle() : entt::null))) < PARTY_DEFAULT_RANGE);

		if (it->second.bNear)
		{
			++iNearMember;
			//LOG_INFO("NEAR {}", ecs::PlayerRuntime::GetName(((ch) ? (ch)->GetEntityHandle() : entt::null)).data());
		}
	}

	if (iNearMember <= 1 && !l->GetDungeon())
	{
		for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
			it->second.bNear = false;

		iNearMember = 0;
	}

	if (iNearMember != m_iCountNearPartyMember)
	{
		m_iCountNearPartyMember = iNearMember;
		bResendAll = true;
	}

	m_iLeadership = l->GetLeadershipSkillLevel();
	int iNewExpBonus = ComputePartyBonusExpPercent();
	m_iAttBonus = ComputePartyBonusAttackGrade();
	m_iDefBonus = ComputePartyBonusDefenseGrade();

	if (m_iExpBonus != iNewExpBonus)
	{
		bResendAll = true;
		m_iExpBonus = iNewExpBonus;
	}

	bool bLongTimeExpBonusChanged = false;

	// ��Ƽ �Ἲ �� ����� �ð��� ������ ����ġ ���ʽ��� �޴´�.
	if (!m_iLongTimeExpBonus && (get_dword_time() - m_dwPartyStartTime > PARTY_ENOUGH_MINUTE_FOR_EXP_BONUS * 60 * 1000 / 1))
	{
		bLongTimeExpBonusChanged = true;
		m_iLongTimeExpBonus = 5;
		bResendAll = true;
	}

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		auto* ch = it->second.pCharacter;
		if (!ch)
			continue;

#ifdef TEXTS_IMPROVEMENT
		if (bLongTimeExpBonusChanged && ecs::PlayerRuntime::GetDesc(((ch) ? (ch)->GetEntityHandle() : entt::null))) {
			ecs::ChatSystem::SendNew(((ch) ? (ch)->GetEntityHandle() : entt::null), CHAT_TYPE_INFO, 487, "");
		}
#endif

		bool bNear = it->second.bNear;

		ComputeRolePoint(((ch) ? (ch)->GetEntityHandle() : entt::null), it->second.bRole, bNear);

		if (bNear)
		{
			if (!bResendAll)
				SendPartyInfoOneToAll(((ch) ? (ch)->GetEntityHandle() : entt::null));
		}
	}

	// PARTY_ROLE_LIMIT_LEVEL_BUG_FIX
	m_anMaxRole[PARTY_ROLE_ATTACKER]	 = m_iLeadership >= 10 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_HASTE]	 = m_iLeadership >= 20 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_TANKER]	 = m_iLeadership >= 20 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_BUFFER]	 = m_iLeadership >= 25 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_SKILL_MASTER] = m_iLeadership >= 35 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_DEFENDER] 	 = m_iLeadership >= 40 ? 1 : 0;
	m_anMaxRole[PARTY_ROLE_ATTACKER]	+= m_iLeadership >= 40 ? 1 : 0;
	// END_OF_PARTY_ROLE_LIMIT_LEVEL_BUG_FIX

	// Party Heal Update
	if (!m_bPartyHealReady)
	{
		if (!m_bCanUsePartyHeal && m_iLeadership >= 18)
			m_dwPartyHealTime = get_dword_time();

		m_bCanUsePartyHeal = m_iLeadership >= 18; // ��ַ� 18 �̻��� ���� ����� �� ����.

		// ��ַ� 40�̻��� ��Ƽ �� ��Ÿ���� ����.
		uint32_t PartyHealCoolTime = (m_iLeadership >= 40) ? PARTY_HEAL_COOLTIME_SHORT * 60 * 1000 : PARTY_HEAL_COOLTIME_LONG * 60 * 1000;

		if (m_bCanUsePartyHeal)
		{
			if (get_dword_time() > m_dwPartyHealTime + PartyHealCoolTime)
			{
				m_bPartyHealReady = true;

				// send heal ready
				if (0) // XXX  DELETEME Ŭ���̾�Ʈ �Ϸ�ɶ�����
					if (GetLeaderCharacter())
						ecs::ChatSystem::Send(((GetLeaderCharacter()) ? (GetLeaderCharacter())->GetEntityHandle() : entt::null), CHAT_TYPE_COMMAND, "PartyHealReady");
			}
		}
	}

	if (bResendAll)
	{
		for (TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
			if (it->second.pCharacter)
				SendPartyInfoOneToAll(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null));
	}
}

void CParty::UpdateOnlineState(uint32_t dwPID, const char* name)
{
	TMember& r = m_memberMap[dwPID];

	TPacketGCPartyAdd p;

	p.header = HEADER_GC_PARTY_ADD;
	p.pid = dwPID;
	r.strName = name;
	strlcpy(p.name, name, sizeof(p.name));

	for (TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)))
			ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null))->Packet(&p, sizeof(p));
	}
}
void CParty::UpdateOfflineState(uint32_t dwPID)
{
	//const TMember& r = m_memberMap[dwPID];

	TPacketGCPartyAdd p;
	p.header = HEADER_GC_PARTY_ADD;
	p.pid = dwPID;
	memset(p.name, 0, CHARACTER_NAME_MAX_LEN+1);

	for (TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (it->second.pCharacter && ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null)))
			ecs::PlayerRuntime::GetDesc(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null))->Packet(&p, sizeof(p));
	}
}

int CParty::GetFlag(std::string_view name)
{
	const std::string key(name);
	TFlagMap::iterator it = m_map_iFlag.find(key);

	if (it != m_map_iFlag.end())
	{
		//LOG_INFO("PARTY GetFlag {} {}", name.c_str(), it->second);
		return it->second;
	}

	//LOG_INFO("PARTY GetFlag {} 0", name.c_str());
	return 0;
}

void CParty::SetFlag(std::string_view name, int value)
{
	const std::string key(name);
	TFlagMap::iterator it = m_map_iFlag.find(key);

	//LOG_INFO("PARTY SetFlag {} {}", name.c_str(), value);
	if (it == m_map_iFlag.end())
	{
		m_map_iFlag.insert(make_pair(key, value));
	}
	else if (it->second != value)
	{
		it->second = value;
	}
}

void CParty::SetDungeon(LPDUNGEON pDungeon)
{
	m_pkDungeon = pDungeon;
	m_map_iFlag.clear();
}

LPDUNGEON CParty::GetDungeon()
{
	return m_pkDungeon;
}

void CParty::SetDungeon_for_Only_party(LPDUNGEON pDungeon)
{
	m_pkDungeon_for_Only_party = pDungeon;
}

LPDUNGEON CParty::GetDungeon_for_Only_party()
{
	return m_pkDungeon_for_Only_party;
}


bool CParty::IsPositionNearLeader(entt::entity character)
{
	const entt::entity chrLeader = m_pkChrLeader ? m_pkChrLeader->GetEntityHandle() : entt::null;
	if (!m_pkChrLeader)
		return false;

	if (DISTANCE_APPROX(ecs::PlayerRuntime::GetX(character) - ecs::PlayerRuntime::GetX(chrLeader), ecs::PlayerRuntime::GetY(character) - ecs::PlayerRuntime::GetY(chrLeader)) >= PARTY_DEFAULT_RANGE)
		return false;

	return true;
}


int CParty::GetExpBonusPercent()
{
	if (GetNearMemberCount() <= 1)
		return 0;

	return m_iExpBonus + m_iLongTimeExpBonus;
}

bool CParty::IsNearLeader(uint32_t pid)
{
	TMemberMap::iterator it = m_memberMap.find(pid);

	if (it == m_memberMap.end())
		return false;

	return it->second.bNear;
}

uint8_t CParty::CountMemberByVnum(uint32_t dwVnum)
{
	if (m_bPCParty)
		return 0;

	auto* tch = static_cast<LPCHARACTER>(nullptr);
	uint8_t bCount = 0;

	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		if (!(tch = it->second.pCharacter))
			continue;

		if (ecs::PlayerRuntime::IsPC(((tch) ? (tch)->GetEntityHandle() : entt::null)))
			continue;

		if (tch->GetMobTable().dwVnum == dwVnum)
			++bCount;
	}

	return bCount;
}

void CParty::SendParameter(entt::entity character)
{
	TPacketGCPartyParameter p;

	p.bHeader = HEADER_GC_PARTY_PARAMETER;
	p.bDistributeMode = m_iExpDistributionMode;

	LPDESC d = ecs::PlayerRuntime::GetDesc(character);

	if (d)
	{
		d->Packet(&p, sizeof(TPacketGCPartyParameter));
	}
}

void CParty::SendParameterToAll()
{
	if (!m_bPCParty)
		return;

	TMemberMap::iterator it;

	for (it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
		if (it->second.pCharacter)
			SendParameter(((it->second.pCharacter) ? (it->second.pCharacter)->GetEntityHandle() : entt::null));
}

void CParty::SetParameter(int iMode)
{
	if (iMode >= PARTY_EXP_DISTRIBUTION_MAX_NUM)
	{
		LOG_ERROR("Invalid exp distribution mode {}", iMode);
		return;
	}

	m_iExpDistributionMode = iMode;
	SendParameterToAll();
}

int CParty::GetExpDistributionMode()
{
	return m_iExpDistributionMode;
}

void CParty::SetExpCentralizeCharacter(uint32_t dwPID)
{
	TMemberMap::iterator it = m_memberMap.find(dwPID);

	if (it == m_memberMap.end())
		return;

	m_pkChrExpCentralize = it->second.pCharacter;
}

LPCHARACTER CParty::GetExpCentralizeCharacter()
{
	return m_pkChrExpCentralize;
}

uint8_t CParty::GetMemberMaxLevel()
{
	uint8_t bMax = 0;

	auto it = m_memberMap.begin();
	while (it!=m_memberMap.end())
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

uint8_t CParty::GetMemberMinLevel()
{
	uint8_t bMin = PLAYER_MAX_LEVEL_CONST;

	auto it = m_memberMap.begin();
	while (it!=m_memberMap.end())
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

int CParty::ComputePartyBonusExpPercent()
{
	if (GetNearMemberCount() <= 1)
		return 0;

	auto* leader = GetLeaderCharacter();
	const entt::entity leaderEntity = leader ? leader->GetEntityHandle() : entt::null;


	int iBonusPartyExpFromItem = 0;

	// UPGRADE_PARTY_BONUS
	int iMemberCount=MIN(8, GetNearMemberCount());

	if (leader && (leader->IsEquipUniqueItem(UNIQUE_ITEM_PARTY_BONUS_EXP) || leader->IsEquipUniqueItem(UNIQUE_ITEM_PARTY_BONUS_EXP_MALL)
		|| leader->IsEquipUniqueItem(UNIQUE_ITEM_PARTY_BONUS_EXP_GIFT) || leader->IsEquipUniqueGroup(10010)))
	{
		// �߱��� ���� ������ Ȯ���ؾ��Ѵ�.
		iBonusPartyExpFromItem = 30;
	}

#ifdef ENABLE_NEW_USE_POTION
	if (leader && ecs::PointSystem::Get(leaderEntity, POINT_PARTY_DROPEXP) > 0) {
		iBonusPartyExpFromItem += ecs::PointSystem::Get(leaderEntity, POINT_PARTY_DROPEXP);
	}
#endif

	return iBonusPartyExpFromItem + CHN_aiPartyBonusExpPercentByMemberCount[iMemberCount];
	// END_OF_UPGRADE_PARTY_BONUS
}

bool CParty::IsPartyInDungeon(int mapIndex)
{
	// ��Ƽ���� mapIndex�� �����ȿ� �ִ��� ������� �˻�
	for(TMemberMap::iterator it = m_memberMap.begin(); it != m_memberMap.end(); ++it)
	{
		auto* ch = it->second.pCharacter;

		if(nullptr == ch)
		{
			continue;
		}

		LPDUNGEON d = ch->GetDungeon();

		if(nullptr == d)
		{
			LOG_TRACE("not in dungeon");
			continue;
		}

		if( mapIndex == (d->GetMapIndex())/10000 )
		{
			return true;
		}

	}
	return false;
}







