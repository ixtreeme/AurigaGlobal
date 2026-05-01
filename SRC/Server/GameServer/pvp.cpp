#include "stdafx.h"
#include <Core/Logging.hpp>
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/SocialSystem.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/PointSystem.hpp"
#include "constants.h"
#include "pvp.h"
#include "crc32.h"
#include "packet.h"
#include "desc.h"
#include "desc_manager.h"
#include "char_interface.hpp"
#include "char_manager.h"
#include "config.h"
#include "sectree_manager.h"
#include "buffer_manager.h"
#include "locale_service.h"
#include "ecs/CharacterAccessors.hpp"

#ifdef ENABLE_PVP_ADVANCED
/*
#ifdef __NEWPET_SYSTEM__
	#include "New_PetSystem.h" roba tua...
#endif
*/
	#include "affect.h"
	#include "party.h"
	#include "guild.h"
	#include "skill.h"
#ifdef __PET_SYSTEM__
	#include "PetSystem.h"
#endif
#ifdef __NEWPET_SYSTEM__
	#include "New_PetSystem.h"
#endif
#endif

using namespace std;

#ifdef ENABLE_PVP_ADVANCED

EVENTINFO(TPVPDuelEventInfo)
{
	DynamicCharacterPtr ch;
	DynamicCharacterPtr victim;
	CPVP * pvp;
	uint8_t state;

	TPVPDuelEventInfo() : ch(), victim(), state(0) {}
};

EVENTINFO(TPVPCheckDisconnect)
{
	DynamicCharacterPtr ch;
	DynamicCharacterPtr victim;

	TPVPCheckDisconnect() : ch(), victim() {}
};

static LPEVENT m_pCheckDisconnect = nullptr;

EVENTFUNC(pvp_check_disconnect)
{
	if (event == nullptr)
		return 0;

	if (event->info == nullptr)
		return 0;

	TPVPCheckDisconnect* info = dynamic_cast<TPVPCheckDisconnect*>(event->info);

	if (info == nullptr)
	{
		LOG_ERROR("disconnect_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER chA = info->ch;
	LPCHARACTER chB = info->victim;

	if (chA == nullptr && chB == nullptr)
	{
		return 0;
	}

	if (chA == nullptr)
	{
		const char* szTableStaticPvP[] = {BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT};

		int betMoney = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[8]);

		if (betMoney > 0)
		{
			ecs::PointSystem::Change(AIHelpers::EcsOf(chB), POINT_GOLD, betMoney, true);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chB), CHAT_TYPE_INFO, 514, "");
#endif
		}

		char buf[CHAT_MAX_LEN + 1];
		snprintf(buf, sizeof(buf), "BINARY_Duel_Delete");
		ecs::ChatSystem::Send(AIHelpers::EcsOf(chB), CHAT_TYPE_COMMAND, buf);

		for (unsigned int i = 0; i < _countof(szTableStaticPvP); i++) {
			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[i], 0);
		}

#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chB), CHAT_TYPE_INFO, 513, "");
#endif
		event_cancel(&m_pCheckDisconnect);
		m_pCheckDisconnect = nullptr;
		return 0;
	}

	if (chB == nullptr)
	{
		const char* szTableStaticPvP[] = {BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT};

		int betMoney = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[8]);

		if (betMoney > 0)
		{
			ecs::PointSystem::Change(AIHelpers::EcsOf(chA), POINT_GOLD, betMoney, true);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chA), CHAT_TYPE_INFO, 514, "");
#endif
		}

		char buf[CHAT_MAX_LEN + 1];
		snprintf(buf, sizeof(buf), "BINARY_Duel_Delete");
		ecs::ChatSystem::Send(AIHelpers::EcsOf(chA), CHAT_TYPE_COMMAND, buf);

		for (unsigned int i = 0; i < _countof(szTableStaticPvP); i++) {
			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[i], 0);
		}

#ifdef TEXTS_IMPROVEMENT
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(chA), CHAT_TYPE_INFO, 513, "");
#endif
		event_cancel(&m_pCheckDisconnect);
		m_pCheckDisconnect = nullptr;
		return 0;
	}

	return PASSES_PER_SEC(1);
}

