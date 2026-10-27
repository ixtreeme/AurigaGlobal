#include <item/AccessorySockets.hpp>
#include <item/RefinementProfiles.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <limits>

namespace auriga::item
{
namespace
{
std::expected<void, AccessorySocketError> Materials(std::vector<ItemKey>& keys, bool allowEmpty = false)
{
    if (keys.size() > MaxAccessorySocketMaterials) return std::unexpected(AccessorySocketError::TooManyMaterials);
    if (keys.empty() && !allowEmpty) return std::unexpected(AccessorySocketError::InvalidCompatibility);
    std::ranges::sort(keys);
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        if (keys[i].Category() != ItemCategory::Material) return std::unexpected(AccessorySocketError::InvalidCompatibility);
        if (i && keys[i] == keys[i-1]) return std::unexpected(AccessorySocketError::DuplicateMaterial);
    }
    return {};
}
std::expected<void, AccessorySocketError> Decision(std::uint16_t chance, SocketAttemptOutcome outcome) noexcept
{
    if (outcome != SocketAttemptOutcome::Succeeded && outcome != SocketAttemptOutcome::Failed)
        return std::unexpected(AccessorySocketError::InvalidDecision);
    if (!chance) return std::unexpected(AccessorySocketError::OperationDisabled);
    if (chance == 10000 && outcome != SocketAttemptOutcome::Succeeded)
        return std::unexpected(AccessorySocketError::InvalidDecision);
    return {};
}
}
std::expected<ValidatedAccessorySocketProfile, AccessorySocketError> ValidateAccessorySocketProfile(AccessorySocketProfile source)
{
    if (!source.id || !detail::ContentCode(source.code)) return std::unexpected(AccessorySocketError::InvalidIdentity);
    if (!source.maxGrade) return std::unexpected(AccessorySocketError::InvalidMaximum);
    if (!source.unlockUnitsPerAttempt || !source.chargeUnitsPerAttempt) return std::unexpected(AccessorySocketError::InvalidUnits);
    if (source.grades.size() != std::size_t(source.maxGrade) + 1) return std::unexpected(AccessorySocketError::IncompleteGrades);
    if (auto result = Materials(source.unlockMaterials); !result) return std::unexpected(result.error());
    if (auto result = Materials(source.chargeMaterials); !result) return std::unexpected(result.error());
    if (auto result = Materials(source.permanentChargeMaterials, true); !result) return std::unexpected(result.error());
    for (auto key : source.permanentChargeMaterials)
        if (std::ranges::binary_search(source.chargeMaterials, key)) return std::unexpected(AccessorySocketError::InvalidCompatibility);
    std::ranges::sort(source.grades, {}, &AccessorySocketGrade::grade);
    AccessorySocketGrade previous;
    for (std::size_t i = 0; i < source.grades.size(); ++i)
    {
        const auto& row = source.grades[i];
        if (row.grade != i) return std::unexpected(AccessorySocketError::InvalidGrade);
        if (row.durationMs > static_cast<std::uint64_t>(INT64_MAX) || (i && !row.durationMs))
            return std::unexpected(AccessorySocketError::InvalidTimer);
        if (row.bonusIncreaseBp > 100000 || row.minimumRaw < 0 || row.minimumRaw > MaxItemStatRaw ||
            row.minimumBp < 0 || row.minimumBp > 100000 || row.minimumInteger < 0 || row.minimumInteger > 100000)
            return std::unexpected(AccessorySocketError::InvalidBonus);
        if (!i && (row.durationMs || row.bonusIncreaseBp || row.minimumRaw || row.minimumBp || row.minimumInteger))
            return std::unexpected(AccessorySocketError::NonzeroGradeZero);
        if (row.unlockChanceBp > 10000 || row.chargeChanceBp > 10000 ||
            (i == source.maxGrade && (row.unlockChanceBp || row.chargeChanceBp)))
            return std::unexpected(AccessorySocketError::InvalidChance);
        if (row.bonusIncreaseBp < previous.bonusIncreaseBp || row.minimumRaw < previous.minimumRaw ||
            row.minimumBp < previous.minimumBp || row.minimumInteger < previous.minimumInteger)
            return std::unexpected(AccessorySocketError::NonMonotonicBonus);
        previous = row;
    }
    return ValidatedAccessorySocketProfile(std::move(source));
}
std::expected<void, AccessorySocketError> ValidateAccessorySocketState(
    const ValidatedAccessorySocketProfile& profile, AccessorySocketState state) noexcept
{
    const auto* grade = profile.FindGrade(state.activeGrade);
    if (!profile.FindGrade(0) || !grade || state.unlockedCapacity > profile.Data().maxGrade || state.activeGrade > state.unlockedCapacity)
        return std::unexpected(AccessorySocketError::InvalidState);
    if ((!state.activeGrade && (state.remainingMs || state.permanent)) ||
        (state.permanent && (state.remainingMs || profile.Data().permanentChargeMaterials.empty())) ||
        (state.activeGrade && !state.permanent && (!state.remainingMs || state.remainingMs > grade->durationMs)))
        return std::unexpected(AccessorySocketError::InvalidState);
    return {};
}
std::expected<AccessorySocketPlan, AccessorySocketError> PlanAccessorySocketUnlock(
    const ValidatedAccessorySocketProfile& profile, AccessorySocketState state, ItemKey material, SocketAttemptOutcome outcome) noexcept
{
    if (auto result = ValidateAccessorySocketState(profile, state); !result) return std::unexpected(result.error());
    if (!std::ranges::binary_search(profile.Data().unlockMaterials, material)) return std::unexpected(AccessorySocketError::MaterialNotAllowed);
    if (state.unlockedCapacity == profile.Data().maxGrade) return std::unexpected(AccessorySocketError::CapacityFull);
    if (auto result = Decision(profile.FindGrade(state.unlockedCapacity)->unlockChanceBp, outcome); !result) return std::unexpected(result.error());
    const bool success = outcome == SocketAttemptOutcome::Succeeded;
    if (success) ++state.unlockedCapacity;
    return AccessorySocketPlan{state, profile.Data().unlockUnitsPerAttempt, success};
}
std::expected<AccessorySocketPlan, AccessorySocketError> PlanAccessorySocketCharge(
    const ValidatedAccessorySocketProfile& profile, AccessorySocketState state, ItemKey material, SocketAttemptOutcome outcome) noexcept
{
    if (auto result = ValidateAccessorySocketState(profile, state); !result) return std::unexpected(result.error());
    const bool permanent = std::ranges::binary_search(profile.Data().permanentChargeMaterials, material);
    if (!permanent && !std::ranges::binary_search(profile.Data().chargeMaterials, material))
        return std::unexpected(AccessorySocketError::MaterialNotAllowed);
    if (state.activeGrade == state.unlockedCapacity) return std::unexpected(AccessorySocketError::NoOpenCapacity);
    if (state.activeGrade && state.permanent != permanent) return std::unexpected(AccessorySocketError::LifetimeMismatch);
    if (auto result = Decision(profile.FindGrade(state.activeGrade)->chargeChanceBp, outcome); !result) return std::unexpected(result.error());
    const bool success = outcome == SocketAttemptOutcome::Succeeded;
    if (success)
    {
        ++state.activeGrade;
        state.permanent = permanent;
        state.remainingMs = permanent ? 0 : profile.FindGrade(state.activeGrade)->durationMs;
    }
    return AccessorySocketPlan{state, profile.Data().chargeUnitsPerAttempt, success};
}
std::expected<AccessorySocketTimeResult, AccessorySocketError> AdvanceAccessorySocketTime(
    const ValidatedAccessorySocketProfile& profile, AccessorySocketState state, std::uint64_t elapsedMs, bool equipped) noexcept
{
    if (auto result = ValidateAccessorySocketState(profile, state); !result) return std::unexpected(result.error());
    std::uint16_t expired = 0;
    if (equipped && !state.permanent)
    {
        // At most 255 transitions even for UINT64_MAX elapsed time. Positive
        // validated durations and subtraction avoid timestamp/amount overflow.
        while (state.activeGrade && elapsedMs >= state.remainingMs)
        {
            elapsedMs -= state.remainingMs;
            --state.activeGrade;
            ++expired;
            state.remainingMs = profile.FindGrade(state.activeGrade)->durationMs;
        }
        if (state.activeGrade) state.remainingMs -= elapsedMs;
    }
    return AccessorySocketTimeResult{state, expired};
}
} // namespace auriga::item
