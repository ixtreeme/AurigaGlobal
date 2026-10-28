#include <item/Consumables.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include "RecoveryMath.hpp"
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
    if (source.arenaQuotaCost && !(source.allowedContexts & static_cast<std::uint8_t>(RecoveryContext::Arena)))
        return std::unexpected(RecoveryProfileError::InvalidArenaQuota);
    if (std::holds_alternative<AutomaticRecovery>(source.effect) &&
        (source.unitsPerUse != 1 || (source.allowedContexts & static_cast<std::uint8_t>(RecoveryContext::Arena)) || source.arenaQuotaCost))
        return std::unexpected(RecoveryProfileError::InvalidAutomatic);
    const auto result = std::visit([](const auto& effect) -> std::expected<void, RecoveryProfileError> {
        using T = std::decay_t<decltype(effect)>;
        if constexpr (std::is_same_v<T, AutomaticRecovery>)
        {
            if ((effect.pool != RecoveryPool::Hp && effect.pool != RecoveryPool::Sp) ||
                (effect.supply != RecoverySupply::Finite && effect.supply != RecoverySupply::Unlimited) ||
                (effect.supply == RecoverySupply::Finite ? effect.capacityRaw <= 0 || effect.capacityRaw > MaxItemStatRaw : effect.capacityRaw != 0) ||
                !effect.tickIntervalMs || effect.tickIntervalMs > INT64_MAX || !effect.maxPoolPerTickBp ||
                effect.maxPoolPerTickBp > 10000 || effect.activeMaxPoolBonusBp > 10000)
                return std::unexpected(RecoveryProfileError::InvalidAutomatic);
        }
        else
        {
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
        }
        return {};
    }, source.effect);
    if (!result) return std::unexpected(result.error());
    return ValidatedRecoveryProfile(std::move(source));
}
std::expected<std::uint16_t, RecoveryUseError> PlanRecoveryArena(
    RecoveryContext context, const std::optional<RecoveryArenaAdmission>& state, std::uint16_t cost) noexcept
{
    if (context != RecoveryContext::World && context != RecoveryContext::Duel &&
        context != RecoveryContext::Arena && context != RecoveryContext::GuildWar)
        return std::unexpected(RecoveryUseError::InvalidState);
    if (context != RecoveryContext::Arena)
    {
        if (state) return std::unexpected(RecoveryUseError::InvalidState);
        return 0;
    }
    if (!state) return std::unexpected(RecoveryUseError::InvalidState);
    if (!state->potionsAllowed) return std::unexpected(RecoveryUseError::ArenaDenied);
    if (state->remainingUses && *state->remainingUses < cost) return std::unexpected(RecoveryUseError::ArenaQuotaExceeded);
    return state->remainingUses ? cost : 0;
}
std::expected<InstantRecoveryPlan, RecoveryUseError> PlanInstantRecovery(
    const ItemCatalog& catalog, ItemKey key, const InstantRecoveryAdmission& state) noexcept
{
    const auto* item = catalog.Find(key);
    if (!item) return std::unexpected(RecoveryUseError::MissingItem);
    const auto* child = std::get_if<ItemConsumableRow>(&item->category);
    if (!child) return std::unexpected(RecoveryUseError::WrongCategory);
    if (!child->recoveryProfile) return std::unexpected(RecoveryUseError::WrongDelivery);
    const auto& profile = catalog.FindRecovery(child->recoveryProfile)->Data();
    const auto* effect = std::get_if<InstantRecovery>(&profile.effect);
    if (!effect) return std::unexpected(RecoveryUseError::WrongDelivery);
    if (item->base.retired) return std::unexpected(RecoveryUseError::RetiredItem);
    const auto& pools = state.pools;
    if (!detail::ValidRecoveryPool(pools.hpRaw, pools.maxHpRaw) || !pools.maxHpRaw || !detail::ValidRecoveryPool(pools.spRaw, pools.maxSpRaw) ||
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
    const auto arena = PlanRecoveryArena(state.context, state.arena, profile.arenaQuotaCost);
    if (!arena) return std::unexpected(arena.error());
    const auto multiplier = 10000 + std::min<std::int64_t>(state.potionBonusBp, effect->flatBonusCapBp);
    const auto hp = std::min(pools.maxHpRaw - pools.hpRaw, detail::ScaleRecovery(effect->hpRaw, multiplier) + detail::ScaleRecovery(pools.maxHpRaw, effect->maxHpBp));
    const auto sp = std::min(pools.maxSpRaw - pools.spRaw, detail::ScaleRecovery(effect->spRaw, multiplier) + detail::ScaleRecovery(pools.maxSpRaw, effect->maxSpBp));
    if (!hp && !sp) return std::unexpected(RecoveryUseError::NoEffect);
    InstantRecoveryPlan plan {hp, sp, profile.unitsPerUse, {}, *arena};
    if (profile.cooldownProfile)
        plan.cooldown = ConsumableCooldownStart{*profile.cooldownProfile, catalog.FindConsumableCooldown(*profile.cooldownProfile)->durationMs};
    return plan;
}
} // namespace auriga::item
