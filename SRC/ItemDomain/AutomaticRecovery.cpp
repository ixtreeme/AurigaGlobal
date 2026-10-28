#include <item/AutomaticRecovery.hpp>
#include <item/ItemCatalog.hpp>
#include "RecoveryMath.hpp"
#include <algorithm>

namespace auriga::item
{
namespace
{
using E = AutomaticRecoveryError;
struct Content { const ItemCatalogEntry* item; const RecoveryProfile* profile; const AutomaticRecovery* effect; };
std::expected<Content, E> Find(const ItemCatalog& catalog, ItemKey key) noexcept
{
    const auto* item = catalog.Find(key);
    if (!item) return std::unexpected(E::MissingItem);
    const auto* child = std::get_if<ItemConsumableRow>(&item->category);
    if (!child) return std::unexpected(E::WrongCategory);
    if (!child->recoveryProfile) return std::unexpected(E::WrongDelivery);
    const auto& profile = catalog.FindRecovery(child->recoveryProfile)->Data();
    const auto* effect = std::get_if<AutomaticRecovery>(&profile.effect);
    if (!effect) return std::unexpected(E::WrongDelivery);
    return Content{item, &profile, effect};
}
bool Valid(const Content& content, ItemInstanceId id, const AutomaticRecoveryAdmission& s) noexcept
{
    const auto& e = *content.effect;
    return detail::ValidRecoveryPool(s.pools.hpRaw, s.pools.maxHpRaw) && s.pools.maxHpRaw > 0 &&
        detail::ValidRecoveryPool(s.pools.spRaw, s.pools.maxSpRaw) && s.availableUnits == 1 &&
        s.classIndex <= 3 && s.sexIndex <= 1 && s.cooldownRemainingMs <= INT64_MAX &&
        (content.profile->cooldownProfile || s.cooldownRemainingMs == 0) &&
        (s.context == RecoveryContext::World || s.context == RecoveryContext::Duel ||
         s.context == RecoveryContext::Arena || s.context == RecoveryContext::GuildWar) &&
        (e.supply == RecoverySupply::Finite ? s.state.remainingRaw >= 0 && s.state.remainingRaw <= e.capacityRaw : s.state.remainingRaw == 0) &&
        (s.state.active ? s.state.nextTickInMs > 0 && s.state.nextTickInMs <= e.tickIntervalMs && s.activeForPool == id
                        : s.state.nextTickInMs == 0 && s.activeForPool != id);
}
std::optional<E> Access(const ItemCatalog& catalog, const Content& c, const AutomaticRecoveryAdmission& s) noexcept
{
    if (c.item->base.retired) return E::RetiredItem;
    if (!(c.profile->allowedContexts & static_cast<std::uint8_t>(s.context))) return E::ContextDenied;
    if (!s.recoveryAllowedByRules) return E::RuleDenied;
    if (s.inPvp && !c.profile->allowDuringPvp) return E::PvpDenied;
    const auto& use = *catalog.FindUsePolicy(c.item->base.usePolicy);
    if (s.inCombat && !use.useDuringCombat) return E::CombatDenied;
    if (s.level < c.item->base.minimumLevel) return E::LevelDenied;
    if (!(use.allowedClasses & (1u << s.classIndex))) return E::ClassDenied;
    if (!(use.allowedSexes & (1u << s.sexIndex))) return E::SexDenied;
    return {};
}
}
std::expected<AutomaticRecoveryState, AutomaticRecoveryError> InitialAutomaticRecoveryState(
    const ItemCatalog& catalog, ItemKey key) noexcept
{
    const auto c = Find(catalog, key);
    if (!c) return std::unexpected(c.error());
    if (c->item->base.retired) return std::unexpected(E::RetiredItem);
    return AutomaticRecoveryState{c->effect->capacityRaw, false, 0};
}
std::expected<AutomaticRecoveryTogglePlan, AutomaticRecoveryError> PlanAutomaticRecoveryToggle(
    const ItemCatalog& catalog, ItemKey key, ItemInstanceId id, const AutomaticRecoveryAdmission& s) noexcept
{
    const auto c = Find(catalog, key);
    if (!c) return std::unexpected(c.error());
    if (!Valid(*c, id, s)) return std::unexpected(E::InvalidState);
    AutomaticRecoveryTogglePlan plan{id, c->effect->pool, {s.state.remainingRaw, false, 0}, {}, {}, 0};
    // Turning off must not trap a locked bottle in a forbidden map/dead state.
    if (s.state.active) return plan;
    if (const auto denied = Access(catalog, *c, s)) return std::unexpected(*denied);
    if (!s.pools.hpRaw) return std::unexpected(E::DeadCharacter);
    if (s.stunned) return std::unexpected(E::Stunned);
    if (s.cooldownRemainingMs) return std::unexpected(E::OnCooldown);
    if (c->effect->supply == RecoverySupply::Finite && !s.state.remainingRaw) return std::unexpected(E::Exhausted);
    plan.next = {s.state.remainingRaw, true, c->effect->tickIntervalMs};
    plan.deactivateOther = s.activeForPool;
    plan.activeMaxPoolBonusBp = c->effect->activeMaxPoolBonusBp;
    if (c->profile->cooldownProfile)
        plan.cooldown = ConsumableCooldownStart{*c->profile->cooldownProfile,
            catalog.FindConsumableCooldown(*c->profile->cooldownProfile)->durationMs};
    return plan;
}
std::expected<AutomaticRecoveryTickPlan, AutomaticRecoveryError> PlanAutomaticRecoveryTick(
    const ItemCatalog& catalog, ItemKey key, ItemInstanceId id, const AutomaticRecoveryTickAdmission& input) noexcept
{
    const auto c = Find(catalog, key);
    if (!c) return std::unexpected(c.error());
    const auto& s = input.actor;
    if (!Valid(*c, id, s) || input.elapsedMs > INT64_MAX || input.pendingHpRaw < 0 || input.pendingSpRaw < 0 ||
        input.pendingHpRaw > MaxItemStatRaw || input.pendingSpRaw > MaxItemStatRaw)
        return std::unexpected(E::InvalidState);
    AutomaticRecoveryTickPlan plan{id, c->effect->pool, s.state};
    if (!s.state.active) return plan;
    if (Access(catalog, *c, s))
    {
        plan.next.active = false; plan.next.nextTickInMs = 0; plan.step = AutomaticRecoveryStep::Deactivate;
        return plan;
    }
    if (c->effect->supply == RecoverySupply::Finite && !plan.next.remainingRaw)
    {
        plan.next.active = false; plan.next.nextTickInMs = 0;
        plan.step = AutomaticRecoveryStep::Depleted; plan.destroyItem = true; return plan;
    }
    if (input.elapsedMs < s.state.nextTickInMs)
    {
        plan.next.nextTickInMs -= input.elapsedMs; return plan;
    }
    // Missed ticks are discarded, including while stunned/dead. Never catch up
    // a long disconnect with a burst of healing or repeated charge spending.
    plan.next.nextTickInMs = c->effect->tickIntervalMs;
    if (!s.pools.hpRaw || s.stunned) return plan;
    const bool hp = c->effect->pool == RecoveryPool::Hp;
    const auto maximum = hp ? s.pools.maxHpRaw : s.pools.maxSpRaw;
    const auto missing = maximum - (hp ? s.pools.hpRaw : s.pools.spRaw);
    const auto pending = hp ? input.pendingHpRaw : input.pendingSpRaw;
    const auto free = missing - std::min(missing, pending);
    auto restored = std::min(free, detail::ScaleRecovery(maximum, c->effect->maxPoolPerTickBp));
    if (c->effect->supply == RecoverySupply::Finite) restored = std::min(restored, s.state.remainingRaw);
    if (!restored) return plan;
    plan.step = AutomaticRecoveryStep::Recover;
    if (hp) plan.hpRestoredRaw = restored; else plan.spRestoredRaw = restored;
    if (c->effect->supply == RecoverySupply::Finite)
    {
        plan.next.remainingRaw -= restored;
        if (!plan.next.remainingRaw)
        {
            plan.next.active = false; plan.next.nextTickInMs = 0;
            plan.step = AutomaticRecoveryStep::Depleted; plan.destroyItem = true;
        }
    }
    return plan;
}
}
