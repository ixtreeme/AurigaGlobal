#pragma once

#include <cstdint>

#include <entt/entt.hpp>

#include "../char.h"

namespace ecs {

struct EvEntityDied {
    entt::entity killer { entt::null };
    entt::entity victim { entt::null };
};

struct EvEntityDamaged {
    entt::entity attacker { entt::null };
    entt::entity target { entt::null };
    int32_t damage { 0 };
    EDamageType type { DAMAGE_TYPE_NONE };
};

struct EvEntityMoved {
    entt::entity entity { entt::null };
    int32_t newX { 0 };
    int32_t newY { 0 };
};

struct EvItemPickup {
    entt::entity picker { entt::null };
    uint32_t itemVnum { 0 };
};

struct EvLevelUp {
    entt::entity entity { entt::null };
    int32_t newLevel { 0 };
};

struct EvExperienceChanged {
    entt::entity entity { entt::null };
    int64_t amount { 0 };
};

struct EvAffectApplied {
    entt::entity target { entt::null };
    uint32_t affectType { 0 };
};

struct EvAffectExpired {
    entt::entity target { entt::null };
    uint32_t affectType { 0 };
};

} // namespace ecs
