#pragma once
#include "Consumables.hpp"
#include "ItemInstanceId.hpp"

namespace auriga::item
{
struct AutomaticRecoveryState
{
    std::int64_t remainingRaw = 0; // Unlimited supply stores exactly zero.
    bool active = false;
    std::uint64_t nextTickInMs = 0; // Inactive: zero; active: 1..interval.
    auto operator<=>(const AutomaticRecoveryState&) const = default;
};
struct AutomaticRecoveryAdmission
{
    RecoveryPools pools;
    AutomaticRecoveryState state;
    std::optional<ItemInstanceId> activeForPool;
    std::uint32_t availableUnits = 0; // A charged instance must be unstacked.
    RecoveryContext context{};
    bool inPvp = false, inCombat = false, stunned = false;
    std::uint16_t level = 0;
    std::uint8_t classIndex = 0, sexIndex = 0;
    std::uint64_t cooldownRemainingMs = 0;
    bool recoveryAllowedByRules = false; // Owner resolves global/map/duel rules; fail closed.
};
struct AutomaticRecoveryTogglePlan
{
    ItemInstanceId item;
    RecoveryPool pool;
    AutomaticRecoveryState next;
    std::optional<ItemInstanceId> deactivateOther;
    std::optional<ConsumableCooldownStart> cooldown;
    std::uint16_t activeMaxPoolBonusBp = 0;
};
struct AutomaticRecoveryTickAdmission
{
    AutomaticRecoveryAdmission actor;
    std::int64_t pendingHpRaw = 0, pendingSpRaw = 0;
    std::uint64_t elapsedMs = 0; // Server monotonic elapsed time, never client time.
};
enum class AutomaticRecoveryStep : std::uint8_t { Idle = 1, Recover = 2, Deactivate = 3, Depleted = 4 };
struct AutomaticRecoveryTickPlan
{
    ItemInstanceId item;
    RecoveryPool pool;
    AutomaticRecoveryState next;
    AutomaticRecoveryStep step = AutomaticRecoveryStep::Idle;
    std::int64_t hpRestoredRaw = 0, spRestoredRaw = 0;
    bool destroyItem = false;
};
enum class AutomaticRecoveryError
{
    MissingItem, WrongCategory, WrongDelivery, RetiredItem, InvalidState,
    DeadCharacter, Stunned, ContextDenied, PvpDenied, CombatDenied, LevelDenied,
    ClassDenied, SexDenied, OnCooldown, Exhausted, RuleDenied,
};
class ItemCatalog;
// Initial state for one newly acquired instance, not a prototype mutation.
std::expected<AutomaticRecoveryState, AutomaticRecoveryError> InitialAutomaticRecoveryState(
    const ItemCatalog& catalog, ItemKey key) noexcept;
// Pure proposals. The future owner must commit item/HP/timer/bonus/replacement/
// cooldown/receipt atomically. No registry, SQL, clock, RNG or live side effects.
std::expected<AutomaticRecoveryTogglePlan, AutomaticRecoveryError> PlanAutomaticRecoveryToggle(
    const ItemCatalog& catalog, ItemKey key, ItemInstanceId item, const AutomaticRecoveryAdmission& state) noexcept;
std::expected<AutomaticRecoveryTickPlan, AutomaticRecoveryError> PlanAutomaticRecoveryTick(
    const ItemCatalog& catalog, ItemKey key, ItemInstanceId item, const AutomaticRecoveryTickAdmission& state) noexcept;
}
