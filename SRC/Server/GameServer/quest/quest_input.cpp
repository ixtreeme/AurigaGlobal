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
#include "questmanager.h"
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

void CInputMain::ScriptButton(entt::entity character, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ScriptButton handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGScriptButton * p = (TPacketCGScriptButton *) c_pData;
	LOG_INFO("QUEST ScriptButton pid {} idx {}", ecs::PlayerRuntime::GetPlayerID(character), p->idx);

	quest::PC* pc = quest::CQuestManager::instance().GetPCForce(ecs::PlayerRuntime::GetPlayerID(character));
	if (pc && pc->IsConfirmWait())
	{
		quest::CQuestManager::instance().Confirm(ecs::PlayerRuntime::GetPlayerID(character), quest::CONFIRM_TIMEOUT);
	}
	else if (p->idx & 0x80000000)
	{
		quest::CQuestManager::Instance().QuestInfo(ecs::PlayerRuntime::GetPlayerID(character), p->idx & 0x7fffffff);
	}
	else
	{
		quest::CQuestManager::Instance().QuestButton(ecs::PlayerRuntime::GetPlayerID(character), p->idx);
	}
}

void CInputMain::ScriptAnswer(entt::entity character, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ScriptAnswer handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGScriptAnswer * p = (TPacketCGScriptAnswer *) c_pData;
	LOG_INFO("QUEST ScriptAnswer pid {} answer {}", ecs::PlayerRuntime::GetPlayerID(character), p->answer);

	if (p->answer > 250)
	{
		quest::CQuestManager::Instance().Resume(ecs::PlayerRuntime::GetPlayerID(character));
	}
	else
	{
		quest::CQuestManager::Instance().Select(ecs::PlayerRuntime::GetPlayerID(character),  p->answer);
	}
}

void CInputMain::ScriptSelectItem(entt::entity character, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate ScriptSelectItem handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGScriptSelectItem* p = (TPacketCGScriptSelectItem*) c_pData;
	LOG_INFO("QUEST ScriptSelectItem pid {} answer {}", ecs::PlayerRuntime::GetPlayerID(character), p->selection);
	quest::CQuestManager::Instance().SelectItem(ecs::PlayerRuntime::GetPlayerID(character), p->selection);
}

void CInputMain::QuestInputString(entt::entity character, const void* c_pData)
{
// migrated from CHARACTER handler
// TODO Phase 8: migrate QuestInputString handler ECS
// DUAL-PATH: legacy only during migration window
	TPacketCGQuestInputString * p = (TPacketCGQuestInputString*) c_pData;

	char msg[65];
	strlcpy(msg, p->msg, sizeof(msg));
	LOG_INFO("QUEST InputString pid {} msg {}", ecs::PlayerRuntime::GetPlayerID(character), msg);

	quest::CQuestManager::Instance().Input(ecs::PlayerRuntime::GetPlayerID(character), msg);
}

void CInputMain::QuestConfirm(entt::entity character, const void* c_pData)
{
	if (!c_pData || !ecs::PlayerRuntime::IsPC(character))
		return;
	const auto* p = static_cast<const TPacketCGQuestConfirm*>(c_pData);
	const auto waiting = CHARACTER_MANAGER::instance().FindEntityByPID(p->requestPID);
	if (!ecs::PlayerRuntime::IsPC(waiting))
		return;
	const auto answer = p->answer ? quest::CONFIRM_YES : static_cast<quest::EQuestConfirmType>(p->answer);
	LOG_INFO("QuestConfirm from {} pid {} name {} answer {}", ecs::PlayerRuntime::GetName(character).data(),
		p->requestPID, ecs::PlayerRuntime::GetName(waiting).data(), static_cast<int>(answer));
	quest::CQuestManager::Instance().Confirm(ecs::PlayerRuntime::GetPlayerID(waiting), answer,
		ecs::PlayerRuntime::GetPlayerID(character));
}
