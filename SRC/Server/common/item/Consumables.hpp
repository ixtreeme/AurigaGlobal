#pragma once
#include "ItemKey.hpp"
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace auriga::item
{
// New domain codes, never casts of USE_* or POINT_* values.
enum class RecoveryContext : std::uint8_t { World = 1, Duel = 2, Arena = 4, GuildWar = 8 };
enum class RecoveryDelivery : std::uint8_t { Instant = 1, Reserve = 2, Periodic = 3 };
struct ConsumableCooldownProfile
{
    std::uint32_t id = 0;
    std::string code;
    std::uint64_t durationMs = 0;
};
struct InstantRecovery
{
    std::int64_t hpRaw = 0, spRaw = 0;
    std::uint16_t maxHpBp = 0, maxSpBp = 0;
    std::uint32_t flatBonusCapBp = 0; // Only the flat part receives potion bonus.
};
struct ReserveRecovery
{
    std::int64_t hpRaw = 0, spRaw = 0;
    std::uint32_t flatBonusCapBp = 0;
    std::uint64_t tickIntervalMs = 0;
    std::uint16_t maxPoolPerTickBp = 0;
};
struct PeriodicRecovery
{
    std::int64_t hpPerTickRaw = 0, spPerTickRaw = 0; // Exactly one pool.
    std::uint64_t tickIntervalMs = 0, durationMs = 0;
    // New application replaces the actor's continuous-recovery group for this
    // pool, including another profile's rate/duration; it never stacks by item.
    // Persisted scheduling and group ownership are not implemented here.
};
using RecoveryEffect = std::variant<InstantRecovery, ReserveRecovery, PeriodicRecovery>;
struct RecoveryProfile
{
    std::uint32_t id = 0;
    std::string code;
    std::uint16_t unitsPerUse = 0;
    std::uint8_t allowedContexts = 0;
    bool allowDuringPvp = false;
    std::optional<std::uint32_t> cooldownProfile;
    RecoveryEffect effect;
    RecoveryDelivery Delivery() const noexcept { return static_cast<RecoveryDelivery>(effect.index() + 1); }
};
enum class RecoveryProfileError { InvalidIdentity, InvalidUnits, InvalidContexts, InvalidCooldown, InvalidAmount, EmptyEffect, InvalidInterval, InvalidDuration, MultiplePools };
class ValidatedRecoveryProfile;
std::expected<ValidatedRecoveryProfile, RecoveryProfileError> ValidateRecoveryProfile(RecoveryProfile source);
bool ValidConsumableCooldown(const ConsumableCooldownProfile& source) noexcept;
class ValidatedRecoveryProfile final
{
public:
    const RecoveryProfile& Data() const noexcept { return data_; }
private:
    explicit ValidatedRecoveryProfile(RecoveryProfile source) : data_(std::move(source)) {}
    friend std::expected<ValidatedRecoveryProfile, RecoveryProfileError> ValidateRecoveryProfile(RecoveryProfile);
    RecoveryProfile data_;
};
struct RecoveryPools
{
    std::int64_t hpRaw = 0, maxHpRaw = 0, spRaw = 0, maxSpRaw = 0;
};
struct InstantRecoveryAdmission
{
    RecoveryPools pools;
    std::int32_t potionBonusBp = 0; // Structural range [-10000,100000].
    std::uint64_t cooldownRemainingMs = 0;
    RecoveryContext context{};
    bool inPvp = false, inCombat = false;
    std::uint16_t level = 0;
    std::uint8_t classIndex = 0, sexIndex = 0;
    std::uint32_t availableUnits = 0;
};
struct ConsumableCooldownStart { std::uint32_t profileId; std::uint64_t durationMs; };
struct InstantRecoveryPlan
{
    std::int64_t hpRestoredRaw = 0, spRestoredRaw = 0;
    std::uint16_t unitsToConsume = 0;
    std::optional<ConsumableCooldownStart> cooldown;
};
enum class RecoveryUseError
{
    MissingItem, WrongCategory, WrongDelivery, RetiredItem, InvalidState, DeadCharacter,
    ContextDenied, PvpDenied, CombatDenied, LevelDenied, ClassDenied, SexDenied,
    OnCooldown, InsufficientUnits, NoEffect,
};
class ItemCatalog;
// Pure server-input proposal. Owner/lock/revision, map-specific quota and actual
// resource/HP/cooldown commit + operation receipt belong to the future writer.
std::expected<InstantRecoveryPlan, RecoveryUseError> PlanInstantRecovery(
    const ItemCatalog& catalog, ItemKey item, const InstantRecoveryAdmission& state) noexcept;
} // namespace auriga::item
