#pragma once
#include "ItemKey.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace auriga::item
{
enum class PolymorphKind : std::uint8_t { Orb = 1, PracticeBook = 2 };
enum class PolymorphBonus : std::uint8_t { None = 1, Attack = 2, Defense = 3, Movement = 4 };
enum class PolymorphBonusUnit : std::uint8_t { None = 1, BasisPoints = 2, MovementPoints = 3 };
struct PolymorphForm
{
    std::uint32_t id = 0;
    std::string code;
    std::uint32_t mobTemplate = 0; // Mob namespace, never an old item identifier.
    PolymorphBonus bookBonus = PolymorphBonus::None;
};
struct PolymorphSkillRule
{
    std::uint8_t skillLevel = 0;
    std::uint64_t durationMs = 0;
    std::uint32_t attackBonusBp = 0;
    std::uint16_t powerBp = 0;
};
struct PolymorphMasteryRule
{
    std::uint8_t mastery = 0;
    std::uint64_t durationMs = 0, bonusDurationMs = 0;
};
struct PolymorphProfile
{
    std::uint32_t id = 0;
    std::string code;
    PolymorphKind kind{};
    std::uint16_t mobLevelGrace = 0, graceLossNumerator = 0, graceLossDenominator = 1;
    std::uint16_t maxBookGrade = 0;
    std::uint32_t practicePerGrade = 0;
    std::int16_t bookChanceBasePercent = 0;
    std::vector<PolymorphSkillRule> skills; // Exact 0..40; no implicit formula/lookup fallback.
    std::vector<PolymorphMasteryRule> masteries; // Books: exact 0..3; orbs: empty.
};
bool ValidPolymorphProfile(const PolymorphProfile&) noexcept;
struct ItemPolymorphRow
{
    ItemKey key;
    std::uint32_t profile = 0;
    std::optional<std::uint32_t> fixedForm; // NULL: form is owned by the concrete instance.
};
struct PolymorphInstanceState
{
    std::optional<std::uint32_t> boundForm;
    std::uint16_t bookGrade = 0;
    std::uint32_t practiceRemaining = 0;
    bool operator==(const PolymorphInstanceState&) const = default;
};
enum class PolymorphAction : std::uint8_t { Use = 1, UpgradeBook = 2 };
struct PolymorphRequest
{
    ItemKey item;
    PolymorphInstanceState state;
    std::uint64_t instanceId = 0, revision = 0, ownerId = 0, actorId = 0, rulesetRevision = 0;
    std::array<std::uint8_t,16> operationId{};
    std::uint32_t count = 0, mobTemplate = 0;
    std::uint64_t mobCatalogRevision = 0, nowUtcMs = 0;
    std::uint16_t characterLevel = 0, mobLevel = 0, refinementLevel = 0, recordedRoll = 0;
    std::uint8_t characterClass = 0, sex = 0, skillLevel = 0, mastery = 0;
    PolymorphAction action{};
    bool admittedByServer = false, formAllowedByServer = false, inCombat = false;
    bool locked = false, exchanging = false, equipped = false, riding = false, alreadyPolymorphed = false, duelBlocksPolymorph = false;
};
struct PolymorphEffectPlan
{
    std::uint32_t formId = 0, mobTemplate = 0;
    std::uint16_t powerBp = 0;
    std::uint64_t expiresAtUtcMs = 0, bonusExpiresAtUtcMs = 0;
    PolymorphBonus bonus = PolymorphBonus::None;
    PolymorphBonusUnit bonusUnit = PolymorphBonusUnit::None;
    std::uint32_t bonusMagnitude = 0;
};
struct PolymorphPlan
{
    PolymorphRequest expected;
    PolymorphInstanceState after;
    std::optional<PolymorphEffectPlan> effect;
    std::uint32_t consumeUnits = 0;
    std::uint16_t chanceBp = 0;
    bool success = false;
    std::uint64_t catalogRevision = 0;
    std::array<std::uint8_t,32> catalogHash{};
};
enum class PolymorphError
{
    Forbidden,InvalidOperation,InvalidSnapshot,MissingItem,NotPolymorph,RetiredItem,
    InvalidStack,InvalidState,InvalidLevel,MissingForm,MobMismatch,WrongAction,
    AtMaximum,NotReady,InvalidDecision,TimeOverflow,MobLevelTooHigh,
};
class ItemCatalog;
// Pure owning plan. The owner supplies an authenticated instance/actor and mob
// snapshot, rechecks both catalog pins and exact revisions, then atomically
// receipts consumption/book progress + effect application. No SQL, ECS or RNG.
// Unknown/blank forms never consume an orb. CommitUnknown requires durable
// operation-ID reconciliation; callbacks must not run before successful commit.
std::expected<PolymorphPlan,PolymorphError> PlanPolymorph(const ItemCatalog&,const PolymorphRequest&);
}