EVENTFUNC(pvp_duel_counter)
{

	if (event == nullptr)
		return 0;

	if (event->info == nullptr)
		return 0;

	TPVPDuelEventInfo* info = dynamic_cast<TPVPDuelEventInfo*>(event->info);

	if (info == nullptr)
	{
		LOG_ERROR("ready_to_start_event> <Factor> Null pointer");
		return 0;
	}

	LPCHARACTER chA = info->ch;
	LPCHARACTER chB = info->victim;

	if (chA == nullptr)
	{
		LOG_ERROR("Duel: Duel start event info is null.");
		return 0;
	}

	if (chB == nullptr)
	{
		LOG_ERROR("Duel: Duel start event info is null.");
		return 0;
	}

	switch (info->state)
	{
		case 0:
		{
			chA->SpecificEffectPacket("D:/ymir work/ui/game/pvp_advanced/3.mse");
			chB->SpecificEffectPacket("D:/ymir work/ui/game/pvp_advanced/3.mse");

			info->state++;
			return PASSES_PER_SEC(1); break;
		}
		case 1:
		{
			chA->SpecificEffectPacket("D:/ymir work/ui/game/pvp_advanced/2.mse");
			chB->SpecificEffectPacket("D:/ymir work/ui/game/pvp_advanced/2.mse");
			info->state++;
			return PASSES_PER_SEC(1);
			break;
		}
		case 2:
		{
			if ((chA->GetDuel("BlockParty")) && (chB->GetDuel("BlockParty")))
			{
				LPPARTY chParty = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(chA));
				LPPARTY victimParty = ecs::SocialSystem::GetParty(AIHelpers::EcsOf(chB));

				if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(chA)))
					chParty->Quit((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(chA))));

				if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(chB)))
					victimParty->Quit((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(chB))));
			}

			if ((chA->GetDuel("BlockPet")) && (chB->GetDuel("BlockPet")))
			{
#ifdef __PET_SYSTEM__
				{
					CPetSystem* chPet = chA->GetPetSystem();
					CPetSystem* victimPet = chB->GetPetSystem();
					if (chPet)
						chPet->UnsummonAll();

					if (victimPet)
						victimPet->UnsummonAll();
				}
#endif
#ifdef __NEWPET_SYSTEM__
				{
					CNewPetSystem* chPet = chA->GetNewPetSystem();
					CNewPetSystem* victimPet = chB->GetNewPetSystem();
					if (chPet)
						chPet->UnsummonAll(chA);

					if (victimPet)
						victimPet->UnsummonAll(chB);
				}
#endif
			}

			if ((chA->GetDuel("BlockPoly")) && (chB->GetDuel("BlockPoly")))
			{
				if (chA->IsPolymorphed()) {
					chA->SetPolymorph(0);
					AffectSystem::RemoveAffect(AIHelpers::EcsOf(chA), AFFECT_POLYMORPH);
				}

				if (chB->IsPolymorphed()) {
					chB->SetPolymorph(0);
					AffectSystem::RemoveAffect(AIHelpers::EcsOf(chB), AFFECT_POLYMORPH);
				}
			}

			if ((chA->GetDuel("BlockRide")) && (chB->GetDuel("BlockRide")))
			{
				if (AffectSystem::FindAffect(AIHelpers::EcsOf(chA), AFFECT_MOUNT)) {
					AffectSystem::RemoveAffect(AIHelpers::EcsOf(chA), AFFECT_MOUNT);
					AffectSystem::RemoveAffect(AIHelpers::EcsOf(chA), AFFECT_MOUNT_BONUS);
					chA->MountVnum(0);
				}

				if (AffectSystem::FindAffect(AIHelpers::EcsOf(chB), AFFECT_MOUNT)) {
					AffectSystem::RemoveAffect(AIHelpers::EcsOf(chB), AFFECT_MOUNT);
					AffectSystem::RemoveAffect(AIHelpers::EcsOf(chB), AFFECT_MOUNT_BONUS);
					chB->MountVnum(0);
				}

				if (chA->IsHorseRiding())
					chA->StopRiding();

				if (chB->IsHorseRiding())
					chB->StopRiding();

				if (chA->GetHorse())
					chA->HorseSummon(false);

				if (chB->GetHorse())
					chB->HorseSummon(false);
			}

			int m_nTableSkill[] = {94,95,96,109,110,111};

			for (unsigned int i = 0; i < _countof(m_nTableSkill); i++)
			{
				if ((chA->GetDuel("BlockBuff")) && (chB->GetDuel("BlockBuff")))
				{
					if (chA->GetJob() != JOB_SHAMAN)
						AffectSystem::RemoveAffect(AIHelpers::EcsOf(chA), m_nTableSkill[i]);

					if (chB->GetJob() != JOB_SHAMAN)
						AffectSystem::RemoveAffect(AIHelpers::EcsOf(chB), m_nTableSkill[i]);
				}
			}

			chA->SpecificEffectPacket("D:/ymir work/ui/game/pvp_advanced/1.mse");
			chB->SpecificEffectPacket("D:/ymir work/ui/game/pvp_advanced/1.mse");

			info->state++;
			return PASSES_PER_SEC(1);
			break;
		}
		case 3:
		{
			chA->SpecificEffectPacket("D:/ymir work/ui/game/pvp_advanced/go.mse");
			chB->SpecificEffectPacket("D:/ymir work/ui/game/pvp_advanced/go.mse");

			info->state++;
			return PASSES_PER_SEC(1);
			break;
		}
		case 4:
		{
			const char* szTableStaticPvP[] = {BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT};

			const char* chA_Name = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(chA)).data();
			const char* chB_Name = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(chB)).data();

			int chA_Level = (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(chA)));
			int chB_Level = (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(chB)));

			uint32_t chA_Race = (ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(chA)));
			uint32_t chB_Race = (ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(chB)));

			int chA_[] = {(ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[0])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[1])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[2])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[3])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[4])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[5])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[6])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[7])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[8]))};
			int chB_[] = {(ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[0])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[1])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[2])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[3])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[4])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[5])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[6])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[7])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[8]))};

			char chA_buf[CHAT_MAX_LEN + 1], chB_buf[CHAT_MAX_LEN + 1];

			snprintf(chA_buf, sizeof(chA_buf), "BINARY_Duel_LiveInterface %s %d %d %d %d %d %d %d %d %d %d %d", chB_Name, chB_Level, chB_Race, chA_[0], chA_[1], chA_[2], chA_[3], chA_[4], chA_[5], chA_[6], chA_[7], chA_[8]);
			snprintf(chB_buf, sizeof(chB_buf), "BINARY_Duel_LiveInterface %s %d %d %d %d %d %d %d %d %d %d %d", chA_Name, chA_Level, chA_Race, chB_[0], chB_[1], chB_[2], chB_[3], chB_[4], chB_[5], chB_[6], chB_[7], chB_[8]);

			ecs::ChatSystem::Send(AIHelpers::EcsOf(chA), CHAT_TYPE_COMMAND, chA_buf);
			ecs::ChatSystem::Send(AIHelpers::EcsOf(chB), CHAT_TYPE_COMMAND, chB_buf);

			chA->SetHP(ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(chA)));
			chB->SetHP(ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(chB)));

			info->pvp->Packet();
			return 0;
			break;
		}
		default:
		{
			return 0;
			break;
		}
	}
}
#endif

