#pragma once

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

namespace ecs {

struct CombatTarget {
    entt::entity target { entt::null };
    uint32_t setTime;
};

struct CombatStats {
    uint32_t alignment;
    uint32_t realAlignment;
    int killerModePulse;
    uint8_t pkMode;
    int maxAggro;
    uint32_t killerPID;
};

struct AttackCooldown {
    uint32_t lastAttackTime;
    uint32_t skipComboAttackByTime;
    uint8_t comboSequence;
    uint32_t lastComboTime;
    int validComboInterval;
    uint8_t comboIndex;
    int comboHackCount;
};

struct DamageMap {
    std::unordered_map<uint32_t, int32_t> entries;
};

struct CombatActiveTag {};
struct InvincibleTag {};

} // namespace ecs
