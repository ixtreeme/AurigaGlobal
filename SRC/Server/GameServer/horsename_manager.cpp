
#include "stdafx.h"
#include "ecs/systems/PlayerRuntimeSystem.hpp"
#include <Core/Logging.hpp>
#include "ecs/systems/AffectSystem.hpp"
#include "ecs/systems/MountSystem.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/status_components.hpp"
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
	TPacketUpdateHorseName packet{};
	packet.dwPlayerID = dwPlayerID;
	strlcpy(packet.szHorseName, szHorseName, sizeof(packet.szHorseName));

	if (db_clientdesc)
		db_clientdesc->DBPacket(HEADER_GD_UPDATE_HORSE_NAME, 0, &packet, sizeof(TPacketUpdateHorseName));
}

void CHorseNameManager::Validate(entt::entity character)
{
    const auto affect = AffectSystem::Lease(character,
        AffectSystem::FindAffect(character, AFFECT_HORSE_NAME));
    if (!affect) return;
    auto& state = g_registry.get<ecs::AffectList>(character);
    if (state.horseNameToken) return;
    static uint64_t nextToken = 0;
    const auto token = state.horseNameToken = ++nextToken;
    struct Guard {
        entt::entity entity;
        uint64_t token;
        ~Guard() {
            if (!g_registry.valid(entity)) return;
            if (auto* state = g_registry.try_get<ecs::AffectList>(entity);
                state && state->horseNameToken == token) state->horseNameToken = 0;
        }
    } guard{character, token};
    const CAffect expected = *affect;
    const auto current = [&] {
        if (!g_registry.valid(character)) return false;
        const auto* state = g_registry.try_get<ecs::AffectList>(character);
        return state && state->horseNameToken == token;
    };
    const auto owned = [&] {
        return current() && AffectSystem::Lease(character, affect.get()) == affect &&
            affect->dwType == expected.dwType && affect->bApplyOn == expected.bApplyOn &&
            affect->lApplyValue == expected.lApplyValue && affect->dwFlag == expected.dwFlag &&
            affect->lDuration == expected.lDuration && affect->lSPCost == expected.lSPCost;
    };
    const bool expired = ecs::QuestSystem::GetFlag(character, "horse_name.valid_till") < get_global_time();
    if (!owned()) return;
    if (!expired) {
        if (affect->lDuration < INT32_MAX) ++affect->lDuration;
        return;
    }
    MountSystem::SummonHorse(character, false, true);
    if (!owned()) return;
    AffectSystem::RemoveAffect(character, affect.get());
    const auto noReplacement = [&] {
        return current() && !AffectSystem::FindAffect(character, AFFECT_HORSE_NAME);
    };
    if (!noReplacement()) return;
    UpdateHorseName(ecs::PlayerRuntime::GetPlayerID(character), "", true);
    if (!noReplacement()) return;
    MountSystem::SummonHorse(character, true, true);
}