CPVP::CPVP(uint32_t dwPID1, uint32_t dwPID2)
{
	if (dwPID1 > dwPID2)
	{
		m_players[0].dwPID = dwPID1;
		m_players[1].dwPID = dwPID2;
		m_players[0].bAgree = true;
	}
	else
	{
		m_players[0].dwPID = dwPID2;
		m_players[1].dwPID = dwPID1;
		m_players[1].bAgree = true;
	}

	uint32_t adwID[2];
	adwID[0] = m_players[0].dwPID;
	adwID[1] = m_players[1].dwPID;
	m_dwCRC = GetFastHash((const char *) &adwID, 8);
	m_bRevenge = false;

	SetLastFightTime();
}

CPVP::CPVP(CPVP & k)
{
	m_players[0] = k.m_players[0];
	m_players[1] = k.m_players[1];

	m_dwCRC = k.m_dwCRC;
	m_bRevenge = k.m_bRevenge;

	SetLastFightTime();
}

CPVP::~CPVP()
{
}

void CPVP::Packet(bool bDelete)
{
	if (!m_players[0].dwVID || !m_players[1].dwVID)
	{
		if (bDelete)
			LOG_ERROR("null vid when removing {} {}", m_players[0].dwVID, m_players[0].dwVID);

		return;
	}

	TPacketGCPVP pack;

	pack.bHeader = HEADER_GC_PVP;

	if (bDelete)
	{
		pack.bMode = PVP_MODE_NONE;
		pack.dwVIDSrc = m_players[0].dwVID;
		pack.dwVIDDst = m_players[1].dwVID;
	}
	else if (IsFight())
	{
		pack.bMode = PVP_MODE_FIGHT;
		pack.dwVIDSrc = m_players[0].dwVID;
		pack.dwVIDDst = m_players[1].dwVID;
	}
	else
	{
		pack.bMode = m_bRevenge ? PVP_MODE_REVENGE : PVP_MODE_AGREE;

		if (m_players[0].bAgree)
		{
			pack.dwVIDSrc = m_players[0].dwVID;
			pack.dwVIDDst = m_players[1].dwVID;
		}
		else
		{
			pack.dwVIDSrc = m_players[1].dwVID;
			pack.dwVIDDst = m_players[0].dwVID;
		}
	}

	const DESC_MANAGER::DESC_SET & c_rSet = DESC_MANAGER::instance().GetClientSet();
	DESC_MANAGER::DESC_SET::const_iterator it = c_rSet.begin();

	while (it != c_rSet.end())
	{
		LPDESC d = *it++;

		if (d->IsPhase(PHASE_GAME) || d->IsPhase(PHASE_DEAD))
			d->Packet(&pack, sizeof(pack));
	}
}

