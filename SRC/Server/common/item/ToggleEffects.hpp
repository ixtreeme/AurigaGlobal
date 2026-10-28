#pragma once
#include "BonusProfiles.hpp"
#include "Consumables.hpp"
#include "ItemInstanceId.hpp"
#include <span>

namespace auriga::item
{
struct ToggleEffectGroup
{
    std::uint32_t id = 0;
    std::string code;
    bool removeOnDeath = false;
};
// New domain flags, never legacy affect/point IDs.
enum class ToggleRequirement : std::uint8_t { Married = 1, PartyLeader = 2 };
struct ToggleEffectProfile
{
    std::uint32_t id = 0;
    std::string code;
    std::uint32_t group = 0, bonusProfile = 0;
    std::uint16_t variant = 0; // Same group+variant rejects another instance; other variant replaces.
    std::uint64_t durationMs = 0; // Finite active online time, initialized once on acquisition.
    std::uint8_t requirements = 0, allowedContexts = 0;
    bool allowDuringPvp = false;
    std::optional<std::uint32_t> cooldownProfile;
};
bool ValidToggleEffectGroup(const ToggleEffectGroup& source) noexcept;
bool ValidToggleEffectProfile(const ToggleEffectProfile& source) noexcept;
struct ToggleEffectState
{
    std::uint64_t remainingMs = 0;
    bool active = false;
    auto operator<=>(const ToggleEffectState&) const = default;
};
struct ActiveToggleEffect
{
    ItemInstanceId item;
    ItemKey key;
    std::uint64_t remainingMs = 0;
};
inline constexpr std::size_t MaxActiveToggleEffects = 1024;
struct ToggleEffectAdmission
{
    ToggleEffectState state;
    std::uint32_t availableUnits = 0;
    RecoveryContext context{};
    bool alive = false, inPvp = false, inCombat = false;
    bool married = false, partyLeader = false;
    bool effectAllowedByRules = false; // Owner resolves map/global rules, fail closed.
    std::uint16_t level = 0;
    std::uint8_t classIndex = 0, sexIndex = 0;
    std::uint64_t cooldownRemainingMs = 0;
};
enum class ToggleEffectStep : std::uint8_t { Idle = 1, Activate = 2, Deactivate = 3, Advance = 4, Expire = 5 };
struct ToggleEffectPlan
{
    ItemInstanceId item;
    ToggleEffectState next;
    ToggleEffectStep step = ToggleEffectStep::Idle;
    std::optional<ItemInstanceId> deactivateOther;
    std::optional<ConsumableCooldownStart> cooldown;
    std::vector<BonusEntry> bonuses; // Owning activation contribution, not an extra tick grant.
    bool destroyItem = false;
};
enum class ToggleEffectError
{
    MissingItem, WrongCategory, WrongEffect, InvalidState, RetiredItem,
    DeadCharacter, ContextDenied, PvpDenied, CombatDenied, LevelDenied,
    ClassDenied, SexDenied, RequirementDenied, RuleDenied, OnCooldown,
    Exhausted, AlreadyActive, ActiveLimit,
};
class ItemCatalog;
std::expected<ToggleEffectState, ToggleEffectError> InitialToggleEffectState(const ItemCatalog&, ItemKey) noexcept;
// Pure proposals. Owner must atomically commit instance/revision, active binding,
// lock, bonus removal/addition, cooldown, timer and receipt. No stack splitting.
std::expected<ToggleEffectPlan, ToggleEffectError> PlanToggleEffectUse(
    const ItemCatalog&, ItemKey, ItemInstanceId, const ToggleEffectAdmission&, std::span<const ActiveToggleEffect>);
// Server-measured elapsed ACTIVE online time. Never client time or offline catch-up.
// Owner settles elapsed time before a use/transfer/logout, preventing free short toggles.
std::expected<ToggleEffectPlan, ToggleEffectError> PlanToggleEffectAdvance(
    const ItemCatalog&, ItemKey, ItemInstanceId, const ToggleEffectAdmission&, std::uint64_t elapsedMs);
}
