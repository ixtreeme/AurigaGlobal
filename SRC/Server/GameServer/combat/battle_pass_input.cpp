#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "../ecs/Registry.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "char.h"
#include "config.h"
#include "constants.h"
#include "packet.h"
#include "protocol.h"
#include "utils.h"
#include "battle_pass.h"
#include "desc.h"
#include "desc_client.h"
#include "../ecs/CharacterAccessors.hpp"

#ifdef ENABLE_BATTLE_PASS
int CInputMain::BattlePass(entt::entity character, const char* data, size_t uiBytes)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate BattlePass handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGBattlePassAction * p = (TPacketCGBattlePassAction *) data;

	if (uiBytes < sizeof(TPacketCGBattlePassAction))
		return -1;

	//const char * c_pData = data + sizeof(TPacketCGBattlePassAction);
	uiBytes -= sizeof(TPacketCGBattlePassAction);

	switch(p->bAction)
	{
		case 1:
			CBattlePass::instance().BattlePassRequestOpen(character);
			break;

		case 2:
			CBattlePass::instance().BattlePassRequestReward(character);
			break;

		case 3:
		{
			uint32_t dwPlayerId = ecs::PlayerRuntime::GetPlayerID(character);
			uint8_t bIsGlobal = 0;

			db_clientdesc->DBPacketHeader(HEADER_GD_BATTLE_PASS_RANKING, ecs::PlayerRuntime::GetDesc(character)->GetHandle(), sizeof(uint32_t) + sizeof(uint8_t));
			db_clientdesc->Packet(&dwPlayerId, sizeof(uint32_t));
			db_clientdesc->Packet(&bIsGlobal, sizeof(uint8_t));
		}
		break;

		default:
			break;
	}

	return 0;
}
#endif

#ifdef ENABLE_BATTLE_PASS
void CInputDB::BattlePassLoad(LPDESC d, const char * c_pData)
{
	//LOG_ERROR("BattlePassLoad");
	const entt::entity chEntity = d ? d->GetEntity() : entt::null;
	if (!ecs::IsCharacter(chEntity))
		return;

	uint32_t dwPID = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	uint32_t dwCount = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	if (ecs::PlayerRuntime::GetPlayerID(chEntity) != dwPID)
		return;

	ecs::PlayerRuntime::LoadBattlePass(chEntity, dwCount, (TPlayerBattlePassMission *)c_pData);
}
void CInputDB::BattlePassLoadRanking(LPDESC d, const char * c_pData)
{
	//LOG_ERROR("BattlePassLoadRanking");
	const entt::entity chEntity = d ? d->GetEntity() : entt::null;
	if (!ecs::IsCharacter(chEntity))
		return;

	uint32_t dwPID = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	uint8_t bIsGlobal = decode_byte(c_pData);
	c_pData += sizeof(uint8_t);

	uint32_t dwCount = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	//LOG_ERROR("BattlePassLoadRanking count {} playerid {}", dwCount, dwPID);

	if (ecs::PlayerRuntime::GetPlayerID(chEntity) != dwPID)
		return;

	if(dwCount)
	{
		std::vector<TBattlePassRanking> sendVector;
		sendVector.resize(dwCount);

		TBattlePassRanking* p = (TBattlePassRanking*) c_pData;

		for (unsigned int i = 0; i < dwCount; ++i, ++p)
		{
			TBattlePassRanking newRanking;
			newRanking.bPos = p->bPos;
			strlcpy(newRanking.playerName, p->playerName, sizeof(newRanking.playerName));
			newRanking.dwFinishTime = p->dwFinishTime;

			sendVector.push_back(newRanking);
		}

		if(!sendVector.empty())
		{
			TPacketGCBattlePassRanking packet;
			packet.bHeader = HEADER_GC_BATTLE_PASS_RANKING;
			packet.wSize = sizeof(packet) + sizeof(TBattlePassRanking) * sendVector.size();
			packet.bIsGlobal = bIsGlobal;

			ecs::PlayerRuntime::GetDesc(chEntity)->BufferedPacket(&packet, sizeof(packet));
			ecs::PlayerRuntime::GetDesc(chEntity)->Packet(&sendVector[0], sizeof(TBattlePassRanking) * sendVector.size());
		}
	}
#ifdef TEXTS_IMPROVEMENT
	else {
		ecs::ChatSystem::SendNew(chEntity, CHAT_TYPE_INFO, 762, "");
	}
#endif
}
#endif
