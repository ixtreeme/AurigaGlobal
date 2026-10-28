#pragma once
#include "ItemKey.hpp"
#include <cstdint>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace auriga::item
{
// Skill IDs name the separate skill domain, never an item vnum or ItemKey.
inline constexpr std::uint16_t MaxBookSkillId = 254;
inline constexpr std::uint8_t MaxBookSkillLevel = 40;
inline constexpr std::size_t MaxSkillBookTargets = 254, MaxSkillBookSteps = 40;
enum class SkillBookOperation : std::uint8_t { Learn = 1, Forget = 2, HorsePoint = 3 };
enum class SkillBookTargetMode : std::uint8_t { Fixed = 1, InstanceBound = 2, None = 3 };
enum class SkillBookBonus : std::uint8_t { Ignore = 1, ApplyAndConsume = 2 };
enum class SkillPointPool : std::uint8_t { None = 0, Main = 1, Secondary = 2, Horse = 3 };
struct SkillBookTarget
{
    std::uint16_t skillId = 0;
    SkillPointPool refundPool{};
    std::uint8_t forbiddenEmpire = 0; // 0 none; 1..3 prohibits that native language.
};
struct SkillBookStep
{
    std::uint8_t currentLevel = 0, nextLevel = 0;
    std::uint16_t minimumCharacterLevel = 0, requiredSuccessfulReads = 0;
    std::uint16_t successChanceBp = 0, boostedChanceBp = 0;
    std::uint64_t experienceCost = 0, delayMinimumMs = 0, delayMaximumMs = 0;
    bool delayOnFailure = false;
};
struct SkillBookProfile
{
    std::uint32_t id = 0;
    std::string code;
    SkillBookOperation operation{};
    SkillBookTargetMode targetMode{};
    std::uint16_t unitsPerUse = 0;
    std::uint8_t allowedMasteries = 0; // Normal/Master/Grandmaster bits 0..2; perfect cannot increase.
    bool allowDelayBypass = false;
    SkillBookBonus bonus{};
    std::vector<SkillBookTarget> targets;
    std::vector<SkillBookStep> steps;
};
bool ValidSkillBookProfile(const SkillBookProfile&) noexcept;
struct BookSkillState
{
    std::uint16_t skillId = 0;
    std::uint8_t level = 0;
    std::uint16_t successfulReads = 0;
};
struct BookHorseState
{
    std::uint8_t unspentPoints = 0, allocatedMeleePoints = 0, allocatedRangedPoints = 0;
};
struct SkillBookAdmission
{
    std::optional<std::uint16_t> boundSkillId; // Trusted instance payload, not a client target.
    std::optional<BookSkillState> skill;
    std::optional<BookHorseState> horse;
    std::uint32_t availableUnits = 0, refundPointBalance = 0;
    std::uint64_t experience = 0, cooldownRemainingMs = 0;
    std::uint16_t characterLevel = 0;
    std::uint8_t classIndex = 0, sexIndex = 0, empire = 0;
    bool alive = false, polymorphed = false, inCombat = false;
    // Owner checks skill existence/max level, job/group, prerequisites, global/map rules.
    // Missing owner decisions deny admission; the isolated catalog is not a skill catalog.
    bool skillSystemEnabled = false, targetAllowedByRules = false;
    bool delayBypassAvailable = false, chanceBonusAvailable = false;
};
enum class SkillBookOutcome : std::uint8_t { Failed = 1, Progress = 2, Learned = 3, Forgotten = 4, HorsePoint = 5 };
struct SkillBookPlan
{
    ItemKey item;
    std::uint32_t profileId;
    std::uint64_t catalogRevision;
    SkillBookOperation operation;
    std::optional<std::uint16_t> skillId; // Absent for the horse training point pool.
    std::uint8_t previousLevel = 0, nextLevel = 0;
    std::uint16_t previousReadCount = 0, nextReadCount = 0;
    SkillBookOutcome outcome = SkillBookOutcome::Failed;
    std::uint16_t unitsToConsume = 0;
    std::uint64_t experienceToConsume = 0;
    std::optional<std::uint64_t> nextReadInMs;
    bool consumeDelayBypass = false, consumeChanceBonus = false;
    SkillPointPool pointPool = SkillPointPool::None;
    std::uint8_t pointsGranted = 0;
};
struct SkillBookDecisions
{
    // Owner-generated and receipt-recorded. Roll is 1..10000, or exactly zero
    // when chance is 0/10000. No hidden RNG, clock, registry or SQL in this module.
    std::uint16_t successRollBp = 0;
    std::uint64_t readDelayMs = 0;
};
enum class SkillBookError
{
    MissingItem, WrongCategory, RetiredItem, InvalidState, Disabled, RuleDenied,
    DeadCharacter, Polymorphed, CombatDenied, LevelDenied, ClassDenied, SexDenied,
    TargetDenied, NativeLanguage, MasteryDenied, LevelNotCovered, InsufficientUnits,
    InsufficientExperience, OnCooldown, PointLimit, InvalidDecisions,
};
class ItemCatalog;
class PreparedSkillBookUse;
std::expected<PreparedSkillBookUse, SkillBookError> PrepareSkillBookUse(const ItemCatalog&, ItemKey, const SkillBookAdmission&);
std::expected<SkillBookPlan, SkillBookError> ResolveSkillBookUse(const PreparedSkillBookUse&, SkillBookDecisions) noexcept;
// Only successful admission can construct this immutable, owning request.
class PreparedSkillBookUse final
{
public:
    std::uint16_t SuccessChanceBp() const noexcept { return chance_; }
    bool NeedsSuccessRoll() const noexcept { return chance_ > 0 && chance_ < 10000; }
    std::uint64_t MinimumDelayMs() const noexcept { return step_.delayMinimumMs; }
    std::uint64_t MaximumDelayMs() const noexcept { return step_.delayMaximumMs; }
private:
    PreparedSkillBookUse(SkillBookPlan plan, SkillBookStep step, std::uint16_t chance)
        : plan_(std::move(plan)), step_(step), chance_(chance) {}
    SkillBookPlan plan_;
    SkillBookStep step_;
    std::uint16_t chance_;
    friend std::expected<PreparedSkillBookUse, SkillBookError> PrepareSkillBookUse(const ItemCatalog&, ItemKey, const SkillBookAdmission&);
    friend std::expected<SkillBookPlan, SkillBookError> ResolveSkillBookUse(const PreparedSkillBookUse&, SkillBookDecisions) noexcept;
};
// Commit item/skill/progress/point/XP/cooldown/token changes with one owner
// revision+receipt. Preparation/resolution performs no mutation or persistence.
}