bool CPVP::Agree(uint32_t dwPID)
{

	m_players[m_players[0].dwPID != dwPID ? 1 : 0].bAgree = true;

#ifdef ENABLE_PVP_ADVANCED
	if (IsFight())
	{
		if (m_pAdvancedDuelTimer != nullptr)
		{
			event_cancel(&m_pAdvancedDuelTimer);
		}

		if (m_pCheckDisconnect != nullptr)
		{
			event_cancel(&m_pCheckDisconnect);
		}

		LPCHARACTER chA = CHARACTER_MANAGER::Instance().FindByPID(dwPID);
		LPCHARACTER chB = CHARACTER_MANAGER::Instance().FindByPID(m_players[m_players[0].dwPID != dwPID ? 0 : 1].dwPID);
		if (!chA || !chB) {
			return false;
		}

		ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(chA), "pvp.timed", 0);
		ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(chB), "pvp.timed", 0);
		const char* szTableStaticPvP[] = {BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT};

		if (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chA), szTableStaticPvP[9]) != 1 && ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(chB), szTableStaticPvP[9]) != 1)
		{
			chA->SetDuel("IsFight", 1);
			chB->SetDuel("IsFight", 1);
		}

		{
			TPVPDuelEventInfo* info = AllocEventInfo<TPVPDuelEventInfo>();
			info->ch = chA;
			info->victim = chB;
			info->state = 0;
			info->pvp = this;

			m_pAdvancedDuelTimer = event_create(pvp_duel_counter, info, PASSES_PER_SEC(1));
		}

		{
			TPVPCheckDisconnect* info = AllocEventInfo<TPVPCheckDisconnect>();
			info->ch = chA;
			info->victim = chB;

			m_pCheckDisconnect = event_create(pvp_check_disconnect, info, PASSES_PER_SEC(1));
		}

		return true;
	}
#else
	if (IsFight())
	{
		Packet();
		return true;
	}
#endif

	return false;
}

bool CPVP::IsFight()
{
	return (m_players[0].bAgree == m_players[1].bAgree) && m_players[0].bAgree;
}

void CPVP::Win(uint32_t dwPID)
{
	int iSlot = m_players[0].dwPID != dwPID ? 1 : 0;

	m_bRevenge = true;

	m_players[iSlot].bAgree = true; // �ڵ����� ����
	m_players[!iSlot].bCanRevenge = true;
	m_players[!iSlot].bAgree = false;

	Packet();
}

bool CPVP::CanRevenge(uint32_t dwPID)
{
	return m_players[m_players[0].dwPID != dwPID ? 1 : 0].bCanRevenge;
}

void CPVP::SetVID(uint32_t dwPID, uint32_t dwVID)
{
	if (m_players[0].dwPID == dwPID)
		m_players[0].dwVID = dwVID;
	else
		m_players[1].dwVID = dwVID;
}

void CPVP::SetLastFightTime()
{
	m_dwLastFightTime = get_dword_time();
}

uint32_t CPVP::GetLastFightTime()
{
	return m_dwLastFightTime;
}

CPVPManager::CPVPManager()
{
}

CPVPManager::~CPVPManager()
{
}

#ifdef ENABLE_PVP_ADVANCED
void RemoveStateFull(LPCHARACTER pkChr)
{
	if (pkChr != nullptr)
	{
		const char* szTableStaticPvP[] = {BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT};

		for (unsigned int i = 0; i < _countof(szTableStaticPvP); i++)
		{
			char buf[CHAT_MAX_LEN + 1];
			snprintf(buf, sizeof(buf), "BINARY_Duel_Delete");

			ecs::ChatSystem::Send(AIHelpers::EcsOf(pkChr), CHAT_TYPE_COMMAND, buf);
			ecs::QuestSystem::SetFlag(AIHelpers::EcsOf(pkChr), szTableStaticPvP[i], 0);
		}
	}
}

