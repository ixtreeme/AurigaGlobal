#include <item/RefinementProfiles.hpp>
#include "ContentValidation.hpp"
#include <algorithm>

namespace auriga::item
{
std::expected<ValidatedRefinementProfile, RefinementError> ValidateRefinementProfile(
    RefinementProfile source, std::span<const ValidatedBonusProfile> bonuses)
{
    if (!source.id || !detail::ContentCode(source.code)) return std::unexpected(RefinementError::InvalidIdentity);
    if (source.kind != RefinementKind::None && source.kind != RefinementKind::Weapon &&
        source.kind != RefinementKind::Armor && source.kind != RefinementKind::Accessory)
        return std::unexpected(RefinementError::InvalidKind);
    if (source.maxLevel > 255 || (source.kind == RefinementKind::None && source.maxLevel != 0))
        return std::unexpected(RefinementError::InvalidLevelRange);
    if (source.levels.size() != std::size_t(source.maxLevel) + 1)
        return std::unexpected(RefinementError::IncompleteLevels);
    if (bonuses.size() > MaxBonusProfiles) return std::unexpected(RefinementError::InvalidBonusContext);
    std::vector<const ValidatedBonusProfile*> bonusIndex;
    bonusIndex.reserve(bonuses.size());
    for (const auto& profile : bonuses)
    {
        if (!profile.Data().id || profile.Data().entries.empty()) return std::unexpected(RefinementError::InvalidBonusContext);
        bonusIndex.push_back(&profile);
    }
    const auto id = [](const auto* profile) { return profile->Data().id; };
    std::ranges::sort(bonusIndex, {}, id);
    for (std::size_t i = 1; i < bonusIndex.size(); ++i)
        if (id(bonusIndex[i-1]) == id(bonusIndex[i])) return std::unexpected(RefinementError::InvalidBonusContext);
    std::ranges::sort(source.levels, {}, &RefinementLevel::level);
    ItemBaseStats previous;
    for (std::size_t i = 0; i < source.levels.size(); ++i)
    {
        const auto& step = source.levels[i];
        if (step.level != i) return std::unexpected(RefinementError::IncompleteLevels);
        for (auto value : step.gain.Values())
            if (value < 0 || value > MaxItemStatRaw) return std::unexpected(RefinementError::InvalidGain);
        if (i == 0 && (step.gain != ItemBaseStats{} || step.extraBonusProfile))
            return std::unexpected(RefinementError::NonzeroLevelZero);
        if (step.extraBonusProfile)
        {
            const auto found = std::ranges::lower_bound(bonusIndex, *step.extraBonusProfile, {}, id);
            if (found == bonusIndex.end() || id(*found) != *step.extraBonusProfile)
                return std::unexpected(RefinementError::MissingBonusProfile);
            if (!AcceptsBonusSource(**found, BonusSource::Refinement)) return std::unexpected(RefinementError::WrongBonusSource);
        }
        if ((source.kind != RefinementKind::Weapon && (step.gain.physicalAttackRaw || step.gain.magicalAttackRaw)) ||
            (source.kind != RefinementKind::Armor && (step.gain.physicalDefenseRaw || step.gain.magicalDefenseRaw)))
            return std::unexpected(RefinementError::WrongStatKind);
        const auto now = step.gain.Values();
        const auto before = previous.Values();
        for (std::size_t stat = 0; stat < now.size(); ++stat)
            if (now[stat] < before[stat]) return std::unexpected(RefinementError::NonMonotonicGain);
        previous = step.gain;
    }
    return ValidatedRefinementProfile(std::move(source));
}

std::expected<ItemBaseStats, RefinementError> ResolveRefinedBaseStats(
    const ItemBaseStats& base, const ValidatedRefinementProfile& profile, std::uint16_t level) noexcept
{
    const auto* step = profile.FindLevel(level);
    if (!step) return std::unexpected(RefinementError::InvalidLevelRange);
    if ((profile.Data().kind == RefinementKind::Weapon && (base.physicalDefenseRaw || base.magicalDefenseRaw)) ||
        (profile.Data().kind == RefinementKind::Armor && (base.physicalAttackRaw || base.magicalAttackRaw)) ||
        (profile.Data().kind == RefinementKind::Accessory && base != ItemBaseStats{}))
        return std::unexpected(RefinementError::WrongStatKind);
    const auto bases = base.Values();
    const auto gains = step->gain.Values();
    std::array<std::int64_t, 4> totals {};
    for (std::size_t i = 0; i < totals.size(); ++i)
    {
        // Both operands are validated before adding: no native overflow and no
        // numerical saturation. Combat multiplication is a separate kernel job.
        if (bases[i] < 0 || bases[i] > MaxItemStatRaw || gains[i] > MaxItemStatRaw - bases[i])
            return std::unexpected(RefinementError::StatOutOfRange);
        totals[i] = bases[i] + gains[i];
    }
    return ItemBaseStats{totals[0], totals[1], totals[2], totals[3]};
}

std::expected<std::uint16_t, RefinementError> NextRefinementLevel(
    const ValidatedRefinementProfile& profile, std::uint16_t currentLevel) noexcept
{
    if (!profile.FindLevel(currentLevel)) return std::unexpected(RefinementError::InvalidLevelRange);
    if (currentLevel == profile.Data().maxLevel) return std::unexpected(RefinementError::AtMaximumLevel);
    // The configured maximum was validated before publication. No +9 test,
    // decimal digit arithmetic or refined-vnum lookup belongs here.
    const auto next = static_cast<std::uint16_t>(currentLevel + 1u);
    if (!profile.FindLevel(next)) return std::unexpected(RefinementError::InvalidLevelRange);
    return next;
}
} // namespace auriga::item
