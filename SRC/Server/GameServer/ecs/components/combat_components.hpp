#pragma once

#include <map>
#include <unordered_set>
#include <vector>

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

// The metin stone this monster drops when it dies, and how likely it is.
// Rolled once when the monster is set up. These were two CHARACTER fields.
struct MetinStoneDrop {
    uint32_t vnum { 0 };
    uint8_t pct { 0 };
};

// Who has hurt this character and by how much. The killer, the experience
// split and the drop ownership are all decided from it, and death clears it.
// This was CHARACTER::m_map_kDamage with two accessors returning the map
// itself, so every caller reached straight into the field.
struct BattleContribution {
    uint64_t totalDamage { 0 };
    int aggro { 0 };
};

struct DamageLedger {
    std::map<entt::entity, BattleContribution> entries;
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
};

// Generation-bearing handles prevent a recycled VID/PID from inheriting a hit.
// Until when combo attacks are skipped. The setter was removed as dead code
// earlier, so this is zero for everyone; the read stays because the check is
// on the attack path and dropping it would be a behaviour change, not a
// cleanup.
struct ComboSkipUntil {
    uint32_t value { 0 };
};

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

// The mobs a stone spawned. This was m_set_pkChrSpawnedBy, a raw pointer set
// that only the stone itself could read, and that nothing kept in step when a
// mob left one stone for another.
struct StoneSpawns {
    std::vector<entt::entity> members;
};

// When this mob last repositioned around its target. One field, but it was
// the only thing keeping IsChangeAttackPosition on CHARACTER.
struct AttackPositionTimer {
    uint32_t lastChange { 0 };
};

// The highest aggro this character has seen. It gates every victim switch,
// and it starts below zero so the first attacker always takes the slot.
struct AggroState {
    int maxAggro { -100 };
};

// Boss invincibility windows. It was a bare bool on CHARACTER that only the
// dungeon scripts and the damage path ever touched.
struct InvincibleState {
    bool value { false };
};

// The GM "armada" flag: HP can fall but death never fires.
// Who landed the killing blow, carried from the damage that did it to the
// death that follows. Cleared as the death is processed.
// Whether the last death came from a monster rather than a player. The
// respawn heal reads it to decide how much HP to give back.
// When this character last died, and when its guild was last told that a
// war kill earns no reward. Both were CHARACTER members that only the
// character itself could read; a zero default reads the same as the
// backdated values Initialize used to write.
struct LastDeadTime {
    uint32_t value { 0 };
};

struct GuildWarNoticeTime {
    uint32_t value { 0 };
};

struct DeadByMonster {
    bool value { false };
};

struct KillerPID {
    uint32_t value { 0 };
};

struct UndyingState {
    bool value { false };
};

struct CombatActiveTag {};
struct InvincibleTag {};

} // namespace ecs
