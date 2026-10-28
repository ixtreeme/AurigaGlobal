#pragma once
#include "BonusProfiles.hpp"
#include "Consumables.hpp"
#include <span>

namespace auriga::item
{
enum class TimedEffectReapply : std::uint8_t { Reject = 1, Replace = 2 };
enum class TimedEffectClock : std::uint8_t { OnlineTime = 1, WallTime = 2 };
struct TimedEffectGroup
{
    std::uint32_t id = 0;
    std::string code;
    TimedEffectReapply reapply{};
    TimedEffectClock clock{};
    bool removeOnDeath = false;
};
struct TimedEffectProfile
{
    std::uint32_t id = 0;
    std::string code;
    std::uint32_t group = 0, bonusProfile = 0;
    std::uint64_t durationMs = 0;
    std::uint16_t unitsPerUse = 0;
    std::uint8_t allowedContexts = 0; // RecoveryContext bits, not legacy map IDs
    bool allowDuringPvp = false;
    std::optional<std::uint32_t> cooldownProfile;
};
bool ValidTimedEffectGroup(const TimedEffectGroup& source) noexcept;
bool ValidTimedEffectProfile(const TimedEffectProfile& source) noexcept;

// Owner-normalized remaining duration, using the group's declared clock.
// This module reads no wall clock and cannot access an ECS registry.
struct ActiveTimedEffect
{
    std::uint64_t effectId = 0;
    std::uint32_t group = 0;
    std::uint64_t remainingMs = 0;
};
inline constexpr std::size_t MaxActiveTimedEffects = 1024;
struct TimedEffectAdmission
{
    std::uint32_t availableUnits = 0;
    std::uint64_t cooldownRemainingMs = 0;
    RecoveryContext context{};
    bool alive = false, inPvp = false, inCombat = false;
    std::uint16_t level = 0;
    std::uint8_t classIndex = 0, sexIndex = 0;
};
struct TimedEffectPlan
{
    std::uint32_t profileId, group, bonusProfile;
    std::uint64_t durationMs;
    TimedEffectClock clock;
    bool removeOnDeath;
    std::uint16_t unitsToConsume;
    std::optional<std::uint64_t> replaceEffectId;
    std::optional<ConsumableCooldownStart> cooldown;
    std::vector<BonusEntry> bonuses; // Owning, no snapshot pointer escapes.
};
enum class TimedEffectUseError
{
    MissingItem, WrongCategory, WrongEffect, RetiredItem, InvalidState,
    DeadCharacter, ContextDenied, PvpDenied, CombatDenied, LevelDenied,
    ClassDenied, SexDenied, OnCooldown, InsufficientUnits, AlreadyActive,
    ActiveLimit,
};
class ItemCatalog;
// Pure proposal only. Owner/lock/revision, timer reservation, replacement,
// consumption, persistence and stat recomputation require one later commit.
std::expected<TimedEffectPlan, TimedEffectUseError> PlanTimedEffectUse(
    const ItemCatalog& catalog, ItemKey item, const TimedEffectAdmission& state,
    std::span<const ActiveTimedEffect> active);
} // namespace auriga::item