void CPVPManager::Decline(LPCHARACTER pkChr, LPCHARACTER pkVictim)
{
	// Fake PC / desc n�lk�li entit�s ne menjen be pvp state-be
	//if (pkChr->IsFakePlayer() || pkVictim->IsFakePlayer())
	//	return;
	if (pkChr && pkVictim)
	{
		RemoveStateFull(pkChr);
		RemoveStateFull(pkVictim);
	}

	CPVPSetMap::iterator it = m_map_pkPVPSetByID.find((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))));

	if (it == m_map_pkPVPSetByID.end())
		return;

	//bool found = false;

	std::unordered_set<CPVP*>::iterator it2 = it->second.begin();

	while (it2 != it->second.end()) {
		CPVP * pkPVP = *it2++;
		uint32_t dwCompanionPID;

		if (pkPVP->m_players[0].dwPID == (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))))
			dwCompanionPID = pkPVP->m_players[1].dwPID;
		else
			dwCompanionPID = pkPVP->m_players[0].dwPID;

		if (dwCompanionPID == (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkVictim))))
		{
			if (pkPVP->IsFight())
			{
#ifdef TEXTS_IMPROVEMENT
				ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChr), CHAT_TYPE_INFO, 511, "");
#endif
				return;
			}

			pkPVP->Packet(true);
			Delete(pkPVP);
			pkPVP->SetLastFightTime();
			//found = true;

			RemoveStateFull(pkChr);
			RemoveStateFull(pkVictim);
#ifdef TEXTS_IMPROVEMENT
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkVictim), CHAT_TYPE_INFO, 512, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pkChr)).data());
#endif
		}
	}
}
#endif

void CPVPManager::Insert(LPCHARACTER pkChr, LPCHARACTER pkVictim)
{
	//if (pkChr->IsFakePlayer() || pkVictim->IsFakePlayer())
	//	return;
	if (pkChr->IsDead() || pkVictim->IsDead())
		return;

	CPVP kPVP((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))), (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkVictim))));

	CPVP * pkPVP;

	if ((pkPVP = Find(kPVP.m_dwCRC)))
	{
#ifdef TEXTS_IMPROVEMENT
		if (pkPVP->Agree((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))))) {
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkVictim), CHAT_TYPE_INFO, 115, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pkChr)).data());
			ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChr), CHAT_TYPE_INFO, 115, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pkVictim)).data());
		}
#endif
		return;
	}

	pkPVP = M2_NEW CPVP(kPVP);

	pkPVP->SetVID((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))), ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(pkChr)));
	pkPVP->SetVID((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkVictim))), ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(pkVictim)));

	m_map_pkPVP.insert(map<uint32_t, CPVP *>::value_type(pkPVP->m_dwCRC, pkPVP));

	m_map_pkPVPSetByID[(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr)))].insert(pkPVP);
	m_map_pkPVPSetByID[(ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkVictim)))].insert(pkPVP);

	pkPVP->Packet();

#ifdef TEXTS_IMPROVEMENT
	ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkVictim), CHAT_TYPE_INFO, 17, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pkChr)).data());
	ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkChr), CHAT_TYPE_INFO, 118, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pkVictim)).data());
#endif

	// NOTIFY_PVP_MESSAGE
	LPDESC pkVictimDesc = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(pkVictim));
#ifdef ENABLE_PVP_ADVANCED
	if (pkVictimDesc)
	{
		const char* szTableStaticPvP[] = {BLOCK_CHANGEITEM, BLOCK_BUFF, BLOCK_POTION, BLOCK_RIDE, BLOCK_PET, BLOCK_POLY, BLOCK_PARTY, BLOCK_EXCHANGE_, BET_WINNER, CHECK_IS_FIGHT};

		int mTable[] = {(ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pkChr), szTableStaticPvP[0])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pkChr), szTableStaticPvP[1])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pkChr), szTableStaticPvP[2])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pkChr), szTableStaticPvP[3])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pkChr), szTableStaticPvP[4])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pkChr), szTableStaticPvP[5])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pkChr), szTableStaticPvP[6])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pkChr), szTableStaticPvP[7])), (ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pkChr), szTableStaticPvP[8]))};

		CGuild * g = ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(pkChr));

		const char* m_Name = ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pkChr)).data();
		const char* m_GuildName = "-";

		int m_Vid = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(pkChr));
		int m_Level = (ecs::PointSystem::GetLevel(AIHelpers::EcsOf(pkChr)));
		int m_PlayTime = ecs::PointSystem::GetReal(AIHelpers::EcsOf(pkChr), POINT_PLAYTIME);
		int m_MaxHP = ecs::PointSystem::GetMaxHP(AIHelpers::EcsOf(pkChr));
		int m_MaxSP = ecs::PointSystem::GetMaxSP(AIHelpers::EcsOf(pkChr));
		int PVP_BLOCK_VIEW_EQUIPMENT = ecs::QuestSystem::GetFlag(AIHelpers::EcsOf(pkChr), BLOCK_EQUIPMENT_);

		uint32_t m_Race = (ecs::PlayerRuntime::GetRaceNum(AIHelpers::EcsOf(pkChr)));

		if (g)
		{
			ecs::ChatSystem::Send(AIHelpers::EcsOf(pkVictim), CHAT_TYPE_COMMAND, "BINARY_Duel_Request %d %s %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d", m_Vid, m_Name, g->GetName(), m_Level, m_Race, m_PlayTime, m_MaxHP, m_MaxSP, mTable[0], mTable[1], mTable[2], mTable[3], mTable[4], mTable[5], mTable[6], mTable[7], mTable[8]);

			if (PVP_BLOCK_VIEW_EQUIPMENT < 1)
				pkChr->SendEquipment(pkVictim);
		}
		else {
			ecs::ChatSystem::Send(AIHelpers::EcsOf(pkVictim), CHAT_TYPE_COMMAND, "BINARY_Duel_Request %d %s %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d", m_Vid, m_Name, m_GuildName, m_Level, m_Race, m_PlayTime, m_MaxHP, m_MaxSP, mTable[0], mTable[1], mTable[2], mTable[3], mTable[4], mTable[5], mTable[6], mTable[7], mTable[8]);

			if (PVP_BLOCK_VIEW_EQUIPMENT < 1)
				pkChr->SendEquipment(pkVictim);
		}
	}
