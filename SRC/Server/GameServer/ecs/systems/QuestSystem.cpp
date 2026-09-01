#include "../../stdafx.h"

#include "QuestSystem.hpp"

#include "../Registry.hpp"
#include "../components/identity_components.hpp"
#include "../../questmanager.h"
#include "../../questpc.h"

#include <string>

namespace ecs::QuestSystem {

int32_t GetFlag(entt::entity e, std::string_view flagName)
{
	if (e == entt::null || !g_registry.valid(e))
		return 0;

	const auto* identity = g_registry.try_get<ecs::PlayerID>(e);
	if (!identity)
		return 0;

	quest::PC* pc = quest::CQuestManager::instance().GetPC(identity->pid);
	return pc ? pc->GetFlag(std::string(flagName)) : 0;
}

void SetFlag(entt::entity e, std::string_view flagName, int32_t value)
{
	if (e == entt::null || !g_registry.valid(e))
		return;

	const auto* identity = g_registry.try_get<ecs::PlayerID>(e);
	if (!identity)
		return;

	if (quest::PC* pc = quest::CQuestManager::instance().GetPC(identity->pid))
		pc->SetFlag(std::string(flagName), value);
}

} // namespace ecs::QuestSystem
