#pragma once

#include <entt/entt.hpp>

#include "components/identity_components.hpp"
#include <Core/Logging.hpp>

namespace ecs::Invariants {

inline bool HasAnyTypeTag(entt::registry& reg, entt::entity e)
{
    return e != entt::null
        && reg.valid(e)
        && reg.any_of<ecs::TagPC, ecs::TagNPC, ecs::TagMonster, ecs::TagStone>(e);
}

inline const char* DescribeTypeTag(entt::registry& reg, entt::entity e)
{
    if (e == entt::null || !reg.valid(e))
        return "UNKNOWN";

    if (reg.all_of<ecs::TagPC>(e))
        return "PC";
    if (reg.all_of<ecs::TagNPC>(e))
        return "NPC";
    if (reg.all_of<ecs::TagMonster>(e))
        return "Mob";
    if (reg.all_of<ecs::TagStone>(e))
        return "Stone";

    return "UNKNOWN";
}

inline void ValidateCharacterTags(entt::registry& reg, entt::entity e, const char* context)
{
    if (e == entt::null || !reg.valid(e))
        return;

    if (!HasAnyTypeTag(reg, e)) {
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} has NO type tag (TagPC/TagNPC/TagMonster/TagStone)",
            static_cast<uint32_t>(e), context ? context : "unknown");
    }
}

} // namespace ecs::Invariants