#else
#ifdef TEXTS_IMPROVEMENT
	if (pkVictimDesc) {
		ecs::ChatSystem::SendNew(AIHelpers::EcsOf(pkVictimDesc), CHAT_TYPE_INFO, 824, "%s", ecs::PlayerRuntime::GetName(AIHelpers::EcsOf(pkChr)).data());
	}
#endif
#endif
}

#ifdef ENABLE_NEWSTUFF
bool CPVPManager::IsFighting(LPCHARACTER pkChr)
{
	if (!pkChr)
		return false;

	return IsFighting((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))));
}

bool CPVPManager::IsFighting(uint32_t dwPID)
{
	CPVPSetMap::iterator it = m_map_pkPVPSetByID.find(dwPID);

	if (it == m_map_pkPVPSetByID.end())
		return false;

	std::unordered_set<CPVP*>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		CPVP * pkPVP = *it2++;
		if (pkPVP->IsFight())
			return true;
	}

	return false;
}
#endif

void CPVPManager::ConnectEx(LPCHARACTER pkChr, bool bDisconnect)
{
	const auto it = m_map_pkPVPSetByID.find((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))));

	if (it == m_map_pkPVPSetByID.end())
		return;

	uint32_t dwVID = bDisconnect ? 0 : ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(pkChr));

	auto it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		CPVP * pkPVP = *it2++;
		pkPVP->SetVID((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))), dwVID);
	}
}

void CPVPManager::Connect(LPCHARACTER pkChr)
{
	ConnectEx(pkChr, false);
}

void CPVPManager::Disconnect(LPCHARACTER pkChr)
{
#ifdef ENABLE_PVP_ADVANCED
	const auto it = m_map_pkPVPSetByID.find((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))));

	if (it == m_map_pkPVPSetByID.end())
		return;

	auto it2 = it->second.begin();

	while (it2 != it->second.end()) {
		CPVP * pkPVP = *it2++;
		pkPVP->Packet(true);
		Delete(pkPVP);
	}
#endif
}

void CPVPManager::GiveUp(LPCHARACTER pkChr, uint32_t dwKillerPID) // This method is calling from no where yet.
{
	CPVPSetMap::iterator it = m_map_pkPVPSetByID.find((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))));

	if (it == m_map_pkPVPSetByID.end())
		return;

	LOG_INFO("PVPManager::Dead {}", (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))));
	std::unordered_set<CPVP*>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		CPVP * pkPVP = *it2++;

		uint32_t dwCompanionPID;

		if (pkPVP->m_players[0].dwPID == (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))))
			dwCompanionPID = pkPVP->m_players[1].dwPID;
		else
			dwCompanionPID = pkPVP->m_players[0].dwPID;

		if (dwCompanionPID != dwKillerPID)
			continue;

		pkPVP->SetVID((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))), 0);

		m_map_pkPVPSetByID.erase(dwCompanionPID);

		it->second.erase(pkPVP);

		if (it->second.empty())
			m_map_pkPVPSetByID.erase(it);

		m_map_pkPVP.erase(pkPVP->m_dwCRC);

		pkPVP->Packet(true);
		M2_DELETE(pkPVP);
		break;
	}
}

