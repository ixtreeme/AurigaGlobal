#pragma once

#include <entt/entt.hpp>

#include "components/identity_components.hpp"
#include "components/spatial_components.hpp"
#include "components/transform_components.hpp"
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

inline void ValidateCommonIdentity(entt::registry& reg, entt::entity e, const char* context)
{
    if (e == entt::null || !reg.valid(e))
        return;

    const char* ctx = context ? context : "unknown";

    if (!reg.all_of<ecs::VIDComponent>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing VIDComponent", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::PlayerName>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing PlayerName", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::RaceComponent>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing RaceComponent", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::RaceState>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing RaceState", static_cast<uint32_t>(e), ctx);
}

inline void ValidatePCIdentity(entt::registry& reg, entt::entity e, const char* context)
{
    if (e == entt::null || !reg.valid(e))
        return;

    const char* ctx = context ? context : "unknown";
    ValidateCommonIdentity(reg, e, ctx);

    if (!reg.all_of<ecs::PlayerID>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing PlayerID", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::AccountID>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing AccountID", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::EmpireComponent>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing EmpireComponent", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::GMLevel>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing GMLevel", static_cast<uint32_t>(e), ctx);
}

inline bool HasMatchingSpatialKind(entt::registry& reg, entt::entity e, ecs::SpatialKind expected)
{
    if (e == entt::null || !reg.valid(e))
        return false;

    const auto* kind = reg.try_get<ecs::SpatialKindTag>(e);
    return kind && kind->kind == expected;
}

inline void ValidateSpatialCoverage(entt::registry& reg, entt::entity e, const char* context)
{
    if (e == entt::null || !reg.valid(e))
        return;

    const char* ctx = context ? context : "unknown";

    if (!reg.all_of<ecs::SpatialEntity>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing SpatialEntity", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::SpatialKindTag>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing SpatialKindTag", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::Position>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing Position", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::PositionZ>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing PositionZ", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::MapIndex>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing MapIndex", static_cast<uint32_t>(e), ctx);
    if (!reg.all_of<ecs::VIDComponent>(e))
        LOG_WARN("[ECS_INVARIANT] entity={} ctx={} missing VIDComponent", static_cast<uint32_t>(e), ctx);
}

} // namespace ecs::Invariants
