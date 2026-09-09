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
    uint32_t setTime { 0 };
};

struct CombatStats {
    uint32_t alignment { 0 };
    uint32_t realAlignment { 0 };
    uint32_t killerModePulse { 0 };
    uint8_t pkMode { 0 };
    int maxAggro { 0 };
    uint32_t killerPID { 0 };
    float attackMultiplier { 1.0f };
    float damageMultiplier { 1.0f };
    uint64_t alignmentRevision { 0 };
};

struct AttackCooldown {
    uint32_t lastAttackTime;
    uint32_t skipComboAttackByTime;
    uint8_t comboSequence;
    uint32_t lastComboTime;
    int validComboInterval;
    uint8_t comboIndex;
    int comboHackCount;
    // The prototype combat updater runs in pulses, not get_dword_time() ms.
    uint32_t lastCombatPulse { 0 };
};

// Generation-bearing handles prevent a recycled VID/PID from inheriting a hit.
struct AttackAudit {
    entt::entity target { entt::null };
    entt::entity attacker { entt::null };
    uint32_t attackTime { 0 };
    uint32_t attackedTime { 0 };
    int speedHackCount { 0 };
};

struct SkillHitState {
    bool value { false };
};

struct DamageMap {
    std::unordered_map<uint32_t, int32_t> entries;
};

// The stone a mob was spawned from. CHARACTER kept this as m_pkChrStone with
// no component, so nothing entity-native could ask who a mob is guarding.
struct StoneOwner {
    entt::entity stone { entt::null };
};

// When this mob last repositioned around its target. One field, but it was
// the only thing keeping IsChangeAttackPosition on CHARACTER.
struct AttackPositionTimer {
    uint32_t lastChange { 0 };
};

struct CombatActiveTag {};
struct InvincibleTag {};

} // namespace ecs