// ���ϰ�: 0 = PK, 1 = PVP
// PVP�� �����ϸ� ����ġ�� �������� ������ PK�� ������ �ʴ´�.
bool CPVPManager::Dead(LPCHARACTER pkChr, uint32_t dwKillerPID)
{
	CPVPSetMap::iterator it = m_map_pkPVPSetByID.find((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))));

	if (it == m_map_pkPVPSetByID.end())
		return false;

	bool found = false;

	LOG_INFO("PVPManager::Dead {}", (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))));
	std::unordered_set<CPVP*>::iterator it2 = it->second.begin();

	while (it2 != it->second.end())
	{
		CPVP * pkPVP = *it2++;

		uint32_t dwCompanionPID;

		if (pkPVP->m_players[0].dwPID == (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))))
			dwCompanionPID = pkPVP->m_players[1].dwPID;
		else
			dwCompanionPID = pkPVP->m_players[0].dwPID;

		if (dwCompanionPID == dwKillerPID)
		{
			if (pkPVP->IsFight())
			{
				pkPVP->SetLastFightTime();
#ifdef ENABLE_PVP_ADVANCED
				pkPVP->Packet(true);
				Delete(pkPVP);
#else
				pkPVP->Win(dwKillerPID);
#endif
				found = true;
				break;
			}
			else if (get_dword_time() - pkPVP->GetLastFightTime() <= 15000)
			{
				found = true;
				break;
			}
		}
	}

	return found;
}

bool CPVPManager::CanAttack(LPCHARACTER pkChr, LPCHARACTER pkVictim, bool bIsFarmMap)//razor93.2024.12.30//CPVPManager::CanAttack(LPCHARACTER pkChr, LPCHARACTER pkVictim)
{
	switch (pkVictim->GetCharType())
	{
		case CHAR_TYPE_NPC:
		case CHAR_TYPE_WARP:
		case CHAR_TYPE_GOTO:
			return false;
	}

	if (pkChr == pkVictim)  // ���� �� ĥ��� �ϳ� -_-
		return false;

	if (ecs::PlayerRuntime::IsNPC(AIHelpers::EcsOf(pkVictim)) && ecs::PlayerRuntime::IsNPC(AIHelpers::EcsOf(pkChr)) && !pkChr->IsGuardNPC())
		return false;
	// Non-PC combat stays allowed during the migration window.
	// The mount restriction below only gates PC-vs-PC combat.
	if (!(ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(pkVictim))) || !(ecs::PlayerRuntime::IsPC(AIHelpers::EcsOf(pkChr))))
		return true;
	if( true == pkChr->IsHorseRiding() )
	{
		if( pkChr->GetHorseLevel() > 0 && 1 == pkChr->GetHorseGrade() )
			return false;
	}
	else
	{
		eMountType eIsMount = GetMountLevelByVnum(pkChr->GetMountVnum(), false);
		switch (eIsMount)
		{
			case MOUNT_TYPE_NONE:
			case MOUNT_TYPE_COMBAT:
			case MOUNT_TYPE_MILITARY:
				break;
			case MOUNT_TYPE_NORMAL:
			default:
				if (test_server)
					LOG_TRACE("CanUseSkill: Mount can't attack. vnum({}) type({})", pkChr->GetMountVnum(), static_cast<int>(eIsMount));
				return false;
				break;
		}
	}

	if (pkVictim->IsObserverMode() || pkChr->IsObserverMode())
		return false;

	{
		uint8_t bMapEmpire = SECTREE_MANAGER::instance().GetEmpireFromMapIndex(ecs::PlayerRuntime::GetMapIndex(AIHelpers::EcsOf(pkChr)));

		if ( ((pkChr->GetPKMode() == PK_MODE_PROTECT) && ((ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(pkChr))) == bMapEmpire)) ||
				((pkVictim->GetPKMode() == PK_MODE_PROTECT) && ((ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(pkVictim))) == bMapEmpire)) )
		{
			return false;
		}
	}

	if ((ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(pkChr))) != (ecs::PlayerRuntime::GetEmpire(AIHelpers::EcsOf(pkVictim))))
	{
		// @warme005
		{
			if ( pkChr->GetPKMode() == PK_MODE_PROTECT || pkVictim->GetPKMode() == PK_MODE_PROTECT )
			{
				return false;


			}
		}

			if (bIsFarmMap == true)
				return false;
		return true;
	}

	bool beKillerMode = false;

	if (ecs::SocialSystem::GetParty(AIHelpers::EcsOf(pkVictim)) && ecs::SocialSystem::GetParty(AIHelpers::EcsOf(pkVictim)) == ecs::SocialSystem::GetParty(AIHelpers::EcsOf(pkChr)))
	{
		return false;
		// Cannot attack same party on any pvp model
	}
	else
	{
		if (pkVictim->IsKillerMode())
		{
			return true;
		}


		switch (pkChr->GetPKMode())
		{
			case PK_MODE_PEACE:
			case PK_MODE_REVENGE:
				// Cannot attack same guild
				if (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(pkVictim)) && ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(pkVictim)) == ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(pkChr)))
					break;

				/*if (pkChr->GetPKMode() == PK_MODE_REVENGE)
				{
					if (pkChr->GetAlignment() < 0 && pkVictim->GetAlignment() >= 0)
					{
						pkChr->SetKillerMode(true);
						return true;
					}
					else if (pkChr->GetAlignment() >= 0 && pkVictim->GetAlignment() < 0)
						return true;
				}
				break;*/

			case PK_MODE_GUILD:
				// Same implementation from PK_MODE_FREE except for attacking same guild
				if (!ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(pkChr)) || (ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(pkVictim)) != ecs::SocialSystem::GetGuild(AIHelpers::EcsOf(pkChr))))
				{
					pkChr->SetKillerMode(true);
					return true;
				}
				break;

			case PK_MODE_FREE:

				pkChr->SetKillerMode(true);

				return true;
				break;
		}
	}

	CPVP kPVP((ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkChr))), (ecs::PlayerRuntime::GetPlayerID(AIHelpers::EcsOf(pkVictim))));
	CPVP * pkPVP = Find(kPVP.m_dwCRC);

	if (!pkPVP || !pkPVP->IsFight())
	{
		if (beKillerMode)
			pkChr->SetKillerMode(true);

		return (beKillerMode);
	}

	pkPVP->SetLastFightTime();
	return true;
}

