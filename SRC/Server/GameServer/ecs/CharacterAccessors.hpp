#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "AIHelpers.hpp"
#include "Registry.hpp"
#include "VIDRegistry.hpp"
#include "components/identity_components.hpp"
#include "components/spatial_components.hpp"
#include "components/transform_components.hpp"
#include "components/vital_components.hpp"
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

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e != entt::null) {
        if (const auto* pid = g_registry.try_get<ecs::PlayerID>(e)) {
            return pid->pid;
        }
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

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e != entt::null) {
        if (const auto* placement = g_registry.try_get<ecs::SectorPlacement>(e)) {
            return placement->mapIndex;
        }

        if (const auto* mapIndex = g_registry.try_get<ecs::MapIndex>(e)) {
            return mapIndex->value;
        }
    }

    return ch->GetMapIndex();
}

inline uint32_t GetVID(LPCHARACTER ch)
{
    if (!ch) {
        return 0;
    }

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e != entt::null) {
        if (const auto* vid = g_registry.try_get<ecs::VIDComponent>(e)) {
            return vid->value;
        }
    }

    return ch->GetLegacyVID();
}

inline int32_t GetX(LPCHARACTER ch)
{
    if (!ch) {
        return 0;
    }

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e != entt::null) {
        if (const auto* pos = g_registry.try_get<ecs::Position>(e)) {
            return static_cast<int32_t>(pos->x);
        }
    }

    return ch->GetX();
}

inline int32_t GetY(LPCHARACTER ch)
{
    if (!ch) {
        return 0;
    }

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e != entt::null) {
        if (const auto* pos = g_registry.try_get<ecs::Position>(e)) {
            return static_cast<int32_t>(pos->y);
        }
    }

    return ch->GetY();
}

inline uint32_t GetRaceNum(LPCHARACTER ch)
{
    if (!ch) {
        return 0;
    }

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e != entt::null) {
        if (const auto* race = g_registry.try_get<ecs::RaceComponent>(e)) {
            return race->value;
        }
    }

    return ch->GetRaceNum();
}

inline int GetLevel(LPCHARACTER ch)
{
    if (!ch) {
        return 0;
    }

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e != entt::null) {
        if (const auto* level = g_registry.try_get<ecs::LevelComponent>(e)) {
            return static_cast<int>(level->value);
        }
    }

    return ch->GetLevel();
}

inline bool IsPC(LPCHARACTER ch)
{
    if (!ch) {
        return false;
    }

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e != entt::null) {
        if (g_registry.all_of<ecs::TagPC>(e)) {
            return true;
        }

        if (g_registry.any_of<ecs::TagNPC, ecs::TagMonster, ecs::TagStone, ecs::TagPet, ecs::TagMount, ecs::TagHorse>(e)) {
            return false;
        }
    }

    return ch->IsPC();
}

inline bool IsNPC(LPCHARACTER ch)
{
    if (!ch) {
        return false;
    }

    const entt::entity e = AIHelpers::EcsOf(ch);
    if (e != entt::null) {
        if (g_registry.all_of<ecs::TagNPC>(e)) {
            return true;
        }

        if (g_registry.any_of<ecs::TagPC, ecs::TagMonster, ecs::TagStone, ecs::TagPet, ecs::TagMount, ecs::TagHorse>(e)) {
            return false;
        }
    }

    return ch->IsNPC();
}

} // namespace ecs
