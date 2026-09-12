#pragma once

#include <entt/entt.hpp>

namespace item_change
{
	// Returns true if the item use was handled (and UseItem should stop).
	bool HandleUse(entt::entity character, entt::entity item);
}