CPVP * CPVPManager::Find(uint32_t dwCRC)
{
	map<uint32_t, CPVP *>::iterator it = m_map_pkPVP.find(dwCRC);

	if (it == m_map_pkPVP.end())
		return nullptr;

	return it->second;
}

void CPVPManager::Delete(CPVP * pkPVP)
{
	map<uint32_t, CPVP *>::iterator it = m_map_pkPVP.find(pkPVP->m_dwCRC);

	if (it == m_map_pkPVP.end())
		return;

	m_map_pkPVP.erase(it);
	m_map_pkPVPSetByID[pkPVP->m_players[0].dwPID].erase(pkPVP);
	m_map_pkPVPSetByID[pkPVP->m_players[1].dwPID].erase(pkPVP);

	M2_DELETE(pkPVP);
}

void CPVPManager::SendList(LPDESC d)
{
	map<uint32_t, CPVP *>::iterator it = m_map_pkPVP.begin();

	uint32_t dwVID = ecs::PlayerRuntime::GetPacketVID(AIHelpers::EcsOf(d->GetCharacter()));

	TPacketGCPVP pack;

	pack.bHeader = HEADER_GC_PVP;

	while (it != m_map_pkPVP.end())
	{
		CPVP * pkPVP = (it++)->second;

		if (!pkPVP->m_players[0].dwVID || !pkPVP->m_players[1].dwVID)
			continue;

		// VID�� �Ѵ� ���� ��쿡�� ������.
		if (pkPVP->IsFight())
		{
			pack.bMode = PVP_MODE_FIGHT;
			pack.dwVIDSrc = pkPVP->m_players[0].dwVID;
			pack.dwVIDDst = pkPVP->m_players[1].dwVID;
		}
		else
		{
			pack.bMode = pkPVP->m_bRevenge ? PVP_MODE_REVENGE : PVP_MODE_AGREE;

			if (pkPVP->m_players[0].bAgree)
			{
				pack.dwVIDSrc = pkPVP->m_players[0].dwVID;
				pack.dwVIDDst = pkPVP->m_players[1].dwVID;
			}
			else
			{
				pack.dwVIDSrc = pkPVP->m_players[1].dwVID;
				pack.dwVIDDst = pkPVP->m_players[0].dwVID;
			}
		}

		d->Packet(&pack, sizeof(pack));
		LOG_TRACE("PVPManager::SendList {} {}", pack.dwVIDSrc, pack.dwVIDDst);

		if (pkPVP->m_players[0].dwVID == dwVID)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(pkPVP->m_players[1].dwVID);
			if (ch && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
			{
				LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));
				d->Packet(&pack, sizeof(pack));
			}
		}
		else if (pkPVP->m_players[1].dwVID == dwVID)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().Find(pkPVP->m_players[0].dwVID);
			if (ch && ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch)))
			{
				LPDESC d = ecs::PlayerRuntime::GetDesc(AIHelpers::EcsOf(ch));
				d->Packet(&pack, sizeof(pack));
			}
		}
	}
}

void CPVPManager::Process()
{
	map<uint32_t, CPVP *>::iterator it = m_map_pkPVP.begin();

	while (it != m_map_pkPVP.end())
	{
		CPVP * pvp = (it++)->second;

		if (get_dword_time() - pvp->GetLastFightTime() > 600000) // 10�� �̻� �ο��� ��������
		{
			pvp->Packet(true);
			Delete(pvp);
		}
	}
}

