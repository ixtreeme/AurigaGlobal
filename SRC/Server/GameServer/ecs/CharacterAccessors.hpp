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

// A character is an entity CHARACTER_MANAGER::CreateCharacterEntity built. It gets
// TagCharacter at creation and loses it in the same registry destroy.
// The type tags do not: the LostCastle clones are built there with none.
inline bool IsCharacter(entt::entity e)
{
    return e != entt::null && g_registry.valid(e) && g_registry.all_of<ecs::TagCharacter>(e);
}

inline ecs::CharacterRuntimeFlagsComponent* TryGetRuntimeFlags(entt::entity e)
{
    if (e == entt::null || !g_registry.valid(e)) {
        return nullptr;
    }

    return g_registry.try_get<ecs::CharacterRuntimeFlagsComponent>(e);
}

} // namespace ecs
