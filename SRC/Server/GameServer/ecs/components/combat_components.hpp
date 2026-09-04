#pragma once

#include <unordered_set>

#include <cstdint>
#include <unordered_map>

#include <entt/entt.hpp>

namespace ecs {

// The target the player has selected - what drives the client's target window
// and its HP bar. Distinct from CombatTarget below, which mirrors GetVictim,
// the character this one is fighting. Legacy kept these as m_pkChrTarget and
// the reverse set m_set_pkChrTargetedBy.
struct SelectedTarget {
    entt::entity target { entt::null };
};

struct SelectedBy {
    std::unordered_set<entt::entity> selectors;
};

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
