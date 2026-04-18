#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "AIHelpers.hpp"
#include "Registry.hpp"
#include "VIDRegistry.hpp"
#include "components/identity_components.hpp"
#include "components/spatial_components.hpp"
#include "components/transform_components.hpp"
#include "../char_interface.hpp"
#include "../typedef.h"

namespace ecs {

// Thin ECS-first accessors with legacy fallback.
// Migration pattern: ch->Method() -> ecs::Method(ch)
// During migration window these delegate to legacy CHARACTER
// methods if ECS component is missing, guaranteeing no regression.

inline uint32_t GetPlayerID(LPCHARACTER ch)
{
    if (!ch) {
        return 0;
    }

    return ch->GetPlayerID();
}

inline const char* GetName(LPCHARACTER ch)
{
    if (!ch) {
        return "";
    }

    return ch->GetName();
}

inline int32_t GetMapIndex(LPCHARACTER ch)
{
    if (!ch) {
        return 0;
    }

    return ch->GetMapIndex();
}

inline uint32_t GetVID(LPCHARACTER ch)
{
    if (!ch) {
        return 0;
    }

    return ch->GetVID();
}

inline int32_t GetX(LPCHARACTER ch)
{
    if (!ch) {
        return 0;
    }

    return ch->GetX();
}

inline int32_t GetY(LPCHARACTER ch)
{
    if (!ch) {
        return 0;
    }

    return ch->GetY();
}

} // namespace ecs
