#pragma once
#include "../ecs/Registry.hpp"

// The retired item runtime is never available, including before native login,
// after a failed load, or for an entity without a native owner component.
inline bool BlocksLegacyItems(entt::entity)
{
    return true;
}
