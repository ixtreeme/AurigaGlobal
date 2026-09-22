#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/ChatSystem.hpp"
#include "../ecs/systems/CombatSystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/MountSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/QuestSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "constants.h"
#include "db.h"
#include "desc.h"
#include "desc_manager.h"
#include "log.h"
#include "packet.h"
#include "protocol.h"
#include "utils.h"
#include "party.h"
#include "char_manager.h"
#include "../ecs/components/dirty_components.hpp"
#include "../ecs/systems/ActivitySystem.hpp"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/MovementSystem.hpp"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "desc_client.h"
#include "gm.h"

void CInputMain::PartyInvite(entt::entity character, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyInvite handler ECS
// DUAL-PATH: legacy only during migration window
	if (ecs::PlayerRuntime::GetArena(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	TPacketCGPartyInvite * p = (TPacketCGPartyInvite*) c_pData;

	const entt::entity pInvitee = CHARACTER_MANAGER::instance().FindEntity(p->vid);

	if (pInvitee == entt::null || !ecs::PlayerRuntime::GetDesc(character) || !ecs::PlayerRuntime::GetDesc(pInvitee))
	{
		LOG_ERROR("PARTY Cannot find invited character");
		return;
	}

	ecs::SocialSystem::PartyInvite(character, pInvitee);
}

void CInputMain::PartyInviteAnswer(entt::entity character, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyInviteAnswer handler ECS
// DUAL-PATH: legacy only during migration window
	if (ecs::PlayerRuntime::GetArena(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	TPacketCGPartyInviteAnswer * p = (TPacketCGPartyInviteAnswer*) c_pData;

	const entt::entity inviter = CHARACTER_MANAGER::instance().FindEntity(p->leader_vid);
	if (!ecs::IsCharacter(inviter) || !ecs::PlayerRuntime::GetDesc(inviter)) {
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 217, "");
#endif
	}
	else if (!p->accept) {
		ecs::SocialSystem::PartyInviteDeny(inviter, ecs::PlayerRuntime::GetPlayerID(character));
	} else {
		ecs::SocialSystem::PartyInviteAccept(inviter, character);
	}
}

void CInputMain::PartySetState(entt::entity character, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartySetState handler ECS
// DUAL-PATH: legacy only during migration window
	if (!CPartyManager::instance().IsEnablePCParty())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 208, "");
#endif
		return;
	}

	TPacketCGPartySetState* p = (TPacketCGPartySetState*) c_pData;

	const entt::entity party = ecs::SocialSystem::GetParty(character);

	if (party == entt::null)
		return;

	if (PartySystem::GetLeaderPID(party) != ecs::PlayerRuntime::GetPlayerID(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 206, "");
#endif
		return;
	}

	if (!PartySystem::IsMember(party, p->pid))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 207, "");
#endif
		return;
	}

	uint32_t pid = p->pid;
	LOG_INFO("PARTY SetRole pid {} to role {} state {}", pid, p->byRole, p->flag ? "on" : "off");

	switch (p->byRole)
	{
		case PARTY_ROLE_NORMAL:
			break;

		case PARTY_ROLE_ATTACKER:
		case PARTY_ROLE_TANKER:
		case PARTY_ROLE_BUFFER:
		case PARTY_ROLE_SKILL_MASTER:
		case PARTY_ROLE_HASTE:
		case PARTY_ROLE_DEFENDER:
			if (PartySystem::SetRole(party, pid, p->byRole, p->flag))
			{
				TPacketPartyStateChange pack;
				pack.dwLeaderPID = ecs::PlayerRuntime::GetPlayerID(character);
				pack.dwPID = p->pid;
				pack.bRole = p->byRole;
				pack.bFlag = p->flag;
				db_clientdesc->DBPacket(HEADER_GD_PARTY_STATE_CHANGE, 0, &pack, sizeof(pack));
			}
			break;
		default:
			LOG_ERROR("wrong byRole in PartySetState Packet name {} state {}", ecs::PlayerRuntime::GetName(character).data(), p->byRole);
			break;
	}
}

void CInputMain::PartyRemove(entt::entity character, const char* c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character))
		return;
	if (ecs::PlayerRuntime::GetArena(character))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 303, "");
