#include "../../stdafx.h"

#include "QuestSystem.hpp"

#include "../CharacterAccessors.hpp"
#include "../../char.h"

#include <string>

namespace ecs::QuestSystem {

int32_t GetFlag(entt::entity e, std::string_view flagName)
{
	auto* ch = ecs::LegacyCharOf(e);
	if (!ch)
		return 0;

	return ch->GetQuestFlag(std::string(flagName));
}

void SetFlag(entt::entity e, std::string_view flagName, int32_t value)
{
	auto* ch = ecs::LegacyCharOf(e);
	if (!ch)
		return;

	ch->SetQuestFlag(std::string(flagName), value);
}

} // namespace ecs::QuestSystem
