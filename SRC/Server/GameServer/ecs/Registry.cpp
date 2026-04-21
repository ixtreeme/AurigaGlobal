#include "../stdafx.h"

#include "Registry.hpp"

#include "../char.h"

entt::registry g_registry;

namespace AIHelpers {

entt::entity EcsOf(LPCHARACTER ch)
{
    if (!ch) {
        return entt::null;
    }

    return ch->GetEntityHandle();
}

entt::entity EcsOf(const CHARACTER* ch)
{
    if (!ch) {
        return entt::null;
    }

    return ch->GetEntityHandle();
}

} // namespace AIHelpers