#endif
		return;
	}

	if (!CPartyManager::instance().IsEnablePCParty())
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 208, "");
#endif
		return;
	}

	if (ecs::SocialSystem::GetDungeon(character) != entt::null)
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 203, "");
#endif
		return;
	}

	TPacketCGPartyRemove* p = (TPacketCGPartyRemove*) c_pData;

	const entt::entity pParty = ecs::SocialSystem::GetParty(character);

	if (pParty == entt::null)
		return;

	if (PartySystem::GetLeaderPID(pParty) == ecs::PlayerRuntime::GetPlayerID(character))
	{
		if (ecs::SocialSystem::GetDungeon(character) == entt::null) {
			if(PartySystem::IsPartyInDungeon(pParty, 351))
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 648, "");
#endif
				return;
			}

			// leader can remove any member
			if (p->pid == ecs::PlayerRuntime::GetPlayerID(character) || PartySystem::GetMemberCount(pParty) == 2)
			{
				// party disband
				CPartyManager::instance().DeleteParty(pParty);
			}
			else
			{
#ifdef TEXTS_IMPROVEMENT
				const entt::entity B = CHARACTER_MANAGER::instance().FindEntityByPID(p->pid);
				if (B != entt::null) {
					//pParty->SendPartyRemoveOneToAll(B);
					ecs::ChatSystem::SendNew(B, CHAT_TYPE_INFO, 216, "");
					//pParty->Unlink(B);
					//CPartyManager::instance().SetPartyMember(ecs::PlayerRuntime::GetPlayerID(B), NULL);
				}
#endif
				PartySystem::Quit(pParty, p->pid);
			}
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 205, "");
		}
#endif
	}
	else
	{
		if (p->pid == ecs::PlayerRuntime::GetPlayerID(character))
		{
			if (ecs::SocialSystem::GetDungeon(character) == entt::null) {
				if (PartySystem::GetMemberCount(pParty) == 2) {
					CPartyManager::instance().DeleteParty(pParty);
				} else {
#ifdef TEXTS_IMPROVEMENT
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 215, "");
#endif
					//pParty->SendPartyRemoveOneToAll(ch);
					PartySystem::Quit(pParty, ecs::PlayerRuntime::GetPlayerID(character));
					//pParty->SendPartyRemoveAllToOne(ch);
					//CPartyManager::instance().SetPartyMember(ecs::PlayerRuntime::GetPlayerID(character), NULL);
				}
			}
#ifdef TEXTS_IMPROVEMENT
			else {
				ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 204, "");
			}
#endif
		}
#ifdef TEXTS_IMPROVEMENT
		else {
			ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 197, "");
		}
#endif
	}
}

void CInputMain::PartyUseSkill(entt::entity character, const char* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyUseSkill handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGPartyUseSkill* p = (TPacketCGPartyUseSkill*) c_pData;
	const entt::entity pParty = ecs::SocialSystem::GetParty(character);
	if (pParty == entt::null)
		return;

	if (ecs::PlayerRuntime::GetPlayerID(character) != PartySystem::GetLeaderPID(pParty))
	{
#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 211, "");
#endif
		return;
	}

	switch (p->bySkillIndex)
	{
		case PARTY_SKILL_HEAL:
			PartySystem::HealParty(pParty);
			break;
		case PARTY_SKILL_WARP:
			{
				const entt::entity pch = CHARACTER_MANAGER::instance().FindEntity(p->vid);
				if (pch != entt::null) {
					PartySystem::SummonToLeader(pParty, ecs::PlayerRuntime::GetPlayerID(pch));
				}
#ifdef TEXTS_IMPROVEMENT
				else {
					ecs::ChatSystem::SendNew(character, CHAT_TYPE_INFO, 209, "");
				}
#endif
			}
			break;
	}
}

void CInputMain::PartyParameter(entt::entity character, const char * c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate PartyParameter handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGPartyParameter * p = (TPacketCGPartyParameter *) c_pData;

	const entt::entity pParty = ecs::SocialSystem::GetParty(character);
	if (pParty != entt::null)
		PartySystem::SetParameter(pParty, p->bDistributeMode);
}
