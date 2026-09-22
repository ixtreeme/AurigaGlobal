#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "packet.h"
#include "protocol.h"
#include "char.h"
#include "constants.h"
#include "utils.h"
#include "affect.h"
#include "char_manager.h"
#include "../ecs/systems/InventorySystem.hpp"
#include "../ecs/systems/ItemSystem.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "desc.h"
#include "desc_client.h"

void CInputDB::AffectLoad(LPDESC d, const char * c_pData)
{
	if (!d)
		return;

	const entt::entity chEntity = d->GetEntity();
	if (!ecs::IsCharacter(chEntity))
		return;

	uint32_t dwPID = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	uint32_t dwCount = decode_4bytes(c_pData);
	c_pData += sizeof(uint32_t);

	if (ecs::PlayerRuntime::GetPlayerID(chEntity) != dwPID)
		return;

	AffectSystem::LoadAffect(chEntity, dwCount, (TPacketAffectElement *) c_pData);
#ifdef ENABLE_BATTLE_PASS
#ifdef ENABLE_FREE_PASS_RAZOR93
	ecs::PlayerRuntime::EnsureFreeBattlePassActive(chEntity);
	if (!ecs::PlayerRuntime::IsBattlePassLoaded(chEntity))
		ecs::PlayerRuntime::LoadBattlePass(chEntity, 0, nullptr);
#endif
#endif


}
