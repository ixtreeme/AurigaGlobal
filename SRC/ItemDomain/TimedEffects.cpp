#include <item/TimedEffects.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <set>

namespace auriga::item
{
bool ValidTimedEffectGroup(const TimedEffectGroup& source) noexcept
{
    return source.id && detail::ContentCode(source.code) &&
        (source.reapply == TimedEffectReapply::Reject || source.reapply == TimedEffectReapply::Replace) &&
        (source.clock == TimedEffectClock::OnlineTime || source.clock == TimedEffectClock::WallTime);
}
bool ValidTimedEffectProfile(const TimedEffectProfile& source) noexcept
{
    return source.id && detail::ContentCode(source.code) && source.group && source.bonusProfile &&
        source.durationMs && source.durationMs <= INT64_MAX && source.unitsPerUse &&
        source.allowedContexts && source.allowedContexts <= 15 && source.cooldownProfile != 0;
}
std::expected<TimedEffectPlan, TimedEffectUseError> PlanTimedEffectUse(
    const ItemCatalog& catalog, ItemKey key, const TimedEffectAdmission& state,
    std::span<const ActiveTimedEffect> active)
{
    using E = TimedEffectUseError;
    const auto* item = catalog.Find(key);
    if (!item) return std::unexpected(E::MissingItem);
    const auto* child = std::get_if<ItemConsumableRow>(&item->category);
    if (!child) return std::unexpected(E::WrongCategory);
    if (!child->timedEffectProfile) return std::unexpected(E::WrongEffect);
    const auto& profile = *catalog.FindTimedEffect(*child->timedEffectProfile);
    const auto& group = *catalog.FindTimedEffectGroup(profile.group);
    if (item->base.retired) return std::unexpected(E::RetiredItem);
    if (state.classIndex > 3 || state.sexIndex > 1 || state.availableUnits > item->base.maxStack ||
        state.cooldownRemainingMs > INT64_MAX || (!profile.cooldownProfile && state.cooldownRemainingMs) ||
        (state.context != RecoveryContext::World && state.context != RecoveryContext::Duel &&
         state.context != RecoveryContext::Arena && state.context != RecoveryContext::GuildWar) ||
        active.size() > MaxActiveTimedEffects)
        return std::unexpected(E::InvalidState);
    if (!state.alive) return std::unexpected(E::DeadCharacter);
    if (!(profile.allowedContexts & static_cast<std::uint8_t>(state.context))) return std::unexpected(E::ContextDenied);
    if (state.inPvp && !profile.allowDuringPvp) return std::unexpected(E::PvpDenied);
    const auto& use = *catalog.FindUsePolicy(item->base.usePolicy);
    if (state.inCombat && !use.useDuringCombat) return std::unexpected(E::CombatDenied);
    if (state.level < item->base.minimumLevel) return std::unexpected(E::LevelDenied);
    if (!(use.allowedClasses & (1u << state.classIndex))) return std::unexpected(E::ClassDenied);
    if (!(use.allowedSexes & (1u << state.sexIndex))) return std::unexpected(E::SexDenied);
    if (state.cooldownRemainingMs) return std::unexpected(E::OnCooldown);
    if (state.availableUnits < profile.unitsPerUse) return std::unexpected(E::InsufficientUnits);
    std::set<std::uint64_t> ids;
    std::set<std::uint32_t> groups;
    std::optional<std::uint64_t> replace;
    for (const auto& effect : active)
    {
        if (!effect.effectId || !catalog.FindTimedEffectGroup(effect.group) || effect.remainingMs > INT64_MAX ||
            !ids.insert(effect.effectId).second)
            return std::unexpected(E::InvalidState);
        if (!effect.remainingMs) continue;
        if (!groups.insert(effect.group).second) return std::unexpected(E::InvalidState);
        if (effect.group == profile.group) replace = effect.effectId;
    }
    if (replace && group.reapply == TimedEffectReapply::Reject) return std::unexpected(E::AlreadyActive);
    if (!replace && groups.size() == MaxActiveTimedEffects) return std::unexpected(E::ActiveLimit);
    TimedEffectPlan plan{profile.id, profile.group, profile.bonusProfile, profile.durationMs,
        group.clock, group.removeOnDeath, profile.unitsPerUse, replace, {},
        catalog.FindBonus(profile.bonusProfile)->Data().entries};
    if (profile.cooldownProfile)
        plan.cooldown = ConsumableCooldownStart{*profile.cooldownProfile, catalog.FindConsumableCooldown(*profile.cooldownProfile)->durationMs};
    return plan;
}
} // namespace auriga::item
