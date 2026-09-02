
#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/AIHelpers.hpp"
#include "ecs/systems/QuestSystem.hpp"
#include "horsename_manager.h"
#include "desc_client.h"
#include "char_manager.h"
#include "char_interface.hpp"
#include "affect.h"
#include "utils.h"

CHorseNameManager::CHorseNameManager()
{
	m_mapHorseNames.clear();
}

const char* CHorseNameManager::GetHorseName(uint32_t dwPlayerID)
{
	std::map<uint32_t, std::string>::iterator iter;

	iter = m_mapHorseNames.find(dwPlayerID);

	if ( iter != m_mapHorseNames.end() )
	{
		return iter->second.c_str();
	}
	else
	{
		return nullptr;
	}
}

void CHorseNameManager::UpdateHorseName(uint32_t dwPlayerID, const char* szHorseName, bool broadcast)
{
	if ( szHorseName == nullptr)
	{
		LOG_ERROR("HORSE_NAME: NULL NAME ({})", dwPlayerID);
		szHorseName = "";
	}

	LOG_INFO("HORSENAME: update {} {}", dwPlayerID, szHorseName);

	m_mapHorseNames[dwPlayerID] = szHorseName;

	if ( broadcast == true )
	{
		BroadcastHorseName(dwPlayerID, szHorseName);
	}
}

void CHorseNameManager::BroadcastHorseName(uint32_t dwPlayerID, const char* szHorseName)
{
	TPacketUpdateHorseName packet;
	packet.dwPlayerID = dwPlayerID;
	strlcpy(packet.szHorseName, szHorseName, sizeof(packet.szHorseName));

	db_clientdesc->DBPacket(HEADER_GD_UPDATE_HORSE_NAME, 0, &packet, sizeof(TPacketUpdateHorseName));
}

void CHorseNameManager::Validate(LPCHARACTER pChar)
{
	const entt::entity charEntity = pChar ? pChar->GetEntityHandle() : entt::null;
	CAffect *pkAff = AffectSystem::FindAffect(charEntity, AFFECT_HORSE_NAME);

	if ( pkAff != nullptr)
	{
		if ( ecs::QuestSystem::GetFlag(charEntity, "horse_name.valid_till") < get_global_time() )
		{
			pChar->HorseSummon(false, true);
			AffectSystem::RemoveAffect(charEntity, pkAff);
			UpdateHorseName(ecs::PlayerRuntime::GetPlayerID(charEntity), "", true);
			pChar->HorseSummon(true, true);
		}
		else
		{
			++(pkAff->lDuration);
		}
	}
}

