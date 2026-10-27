#include <item/BonusProfiles.hpp>
#include "ContentValidation.hpp"
#include <algorithm>

namespace auriga::item
{
std::expected<ValidatedBonusProfile, BonusProfileError> ValidateBonusProfile(BonusProfile source)
{
    if (!source.id || !detail::ContentCode(source.code)) return std::unexpected(BonusProfileError::InvalidIdentity);
    if (source.source != BonusSource::ItemBase && source.source != BonusSource::Refinement)
        return std::unexpected(BonusProfileError::InvalidSource);
    if (source.entries.empty()) return std::unexpected(BonusProfileError::EmptyProfile);
    if (source.entries.size() > MaxBonusEntriesPerProfile) return std::unexpected(BonusProfileError::TooManyEntries);
    std::ranges::sort(source.entries, {}, &BonusEntry::bonus);
    std::uint32_t previous = 0;
    for (const auto& row : source.entries)
    {
        const auto id = static_cast<std::uint32_t>(row.bonus);
        const auto* definition = FindBonusDefinition(id);
        if (!definition) return std::unexpected(BonusProfileError::UnknownBonus);
        if (id == previous) return std::unexpected(BonusProfileError::DuplicateBonus);
        if (!AllowsBonusSource(*definition, source.source)) return std::unexpected(BonusProfileError::SourceNotAllowed);
        if (row.valueScaled < definition->minimum || row.valueScaled > definition->maximum)
            return std::unexpected(BonusProfileError::ValueOutOfRange);
        previous = id;
    }
    return ValidatedBonusProfile(std::move(source));
}
bool AcceptsBonusSource(const ValidatedBonusProfile& profile, BonusSource source) noexcept
{
    // Moved-from validated objects must not authorize a reference to empty data.
    return profile.Data().id && !profile.Data().entries.empty() && profile.Data().source == source;
}
const ValidatedBonusProfile* FindBonusProfile(std::span<const ValidatedBonusProfile> profiles, std::uint32_t id) noexcept
{
    const ValidatedBonusProfile* found = nullptr;
    for (const auto& profile : profiles)
        if (profile.Data().id == id)
        {
            if (found || profile.Data().entries.empty()) return nullptr;
            found = &profile;
        }
    return found;
}
} // namespace auriga::item
