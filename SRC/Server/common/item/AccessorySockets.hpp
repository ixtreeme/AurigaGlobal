#pragma once
#include "ItemKey.hpp"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace auriga::item
{
inline constexpr std::size_t MaxAccessorySocketMaterials = 256;
struct AccessorySocketGrade
{
    std::uint8_t grade = 0;
    std::uint64_t durationMs = 0; // Consumed only while equipped; no UTC sentinel.
    std::uint32_t bonusIncreaseBp = 0;
    std::int64_t minimumRaw = 0;
    std::int32_t minimumBp = 0, minimumInteger = 0;
    // Chance of the NEXT operation from this capacity/active-grade index.
    std::uint16_t unlockChanceBp = 0, chargeChanceBp = 0;
};
struct AccessorySocketProfile
{
    std::uint32_t id = 0;
    std::string code;
    std::uint8_t maxGrade = 0;
    std::uint16_t unlockUnitsPerAttempt = 0, chargeUnitsPerAttempt = 0;
    std::vector<AccessorySocketGrade> grades;
    std::vector<ItemKey> unlockMaterials, chargeMaterials;
    std::vector<ItemKey> permanentChargeMaterials; // Optional; disjoint from timed charges.
};
enum class AccessorySocketError
{
    InvalidIdentity, InvalidMaximum, InvalidUnits, IncompleteGrades, InvalidGrade,
    InvalidTimer, InvalidBonus, NonzeroGradeZero, InvalidChance, NonMonotonicBonus,
    InvalidCompatibility, DuplicateMaterial, TooManyMaterials, InvalidState,
    MaterialNotAllowed, CapacityFull, NoOpenCapacity, InvalidDecision, OperationDisabled, LifetimeMismatch,
};
class ValidatedAccessorySocketProfile;
std::expected<ValidatedAccessorySocketProfile, AccessorySocketError> ValidateAccessorySocketProfile(AccessorySocketProfile source);
class ValidatedAccessorySocketProfile final
{
public:
    const AccessorySocketProfile& Data() const noexcept { return data_; }
    const AccessorySocketGrade* FindGrade(std::uint8_t grade) const noexcept
    {
        return grade <= data_.maxGrade && grade < data_.grades.size() ? &data_.grades[grade] : nullptr;
    }
private:
    explicit ValidatedAccessorySocketProfile(AccessorySocketProfile data) : data_(std::move(data)) {}
    friend std::expected<ValidatedAccessorySocketProfile, AccessorySocketError> ValidateAccessorySocketProfile(AccessorySocketProfile);
    AccessorySocketProfile data_;
};
struct AccessorySocketState
{
    std::uint8_t unlockedCapacity = 0, activeGrade = 0;
    std::uint64_t remainingMs = 0;
    bool permanent = false; // Active grades only; remainingMs must then be zero.
    bool operator==(const AccessorySocketState&) const = default;
};
enum class SocketAttemptOutcome : std::uint8_t { Succeeded = 1, Failed = 2 };
struct AccessorySocketPlan
{
    AccessorySocketState state;
    std::uint16_t materialUnits = 0;
    bool succeeded = false;
};
struct AccessorySocketTimeResult
{
    AccessorySocketState state;
    std::uint16_t gradesExpired = 0;
};
std::expected<void, AccessorySocketError> ValidateAccessorySocketState(
    const ValidatedAccessorySocketProfile& profile, AccessorySocketState state) noexcept;
// Pure proposals only: no RNG, ownership admission, inventory, SQL or commit.
// The future server writer supplies a recorded economic outcome and atomically
// commits the item, material, revision and operation receipt together.
std::expected<AccessorySocketPlan, AccessorySocketError> PlanAccessorySocketUnlock(
    const ValidatedAccessorySocketProfile& profile, AccessorySocketState state, ItemKey material, SocketAttemptOutcome outcome) noexcept;
std::expected<AccessorySocketPlan, AccessorySocketError> PlanAccessorySocketCharge(
    const ValidatedAccessorySocketProfile& profile, AccessorySocketState state, ItemKey material, SocketAttemptOutcome outcome) noexcept;
std::expected<AccessorySocketTimeResult, AccessorySocketError> AdvanceAccessorySocketTime(
    const ValidatedAccessorySocketProfile& profile, AccessorySocketState state, std::uint64_t elapsedMs, bool equipped) noexcept;
} // namespace auriga::item
