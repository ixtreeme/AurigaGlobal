#include "stdafx.h"
#include <Core/Logging.hpp>
#include "input.h"
#include "packet.h"
#include "protocol.h"
#include "char.h"
#include "constants.h"
#include "utils.h"
#include "unique_item.h"
#include "desc_client.h"
#include "../ecs/systems/PlayerItemActions.hpp"
#include "../ecs/systems/PlayerRuntimeSystem.hpp"
#include "../ecs/systems/PointSystem.hpp"
#include "../ecs/systems/SkillSystem.hpp"
#include "../ecs/systems/AffectSystem.hpp"
#include "../ecs/systems/SocialSystem.hpp"
#include "../ecs/systems/StatSystem.hpp"
#include "char_manager.h"
#include "desc.h"
#include "../ecs/systems/NetworkSyncSystem.hpp"
#include "../ecs/systems/SessionSystem.hpp"
#include "desc_manager.h"
#include "questmanager.h"

// Retired DB item frames never create or complete a character inventory.
// Only the successful native ItemStore login opens that load barrier.
void CInputDB::ItemLoad(LPDESC d, const char*)
{
    if (d) d->SetPhase(PHASE_CLOSE);
}

void CInputDB::ItemAwardInformer(TPacketItemAwardInfromer *data)
{
	LPDESC d = DESC_MANAGER::instance().FindByLoginName(data->login);

	if(d == nullptr)
		return;
	else
	{
		const entt::entity chEntity = d->GetEntity();
		if (ecs::IsCharacter(chEntity))
		{
			ecs::PlayerRuntime::SetItemAwardVnum(chEntity, data->vnum);
			ecs::PlayerRuntime::SetItemAwardCommand(chEntity, data->command);

			if(d->IsPhase(PHASE_GAME))
			{
				quest::CQuestManager::instance().ItemInformer((ecs::PlayerRuntime::GetPlayerID(chEntity)),ecs::PlayerRuntime::GetItemAwardVnum(chEntity));
			}
		}
	}
}
