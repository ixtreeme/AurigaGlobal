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

// Phase 15E-final.LPENTITY.4-architect.D.1:
// PositionChangedEvent fires whenever an entity's authoritative ECS Position
// changes. Carries both the old and the new (x, y, z, mapIndex) so the
// VisibilitySystem (Phase D.4) can run a symmetric-diff against the sectree
// and incrementally maintain ViewMap / ViewerMap without polling.
//
// Map-warp case: oldMapIndex and newMapIndex differ; the handler treats
// this as a full leave-old / enter-new visibility transition.
//
// Trigger sites (planned for D.2): SpatialService::InsertEntity,
// SyncPositionComponents, MovementSystem update loop, MirrorLegacyMovement,
// SetObserverMode (synthetic event with old==new but observer flag flipped).
//
// No subscribers in D.1 - event struct lands first so D.2 can compile.
struct PositionChangedEvent {
    entt::entity entity { entt::null };
    int32_t      oldX { 0 };
    int32_t      oldY { 0 };
    int32_t      oldZ { 0 };
    int32_t      newX { 0 };
    int32_t      newY { 0 };
    int32_t      newZ { 0 };
    int32_t      oldMapIndex { 0 };
    int32_t      newMapIndex { 0 };
};

struct EvItemPickup {
    entt::entity picker { entt::null };
    uint32_t itemVnum { 0 };
};

struct EvItemEquipped {
    entt::entity charEntity { entt::null };
    entt::entity itemEntity { entt::null };
};

struct EvItemUnequipped {
    entt::entity charEntity { entt::null };
    entt::entity itemEntity { entt::null };
};

struct EvItemDestroyed {
    entt::entity itemEntity { entt::null };
    uint32_t itemID { 0 };
};

struct EvItemExpired {
    entt::entity itemEntity { entt::null };
    uint32_t itemID { 0 };
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

// War map lifecycle
struct EvWarBegin {
    uint32_t warId { 0 };
};

struct EvWarEnd {
    uint32_t warId { 0 };
};

struct EvWarTimeout {
    uint32_t warId { 0 };
};

// Dungeon lifecycle
struct EvDungeonPrepare {
    uint32_t dungeonId { 0 };
};

struct EvDungeonEnd {
    uint32_t dungeonId { 0 };
};

struct EvDungeonDead {
    uint32_t dungeonId { 0 };
};

// Session
struct EvDescDisconnect {
    uint32_t descHandle { 0 };
};

struct EvDescPing {
    uint32_t descHandle { 0 };
};

// Mount
struct EvMountSystemUpdate {
    entt::entity mountEntity { entt::null };
};

// Guild
struct EvGuildInvite {
    uint32_t guildId { 0 };
    uint32_t targetPid { 0 };
};

// Horse stamina
struct EvHorseStaminaConsume {
    entt::entity riderEntity { entt::null };
};

struct EvHorseStaminaRegen {
    entt::entity riderEntity { entt::null };
};

// Combat
struct EvStunBegin {
    entt::entity entity { entt::null };
    uint32_t durationMs { 0 };
};

struct EvCharDead {
    entt::entity killer { entt::null };
    entt::entity victim { entt::null };
};

// Affect / DOT
struct EvFireApplied {
    entt::entity entity { entt::null };
    int32_t damage { 0 };
};

struct EvPoisonApplied {
    entt::entity entity { entt::null };
    int32_t damage { 0 };
};

struct EvBleedingApplied {
    entt::entity entity { entt::null };
    int32_t damage { 0 };
};

// Character/session lifecycle
struct EvCharSaved {
    entt::entity entity { entt::null };
};

struct EvCharDisconnect {
    entt::entity entity { entt::null };
};

// Movement / recovery
struct EvWarpBegin {
    entt::entity entity { entt::null };
    uint32_t mapIndex { 0 };
    int32_t x { 0 };
    int32_t y { 0 };
};

struct EvRecovery {
    entt::entity entity { entt::null };
    int32_t hpGain { 0 };
    int32_t mpGain { 0 };
};

// Skill
struct EvSkillUsed {
    entt::entity entity { entt::null };
    uint32_t skillId { 0 };
};

} // namespace ecs
