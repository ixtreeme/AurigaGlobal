#include <item/Consumables.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <type_traits>

namespace auriga::item
{
bool ValidConsumableCooldown(const ConsumableCooldownProfile& source) noexcept
{
    return source.id && detail::ContentCode(source.code) && source.durationMs && source.durationMs <= INT64_MAX;
}
std::expected<ValidatedRecoveryProfile, RecoveryProfileError> ValidateRecoveryProfile(RecoveryProfile source)
{
    if (!source.id || !detail::ContentCode(source.code)) return std::unexpected(RecoveryProfileError::InvalidIdentity);
    if (!source.unitsPerUse) return std::unexpected(RecoveryProfileError::InvalidUnits);
    if (!source.allowedContexts || source.allowedContexts > 15) return std::unexpected(RecoveryProfileError::InvalidContexts);
    if (source.cooldownProfile == 0) return std::unexpected(RecoveryProfileError::InvalidCooldown);
    const auto result = std::visit([](const auto& effect) -> std::expected<void, RecoveryProfileError> {
        using T = std::decay_t<decltype(effect)>;
        const auto hp = [&] { if constexpr (std::is_same_v<T, PeriodicRecovery>) return effect.hpPerTickRaw; else return effect.hpRaw; }();
        const auto sp = [&] { if constexpr (std::is_same_v<T, PeriodicRecovery>) return effect.spPerTickRaw; else return effect.spRaw; }();
        if (hp < 0 || sp < 0 || hp > MaxItemStatRaw || sp > MaxItemStatRaw)
            return std::unexpected(RecoveryProfileError::InvalidAmount);
        if constexpr (std::is_same_v<T, InstantRecovery>)
        {
            if (effect.maxHpBp > 10000 || effect.maxSpBp > 10000 || effect.flatBonusCapBp > 100000)
                return std::unexpected(RecoveryProfileError::InvalidAmount);
            if (!hp && !sp && !effect.maxHpBp && !effect.maxSpBp) return std::unexpected(RecoveryProfileError::EmptyEffect);
        }
        else
        {
            if (!hp && !sp) return std::unexpected(RecoveryProfileError::EmptyEffect);
            if (!effect.tickIntervalMs || effect.tickIntervalMs > INT64_MAX) return std::unexpected(RecoveryProfileError::InvalidInterval);
            if constexpr (std::is_same_v<T, ReserveRecovery>)
            {
                if (effect.flatBonusCapBp > 100000 || !effect.maxPoolPerTickBp || effect.maxPoolPerTickBp > 10000)
                    return std::unexpected(RecoveryProfileError::InvalidAmount);
            }
            else
            {
                if (hp && sp) return std::unexpected(RecoveryProfileError::MultiplePools);
                if (effect.durationMs > INT64_MAX || effect.durationMs < effect.tickIntervalMs || effect.durationMs % effect.tickIntervalMs)
                    return std::unexpected(RecoveryProfileError::InvalidDuration);
            }
        }
        return {};
    }, source.effect);
    if (!result) return std::unexpected(result.error());
    return ValidatedRecoveryProfile(std::move(source));
}
namespace
{
static_assert(MaxItemStatRaw <= INT64_MAX / 12);
// Exact floor(value*bp/10000) for this bounded nonnegative recovery domain only.
// The intermediate quotient product is <= 11*10^15, remainder product < 1.1e9.
// This does not replace the wide signed arithmetic required by CombatKernel.
std::int64_t Scale(std::int64_t value, std::int64_t bp) noexcept
{
    return (value / 10000) * bp + (value % 10000) * bp / 10000;
}
bool Pool(std::int64_t current, std::int64_t maximum) noexcept
{
    return current >= 0 && maximum >= 0 && maximum <= MaxItemStatRaw && current <= maximum;
}
}
std::expected<InstantRecoveryPlan, RecoveryUseError> PlanInstantRecovery(
    const ItemCatalog& catalog, ItemKey key, const InstantRecoveryAdmission& state) noexcept
{
    const auto* item = catalog.Find(key);
    if (!item) return std::unexpected(RecoveryUseError::MissingItem);
    const auto* child = std::get_if<ItemConsumableRow>(&item->category);
    if (!child) return std::unexpected(RecoveryUseError::WrongCategory);
    const auto& profile = catalog.FindRecovery(child->recoveryProfile)->Data();
    const auto* effect = std::get_if<InstantRecovery>(&profile.effect);
    if (!effect) return std::unexpected(RecoveryUseError::WrongDelivery);
    if (item->base.retired) return std::unexpected(RecoveryUseError::RetiredItem);
    const auto& pools = state.pools;
    if (!Pool(pools.hpRaw, pools.maxHpRaw) || !pools.maxHpRaw || !Pool(pools.spRaw, pools.maxSpRaw) ||
        state.potionBonusBp < -10000 || state.potionBonusBp > 100000 || state.classIndex > 3 || state.sexIndex > 1 ||
        (state.context != RecoveryContext::World && state.context != RecoveryContext::Duel &&
         state.context != RecoveryContext::Arena && state.context != RecoveryContext::GuildWar) ||
        (!profile.cooldownProfile && state.cooldownRemainingMs) || state.availableUnits > item->base.maxStack)
        return std::unexpected(RecoveryUseError::InvalidState);
    if (!pools.hpRaw) return std::unexpected(RecoveryUseError::DeadCharacter);
    if (!(profile.allowedContexts & static_cast<std::uint8_t>(state.context))) return std::unexpected(RecoveryUseError::ContextDenied);
    if (state.inPvp && !profile.allowDuringPvp) return std::unexpected(RecoveryUseError::PvpDenied);
    const auto* use = catalog.FindUsePolicy(item->base.usePolicy);
    if (state.inCombat && !use->useDuringCombat) return std::unexpected(RecoveryUseError::CombatDenied);
    if (state.level < item->base.minimumLevel) return std::unexpected(RecoveryUseError::LevelDenied);
    if (!(use->allowedClasses & (1u << state.classIndex))) return std::unexpected(RecoveryUseError::ClassDenied);
    if (!(use->allowedSexes & (1u << state.sexIndex))) return std::unexpected(RecoveryUseError::SexDenied);
    if (state.cooldownRemainingMs) return std::unexpected(RecoveryUseError::OnCooldown);
    if (state.availableUnits < profile.unitsPerUse) return std::unexpected(RecoveryUseError::InsufficientUnits);
    const auto multiplier = 10000 + std::min<std::int64_t>(state.potionBonusBp, effect->flatBonusCapBp);
    const auto hp = std::min(pools.maxHpRaw - pools.hpRaw, Scale(effect->hpRaw, multiplier) + Scale(pools.maxHpRaw, effect->maxHpBp));
    const auto sp = std::min(pools.maxSpRaw - pools.spRaw, Scale(effect->spRaw, multiplier) + Scale(pools.maxSpRaw, effect->maxSpBp));
    if (!hp && !sp) return std::unexpected(RecoveryUseError::NoEffect);
    InstantRecoveryPlan plan {hp, sp, profile.unitsPerUse, {}};
    if (profile.cooldownProfile)
        plan.cooldown = ConsumableCooldownStart{*profile.cooldownProfile, catalog.FindConsumableCooldown(*profile.cooldownProfile)->durationMs};
    return plan;
}
} // namespace auriga::item
