#pragma once

#include "EventDispatcher.hpp"
#include "Registry.hpp"
#include "VIDRegistry.hpp"
#include "components/combat_components.hpp"
#include "components/dirty_components.hpp"
#include "components/identity_components.hpp"
#include "components/inventory_components.hpp"
#include "components/quest_components.hpp"
#include "components/session_components.hpp"
#include "components/skill_components.hpp"
#include "components/social_components.hpp"
#include "components/status_components.hpp"
#include "components/transform_components.hpp"
#include "components/vital_components.hpp"

template <typename T>
T* ECS_TryGet(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return nullptr;
    }

    return g_registry.try_get<T>(e);
}

template <typename T>
T& ECS_Get(entt::entity e, const char* context)
{
    if (auto* ptr = ECS_TryGet<T>(e)) {
        return *ptr;
    }

    sys_err("ECS_Get failed: missing component in %s", context);
    static T fallback {};
    return fallback;
}
