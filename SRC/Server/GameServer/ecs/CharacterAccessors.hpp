#pragma once

#include <cstdint>
#include <utility>

#include <entt/entt.hpp>

#include "AIHelpers.hpp"
#include "Registry.hpp"
#include "VIDRegistry.hpp"
#include "components/character_runtime_components.hpp"
#include "components/identity_components.hpp"
#include "components/spatial_components.hpp"
#include "components/transform_components.hpp"
#include "components/vital_components.hpp"
#include "EntityInvariants.hpp"
#include "../char_interface.hpp"
#include "../typedef.h"

namespace ecs {

inline auto LegacyCharOf(entt::entity e) -> decltype(std::declval<ecs::LegacyCharPtr>().ptr)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return nullptr;
    }

    if (const auto* legacy = g_registry.try_get<ecs::LegacyCharPtr>(e)) {
        return legacy->ptr;
    }

    return nullptr;
}

// A character is an entity carrying one of the four character type tags.
// Every character CHARACTER_MANAGER builds gets its tag right after its
// legacy pointer, and loses both in the same registry destroy, so this
// answers `LegacyCharOf(e) != nullptr` without touching the pointer.
inline bool IsCharacter(entt::entity e)
{
    return Invariants::HasAnyTypeTag(g_registry, e);
}

inline ecs::CharacterRuntimeFlagsComponent* TryGetRuntimeFlags(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return nullptr;
    }

    return g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
}

} // namespace ecs
