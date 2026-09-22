#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "packet.h"
#include "protocol.h"
#include "char.h"
#include "constants.h"
#include "utils.h"
#include "guild.h"
#include "guild_manager.h"
#include "party.h"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "char_manager.h"
#include "desc.h"
#include "desc_client.h"

void CInputDB::PartyCreate(const char* c_pData)
{
	TPacketPartyCreate* p = (TPacketPartyCreate*) c_pData;
	CPartyManager::instance().P2PCreateParty(p->dwLeaderPID);
}

void CInputDB::PartyDelete(const char* c_pData)
{
	TPacketPartyDelete* p = (TPacketPartyDelete*) c_pData;
	CPartyManager::instance().P2PDeleteParty(p->dwLeaderPID);
}

void CInputDB::PartyAdd(const char* c_pData)
{
	TPacketPartyAdd* p = (TPacketPartyAdd*) c_pData;
	CPartyManager::instance().P2PJoinParty(p->dwLeaderPID, p->dwPID, p->bState);
}

void CInputDB::PartyRemove(const char* c_pData)
{
	TPacketPartyRemove* p = (TPacketPartyRemove*) c_pData;
	CPartyManager::instance().P2PQuitParty(p->dwPID);
}

void CInputDB::PartyStateChange(const char* c_pData)
{
	TPacketPartyStateChange * p = (TPacketPartyStateChange *) c_pData;
	const entt::entity pParty = CPartyManager::instance().P2PCreateParty(p->dwLeaderPID);

	if (pParty == entt::null)
		return;

	PartySystem::SetRole(pParty, p->dwPID, p->bRole, p->bFlag);
}

void CInputDB::PartySetMemberLevel(const char* c_pData)
{
	TPacketPartySetMemberLevel* p = (TPacketPartySetMemberLevel*) c_pData;
	const entt::entity pParty = CPartyManager::instance().P2PCreateParty(p->dwLeaderPID);

	if (pParty == entt::null)
		return;

	PartySystem::P2PSetMemberLevel(pParty, p->dwPID, p->bLevel);
}
