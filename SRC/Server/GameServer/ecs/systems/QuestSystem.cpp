#include "../../stdafx.h"

#include "QuestSystem.hpp"

#include "../CharacterAccessors.hpp"
#include "../../char.h"

namespace ecs::QuestSystem {

int32_t GetFlag(entt::entity e, const std::string& flagName)
{
	auto* ch = ecs::LegacyCharOf(e);
	if (!ch)
		return 0;

	return ch->GetQuestFlag(flagName);
}

void SetFlag(entt::entity e, const std::string& flagName, int32_t value)
{
	auto* ch = ecs::LegacyCharOf(e);
	if (!ch)
		return;

	ch->SetQuestFlag(flagName, value);
}

} // namespace ecs::QuestSystem
