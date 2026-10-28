#include <item/ToggleEffects.hpp>
#include <item/ItemCatalog.hpp>
#include "ContentValidation.hpp"
#include <algorithm>
#include <set>

namespace auriga::item
{
bool ValidToggleEffectGroup(const ToggleEffectGroup& p) noexcept
{
    return p.id && detail::ContentCode(p.code);
}
bool ValidToggleEffectProfile(const ToggleEffectProfile& p) noexcept
{
    return p.id && detail::ContentCode(p.code) && p.group && p.bonusProfile && p.variant &&
        p.durationMs && p.durationMs <= INT64_MAX && p.requirements <= 3 &&
        p.allowedContexts && p.allowedContexts <= 15 && p.cooldownProfile != 0;
}
namespace
{
using E = ToggleEffectError;
std::expected<const ToggleEffectProfile*, E> Profile(const ItemCatalog& c, ItemKey key) noexcept
{
    const auto* item = c.Find(key);
    if (!item) return std::unexpected(E::MissingItem);
    const auto* child = std::get_if<ItemConsumableRow>(&item->category);
    if (!child) return std::unexpected(E::WrongCategory);
    if (!child->toggleEffectProfile) return std::unexpected(E::WrongEffect);
    return c.FindToggleEffect(*child->toggleEffectProfile);
}
bool Valid(const ToggleEffectProfile& p, ItemInstanceId id, const ToggleEffectAdmission& a) noexcept
{
    return id.Value() && a.availableUnits == 1 && a.classIndex <= 3 && a.sexIndex <= 1 &&
        a.state.remainingMs <= p.durationMs && (!a.state.active || a.state.remainingMs) &&
        a.cooldownRemainingMs <= INT64_MAX && (p.cooldownProfile || !a.cooldownRemainingMs) &&
        (a.context == RecoveryContext::World || a.context == RecoveryContext::Duel ||
         a.context == RecoveryContext::Arena || a.context == RecoveryContext::GuildWar);
}
std::optional<E> Access(const ItemCatalog& c, ItemKey key, const ToggleEffectProfile& p,
                       const ToggleEffectAdmission& a, bool activation) noexcept
{
    const auto& base = c.Find(key)->base;
    const auto& use = *c.FindUsePolicy(base.usePolicy);
    if (base.retired) return E::RetiredItem;
    if (!a.effectAllowedByRules) return E::RuleDenied;
    if (!(p.allowedContexts & static_cast<std::uint8_t>(a.context))) return E::ContextDenied;
    if (a.inPvp && !p.allowDuringPvp) return E::PvpDenied;
    if (a.level < base.minimumLevel) return E::LevelDenied;
    if (!(use.allowedClasses & (1u << a.classIndex))) return E::ClassDenied;
    if (!(use.allowedSexes & (1u << a.sexIndex))) return E::SexDenied;
    if (((p.requirements & 1) && !a.married) || ((p.requirements & 2) && !a.partyLeader)) return E::RequirementDenied;
    if (activation && !a.alive) return E::DeadCharacter;
    if (activation && a.inCombat && !use.useDuringCombat) return E::CombatDenied;
    if (activation && a.cooldownRemainingMs) return E::OnCooldown;
    return {};
}
}
std::expected<ToggleEffectState, E> InitialToggleEffectState(const ItemCatalog& c, ItemKey key) noexcept
{
    const auto p = Profile(c, key);
    if (!p) return std::unexpected(p.error());
    if (c.Find(key)->base.retired) return std::unexpected(E::RetiredItem);
    return ToggleEffectState{(*p)->durationMs, false};
}
std::expected<ToggleEffectPlan, E> PlanToggleEffectUse(const ItemCatalog& c, ItemKey key, ItemInstanceId id,
    const ToggleEffectAdmission& a, std::span<const ActiveToggleEffect> active)
{
    const auto p = Profile(c, key);
    if (!p) return std::unexpected(p.error());
    if (!Valid(**p, id, a) || active.size() > MaxActiveToggleEffects) return std::unexpected(E::InvalidState);
    std::set<std::uint64_t> ids;
    std::set<std::uint32_t> groups;
    const ActiveToggleEffect* conflict = nullptr;
    const ToggleEffectProfile* conflictProfile = nullptr;
    bool self = false;
    for (const auto& other : active)
    {
        const auto op = Profile(c, other.key);
        if (!op || !other.item.Value() || !other.remainingMs || other.remainingMs > (*op)->durationMs ||
            !ids.insert(other.item.Value()).second || !groups.insert((*op)->group).second)
            return std::unexpected(E::InvalidState);
        if (other.item == id)
        {
            if (other.key != key || !a.state.active || other.remainingMs != a.state.remainingMs)
                return std::unexpected(E::InvalidState);
            self = true;
        }
        if ((*op)->group == (*p)->group) { conflict = &other; conflictProfile = *op; }
    }
    if (self != a.state.active) return std::unexpected(E::InvalidState);
    ToggleEffectPlan plan{id, a.state};
    if (self) // Cleanup remains possible after death, retirement or loss of requirements.
    {
        plan.next.active = false;
        plan.step = ToggleEffectStep::Deactivate;
        return plan;
    }
    if (const auto error = Access(c, key, **p, a, true)) return std::unexpected(*error);
    if (!a.state.remainingMs) return std::unexpected(E::Exhausted);
    if (conflict && conflictProfile->variant == (*p)->variant) return std::unexpected(E::AlreadyActive);
    if (!conflict && groups.size() == MaxActiveToggleEffects) return std::unexpected(E::ActiveLimit);
    plan.step = ToggleEffectStep::Activate;
    plan.next.active = true;
    if (conflict) plan.deactivateOther = conflict->item;
    if ((*p)->cooldownProfile)
        plan.cooldown = ConsumableCooldownStart{*(*p)->cooldownProfile, c.FindConsumableCooldown(*(*p)->cooldownProfile)->durationMs};
    plan.bonuses = c.FindBonus((*p)->bonusProfile)->Data().entries;
    return plan;
}
std::expected<ToggleEffectPlan, E> PlanToggleEffectAdvance(const ItemCatalog& c, ItemKey key, ItemInstanceId id,
    const ToggleEffectAdmission& a, std::uint64_t elapsedMs)
{
    const auto p = Profile(c, key);
    if (!p) return std::unexpected(p.error());
    if (!Valid(**p, id, a) || elapsedMs > INT64_MAX) return std::unexpected(E::InvalidState);
    ToggleEffectPlan plan{id, a.state};
    if (!a.state.active) return plan;
    // Charge the elapsed valid active interval BEFORE applying the current boundary's
    // death/rule changes. Owner calls at every transition, not after a long stale snapshot.
    plan.next.remainingMs -= std::min(plan.next.remainingMs, elapsedMs);
    plan.step = ToggleEffectStep::Advance;
    if (!plan.next.remainingMs)
    {
        plan.next.active = false;
        plan.step = ToggleEffectStep::Expire;
        plan.destroyItem = true;
    }
    else if (Access(c, key, **p, a, false) || (!a.alive && c.FindToggleEffectGroup((*p)->group)->removeOnDeath))
    {
        plan.next.active = false;
        plan.step = ToggleEffectStep::Deactivate;
    }
    return plan;
}
}
